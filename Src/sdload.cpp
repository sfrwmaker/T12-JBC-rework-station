/*
 * sdload.cpp
 *
 * Sep 03 2023
 *    Added methods to load/save configuration data files
 *    Rewrite the most methods
 * Sep 05 2023
 *    Changed the file name type from std::string to const char *
 *    Modified the SDLOAD::haveToUpdate() and SDLOAD::copyFile()
 *
 */

#include "sdload.h"
#include "jsoncfg.h"

t_msg_id SDLOAD::loadNLS(void) {
	t_msg_id e = startNLS();
	if (MSG_LAST != e)
		return e;
	if (!allocateCopyBuffer())
		return MSG_SD_MEMORY;
	uint8_t l = copyLanguageData();
	umountAll();
	if (buffer) {											// Deallocate copy buffer memory
		free(buffer);
		buffer_size = 0;
	}
	return (l>0)?MSG_LAST:MSG_SD_INCONSISTENT;
}

t_msg_id SDLOAD::loadCfg(HW* core) {
	if (FR_OK != f_mount(&flashfs, "0:/", 1)) {
		return MSG_EEPROM_WRITE;
	}
	if (FR_OK != f_mount(&sdfs, "1:/", 1)) {
		f_mount(NULL, "0:/", 0);
		return MSG_SD_MOUNT;
	}
	if (!allocateCopyBuffer())
		return MSG_SD_MEMORY;
	for (uint8_t f = 0; f < 20; ++f) {						// Load up-to 20 files, actually there are 5 file only
		const TCHAR *fn = core->cfg.fileName(f);			// Next configuration file name
		if (!fn) break;
		copyFile(fn, true);									// Copy file from SD-CARD to the FLASH
	}
	umountAll();
	if (buffer) {											// Deallocate copy buffer memory
		free(buffer);
		buffer_size = 0;
	}
	return MSG_LAST;										// No Error Detected
}

t_msg_id SDLOAD::saveCfg(HW *core) {
	if (FR_OK != f_mount(&flashfs, "0:/", 1)) {
		return MSG_EEPROM_WRITE;
	}
	if (FR_OK != f_mount(&sdfs, "1:/", 1)) {
		f_mount(NULL, "0:/", 0);
		return MSG_SD_MOUNT;
	}
	if (!allocateCopyBuffer())
		return MSG_SD_MEMORY;
	for (uint8_t f = 0; f < 20; ++f) {						// Load up-to 20 files, actually there are 5 file only
		const TCHAR *fn = core->cfg.fileName(f);			// Next configuration file name
		if (!fn) break;
		copyFile(fn, false);								// Copy file from FLASH to the SD-CARD
	}
	umountAll();
	if (buffer) {											// Deallocate copy buffer memory
		free(buffer);
		buffer_size = 0;
	}
	return MSG_LAST;										// No Error Detected
}

t_msg_id SDLOAD::startNLS(void) {
	if (FR_OK != f_mount(&sdfs, "1:/", 1))
		return MSG_SD_MOUNT;
	std::string cfg_path = "1:" + std::string(fn_cfg);
	if (FR_OK != f_open(&cfg_f, cfg_path.c_str(), FA_READ)) {
		f_mount(NULL, "1:/", 0);
		return MSG_SD_NO_CFG;
	}
	lang_cfg.readConfig(&cfg_f);
	lang_list = lang_cfg.getLangList();
	if (lang_list->empty()) {
		f_mount(NULL, "1:/", 0);
		return MSG_SD_NO_LANG;
	}

	if (FR_OK != f_mount(&flashfs, "0:/", 1)) {
		f_mount(NULL, "1:/", 0);
		return MSG_EEPROM_WRITE;
	}
	return MSG_LAST;										// Everything OK, No error message!
}

void SDLOAD::umountAll(void) {
	f_mount(NULL, "0:/", 0);
	f_mount(NULL, "1:/", 0);
}

bool SDLOAD::allocateCopyBuffer(void) {
	for (uint8_t i = 0; i < 3; ++i) {
		buffer = (uint8_t *)malloc(b_sizes[i]);
		if (buffer) {										// Successfully allocated
			buffer_size = b_sizes[i];
			return true;
		}
	}
	return false;
}

