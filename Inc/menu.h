/*
 * menu.h
 *
 *  Created on: 10 July 2022
 *      Author: Alex
 *
 *  Sep 03 2023
 *  	Added MENU_FLASH class
 */

#ifndef MENU_H_
#define MENU_H_
#include "mode.h"
#include "sdload.h"

//---------------------- The Menu mode -------------------------------------------
class MMENU : public MODE {
	public:
		MMENU(HW* pCore, MODE *m_change_tip, MODE *m_params, MODE* m_act, MODE* m_t12_menu, MODE* m_jbc_menu, MODE* m_gun_calib, MODE *m_about);
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		MODE*		mode_change_tip;
		MODE*		mode_menu_setup;
		MODE*		mode_activate_tips;
		MODE*		mode_t12_menu;
		MODE*		mode_jbc_menu;
		MODE*		mode_calib_menu;
		MODE*		mode_about;
		uint8_t		mode_menu_item 	= 1;					// Save active menu element index to return back later
		const uint16_t	min_standby_C	= 120;				// Minimum standby temperature, Celsius
		enum { MM_PARAMS = 0, MM_CHANGE_TIP, MM_ACTIVATE_TIPS, MM_T12_MENU, MM_JBC_MENU, MM_CALIB_GUN,
			MM_RESET_CONFIG, MM_ABOUT, MM_QUIT
		};
};

//---------------------- The Setup menu mode -------------------------------------------
class MSETUP : public MODE {
public:
	MSETUP(HW* pCore, MODE *m_pid_tune) : MODE(pCore)	{ mode_pid_tune = m_pid_tune; }
	virtual void	init(void);
	virtual MODE*	loop(void);
private:
	MODE*		mode_pid_tune	= 0;
	bool		buzzer			= true;					// Whether the buzzer is enabled
	bool		celsius			= true;					// Temperature units: C/F
	bool		temp_step		= false;				// The preset temperature step (1/5)
	bool		u_clock_wise	= true;					// The rotary encoder mode
	bool		l_clock_wise	= true;
	uint8_t		dspl_bright		= 100;					// Display brightness
	uint8_t		dspl_rotation	= 0;					// Display rotation
	uint8_t		lang_index		= 0;					// Language Index (0 - english)
	uint8_t		num_lang		= 0;					// Number of the loaded languages
	uint8_t		set_param		= 0;					// The index of the modifying parameter
	uint8_t		mode_menu_item 	= 0;					// Save active menu element index to return back later
	// When new menu item added, in_place_start, in_place_end, tip_calib_menu constants should be adjusted
	const uint8_t	in_place_start	= 4;				// See the menu names. Index of the first parameter that can be changed inside menu (see nls.h)
	const uint8_t	in_place_end	= 7;				// See the menu names. Index of the last parameter that can be changed inside menu
	enum { MM_UNITS = 0, MM_BUZZER, MM_I_ENC, MM_G_ENC, MM_TEMP_STEP, MM_BRIGHT, MM_ROTATION, MM_LANGUAGE, MM_PID, MM_SAVE, MM_CANCEL
	};
};

//---------------------- Calibrate tip menu --------------------------------------
class MCALMENU : public MODE {
	public:
		MCALMENU(HW* pCore, MODE* cal_auto, MODE* cal_manual);
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		MODE*			mode_calibrate_tip;
		MODE*			mode_calibrate_tip_manual;
		enum { MC_AUTO = 0, MC_MANUAL, MC_CLAER, MC_QUIT };
};

//---------------------- T12 IRON setup menu -------------------------------------
class MENU_T12 : public MODE {
	public:
		MENU_T12(HW* pCore, MODE* calib) : MODE(pCore)		{ mode_calibrate = calib; }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		MODE*		mode_calibrate;
		MODE*		mode_pid;
		bool		reed			= false;				// T12 IRON switch type: reed/tilt
		bool		auto_start		= false;				// T12 automatic startup
		uint8_t		off_timeout		= 0;					// Automatic switch off timeout in minutes or 0 to disable
		uint16_t	low_temp		= 0;					// The low power temperature (Celsius) 0 - disable tilt sensor
		uint8_t		low_to			= 0;					// The low power timeout, seconds
		uint8_t		delta_temp		= 0;					// The boost temp is in the internal units
		uint16_t	duration		= 0;					// The boost time duration
		uint8_t		set_param		= 0;					// The index of the modifying parameter
		uint8_t		mode_menu_item	= 0;
		// When new menu item added, in_place_start, in_place_end, tip_calib_menu constants should be adjusted
		const uint8_t	in_place_start	= 2;				// See the menu names. Index of the first parameter that can be changed inside menu (see nls.h)
		const uint8_t	in_place_end	= 6;				// See the menu names. Index of the last parameter that can be changed inside menu
		const uint16_t	min_standby_C	= 120;				// Minimum standby temperature, Celsius
		enum { MT_SWITCH_TYPE = 0, MT_AUTO_START, MT_AUTO_OFF, MT_STANDBY_TEMP, MT_STANDBY_TIME,
			MT_BOOST_TEMP, MT_BOOST_TIME, MT_SAVE, MT_CALIBRATE, MT_BACK
		};
};

//---------------------- JBC IRON setup menu -------------------------------------
class MENU_JBC : public MODE {
	public:
		MENU_JBC(HW* pCore, MODE* calib) : MODE(pCore)		{ mode_calibrate = calib; }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		MODE*		mode_calibrate;
		uint8_t		off_timeout		= 0;					// Automatic switch off timeout in minutes or 0 to disable
		uint16_t	stby_temp		= 0;					// The low power temperature (Celsius) 0 - switch off the JBC IRON immediately
		int8_t		set_param		= -1;					// The index of the modifying parameter
		uint8_t		mode_menu_item	= 0;
		// When new menu item added, in_place_start, in_place_end, tip_calib_menu constants should be adjusted
		const uint8_t	in_place_start	= 0;				// See the menu names. Index of the first parameter that can be changed inside menu (see nls.h)
		const uint8_t	in_place_end	= 1;				// See the menu names. Index of the last parameter that can be changed inside menu
		const uint16_t	min_standby_C	= 120;				// Minimum standby temperature, Celsius
		enum { MJ_AUTO_OFF = 0, MJ_STANDBY_TEMP, MJ_SAVE, MJ_CALIBRATE, MJ_BACK };
};

//---------------------- PID setup menu ------------------------------------------
class MENU_PID : public MODE {
	public:
		MENU_PID(HW* pCore, MODE* pid_tune)	: MODE(pCore)	{ mode_pid		= pid_tune; }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		MODE*			mode_pid;
		enum { MP_T12 = 0, MP_JBC, MP_GUN, MP_BACK };
};

//---------------------- FLASH manage menu ---------------------------------------
class MENU_FLASH : public MODE {
	public:
		MENU_FLASH(HW* pCore, MFAIL *pFail)	: MODE(pCore)	{ this->pFail = pFail; }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		MFAIL			*pFail;
		SDLOAD			lang_loader;						// To load language data from sd-card to flash
		enum { MF_LOAD_LANG = 0, MF_LOAD_CFG, MF_SAVE_CFG, MF_QUIT };
};

#endif
