/*
 * nls.h
 *
 * Sep 03 2023
 * 	Added Configuration manage menu
 *
 */

#ifndef MSG_NLS_H_
#define MSG_NLS_H_

#include <string>

typedef enum e_msg { MSG_MENU_MAIN, MSG_MENU_SETUP = 10, MSG_MENU_T12 = 10+12, MSG_MENU_JBC = 10+12+11, MSG_MENU_CALIB = 10+12+11+6, MSG_PID_MENU = 10+12+11+6+5, MSG_FLASH_MENU = 10+12+11+6+5+5,
					MSG_ON = 10+12+11+6+5+5+5, MSG_OFF, MSG_FAN, MSG_PWR,
					MSG_REF_POINT, MSG_REED, MSG_TILT, MSG_DEG, MSG_MINUTES, MSG_SECONDS,
					MSG_CW, MSG_CCW, MSG_SET, MSG_ERROR, MSG_TUNE_PID, MSG_SELECT_TIP,
					MSG_EEPROM_READ, MSG_EEPROM_WRITE, MSG_EEPROM_DIRECTORY, MSG_FORMAT_EEPROM, MSG_FORMAT_FAILED,
					MSG_SAVE_ERROR, MSG_HOT_AIR_GUN, MSG_T12_IRON, MSG_JBC_IRON, MSG_SAVE_Q, MSG_YES, MSG_NO, MSG_DELETE_FILE, MSG_FLASH_DEBUG,
					MSG_SD_MOUNT, MSG_SD_NO_CFG, MSG_SD_NO_LANG, MSG_SD_MEMORY, MSG_SD_INCONSISTENT,
					MSG_LAST,
					MSG_ACTIVATE_TIPS 	= MSG_MENU_MAIN + 3,
					MSG_ABOUT 			= MSG_MENU_MAIN + 8,
					MSG_AUTO			= MSG_MENU_CALIB + 1,
					MSG_MANUAL			= MSG_MENU_CALIB + 2
} t_msg_id;

typedef struct s_msg_nls {
	const char		*msg;
	std::string		msg_nls;
} t_msg;

class NLS_MSG {
	public:
		NLS_MSG()											{ }
		void			activate(bool use_nls)				{ this->use_nls = use_nls; }
		const char*		msg(t_msg_id id);
		std::string		str(t_msg_id id);
		uint8_t			menuSize(t_msg_id id);
		bool			set(std::string& parameter, std::string& value, std::string& parent);
	protected:
		bool	use_nls		= false;
		t_msg		message[MSG_LAST] = {
				// MAIN MENU
				{"Main Menu",		std::string()},			// Title is the first element of each menu
				{"parameters",		std::string()},
				{"change T12 tip",	std::string()},
				{"activate tips",	std::string()},			// Change MSG_ACTIVATE_TIPS if new item menu inserted
				{"T12 setup",		std::string()},
				{"JBC setup",		std::string()},
				{"calibrate GUN",	std::string()},
				{"reset config",	std::string()},
				{"about",			std::string()},			// Change MSG_ABOUT if new item menu inserted
				{"quit",			std::string()},
				// SETUP MENU
				{"Parameters",		std::string()},			// Title
				{"units",			std::string()},
				{"buzzer",			std::string()},
				{"upper encoder",	std::string()},
				{"lower encoder",	std::string()},
				{"temp. step",		std::string()},
				{"brightness",		std::string()},			// Change in-place menu item
				{"rotation",		std::string()},			// Change in-place menu item
				{"language",		std::string()},			// Change in-place menu item
				{"tune PID",		std::string()},
				{"save",			std::string()},
				{"cancel",			std::string()},
				// T12 IRON MENU
				{"T12 iron setup",	std::string()},			// Title
				{"switch type",		std::string()},
				{"auto start",		std::string()},
				{"auto off",		std::string()},
				{"standby temp.",	std::string()},
				{"standby time",	std::string()},
				{"boost temp.",		std::string()},
				{"boost time",		std::string()},
				{"save",			std::string()},
				{"calibrate tip",	std::string()},
				{"back to menu",	std::string()},
				// JBC IRON MENU
				{"JBC iron setup",	std::string()},			// Title
				{"auto off",		std::string()},
				{"standby temp.",	std::string()},
				{"save",			std::string()},
				{"calibrate tip",	std::string()},
				{"back to menu",	std::string()},
				// IRON TIP CALIBRATION MENU
				{"Calibrate",		std::string()},			// Title
				{"automatic",		std::string()},			// Change MSG_AUTO if new item menu inserted
				{"manual",			std::string()},			// Change MSG_MANUAL if new item menu inserted
				{"clear",			std::string()},
				{"quit",			std::string()},
				// PID Tune Menu
				{"Tune PID",		std::string()},			// Title
				{"T12 PID",			std::string()},
				{"JBC PID",			std::string()},
				{"Gun PID",			std::string()},
				{"back to menu",	std::string()},
				// Configuration manage menu
				{"Manage config",	std::string()},			// Title
				{"Load lang data",	std::string()},
				{"Load config",		std::string()},
				{"Save config",		std::string()},
				{"quit",			std::string()},
				// SINGLE MESSAGE STRINGS
				{"ON",				std::string()},
				{"OFF",				std::string()},
				{"Fan:",			std::string()},
				{"pwr:",			std::string()},
				{"Ref. #",			std::string()},
				{"REED",			std::string()},
				{"TILT",			std::string()},
				{"deg.",			std::string()},
				{"min",				std::string()},
				{"sec",				std::string()},
				{"cw",				std::string()},
				{"ccw",				std::string()},
				{"Set:",			std::string()},
				{"ERROR",			std::string()},
				{"Tune PID",		std::string()},
				{"Select tip",		std::string()},
				{"FLASH read error",		std::string()},
				{"FLASH write error",		std::string()},
				{"No directory",			std::string()},
				{"format FLASH?",			std::string()},
				{"Failed to format FLASH",	std::string()},
				{"saving configuration",	std::string()},
				{"Hot Gun",					std::string()},
				{"T12 iron",				std::string()},
				{"JBC iron",				std::string()},
				{"Save?",					std::string()},
				{"Yes",						std::string()},
				{"No",						std::string()},
				{"Delete file?",			std::string()},
				{"FLASH debug",				std::string()},
				{"Failed mount SD",			std::string()},
				{"NO config file",			std::string()},
				{"No lang. specified",		std::string()},
				{"No memory",				std::string()},
				{"Inconsistent lang",		std::string()}
		};
		const t_msg_id menu[6] = { MSG_MENU_MAIN, MSG_MENU_SETUP, MSG_MENU_T12, MSG_MENU_JBC, MSG_MENU_CALIB, MSG_PID_MENU };
};

#endif