uint8_t SDLOAD::copyLanguageData(void) {
	uint8_t l_copied = 0;
	while (!lang_list->empty()) {
		t_lang_cfg lang = lang_list->back();
		lang_list->pop_back();
		bool lang_ok = isLanguageDataConsistent(lang);		// Check the language files exist
		if (lang_ok)
			lang_ok = copyFile(lang.messages_file.c_str(), true);
		if (lang_ok)
			lang_ok = copyFile(lang.font_file.c_str(), true);
		if (lang_ok)
			++l_copied;
	}
	if (l_copied > 0) {
		std::string cfg_name = fn_cfg;
		if (copyFile(cfg_name.c_str(), true))
			return l_copied;
	}
	return 0;
}

bool SDLOAD::isLanguageDataConsistent(t_lang_cfg &lang_data) {
	std::string file_path = "1:" + lang_data.messages_file;
	FILINFO fno;
	// Checking message file
	if (FR_OK != f_stat(file_path.c_str(), &fno))
		return false;
	if (fno.fsize == 0 || (fno.fattrib & AM_ARC) == 0)
		return false;
	// Checking font file
	file_path = "1:" + lang_data.font_file;
	if (FR_OK != f_stat(file_path.c_str(), &fno))
		return false;
	if (fno.fsize == 0 || (fno.fattrib & AM_ARC) == 0)
		return false;
	return true;
}

bool SDLOAD::haveToUpdate(const char *name) {
	FILINFO fno;
	uint8_t fn_len = strlen(name);
	char fn[fn_len + 2];
	strcpy(&fn[2], name);									// Source file path on SD-CARD
	fn[0] = '1';
	fn[1] = ':';
	if (FR_OK != f_stat(fn, &fno))			// Failed to get info of the new file
		return true;
	uint32_t source_date = fno.fdate << 16 | fno.ftime;		// The source file timestamp
	fn[0] = '0';											// Destination file path on SPI FLASH
	if (FR_OK != f_stat(fn, &fno))							// Failed to get info of the file, perhaps, the destination file does not exist
		return true;
	if (fno.fsize == 0 || (fno.fattrib & AM_ARC) == 0)		// Destination file size is zero or is not a n archive file at all
		return false;
	uint32_t dest_date = fno.fdate << 16 | fno.ftime;		// The destination file timestamp
	return (dest_date < source_date);						// If the destination file timestamp is older than source one
}

bool SDLOAD::copyFile(const char *name, bool load) {
	FIL	sf, df;												// Source and destination file descriptors
	if (load && !haveToUpdate(name)) return true;			// The file already exists on the SPI FLASH and its date is not older than the source file on SD-CARD
	if (!buffer || buffer_size == 0)						// Here the copy buffer has to be allocated already, but double check it
		return false;
	uint8_t fn_len = strlen(name);
	char fn[fn_len + 2];
	strcpy(&fn[2], name);
	fn[0] = load?'1':'0';
	fn[1] = ':';

	if (FR_OK != f_open(&sf, fn, FA_READ))					// Failed to open source file for reading
		return false;
	FILINFO fno;
	bool ts_ok =  (FR_OK == f_stat(fn, &fno));				// The file timestamp is OK

	fn[0] = load?'0':'1';
	if (FR_OK != f_open(&df, fn, FA_CREATE_ALWAYS | FA_WRITE)) // Failed to create destination file for writing
		return false;
	bool copied = true;
	while (true) {											// The file copy loop
		UINT br = 0;										// Read bytes
		f_read(&sf, (void *)buffer, (UINT)buffer_size, &br);
		if (br == 0)										// End of source file
			break;
		UINT written = 0;									// Written bytes
		f_write(&df, (void *)buffer, br, &written);
		if (written != br) {
			copied = false;
			break;
		}
	}
	f_close(&df);
	f_close(&sf);
	if (!copied) {
		f_unlink(fn);										// Remove destination file in case on any error
	} else {												// Copy date and time from the source file to the destination file
		if (ts_ok) {										// The source file timestamp was ok
			f_utime(fn, &fno);
		}
	}
	return copied;
}
