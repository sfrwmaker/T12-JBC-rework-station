/*
 * sdload.h
 *
 * Sep 03 2023
 *    Added methods to load/save configuration data files
 *    Rewrite the most methods
 * Sep 05 2023
 *    Changed the argument type in haveToUpdate() from std::string to const char *
 *    Changed the argument type in copyFile() from std::string to const char *
 *
 */

#ifndef SDLOAD_H_
#define SDLOAD_H_

#include "jsoncfg.h"
#include "ff.h"
#include "vars.h"
#include "sdspi.h"
#include "hw.h"

extern SDCARD sd;

class SDLOAD {
	public:
		SDLOAD(void)										{ }
		t_msg_id	loadNLS(void);							// Returns message code
		t_msg_id	loadCfg(HW *core);						// Load configuration files from SD-CARD
		t_msg_id	saveCfg(HW *core);						// Save configuration files to SD-CARD
		uint8_t		sdStatus(void)							{ return sd.init_status; } // SD status initialized by SD_Init() function (see sdspi.c)
	private:
		t_msg_id	startNLS(void);
		uint8_t		copyLanguageData(void);
		void		umountAll(void);
		bool		allocateCopyBuffer(void);
		bool		isLanguageDataConsistent(t_lang_cfg &lang_data);
		bool		haveToUpdate(const char *name);
		bool		copyFile(const char *name, bool load = true);
		uint8_t			*buffer		= 0;					// The buffer to copy the file, allocated later
		uint16_t		buffer_size	= 0;					// The allocated buffer size
		FATFS			sdfs, flashfs;
		FIL				cfg_f;
		JSON_LANG_CFG	lang_cfg;
		t_lang_list		*lang_list = 0;
		const TCHAR*	fn_cfg			= nsl_cfg;			// vars.h
		const uint16_t	b_sizes[3] = { 4096, 1024, 512};	// Possible copy buffer sizes
};

#endif
