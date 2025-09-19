/*
 * menu.cpp
 *
 *  Created on: 10 July 2022
 *      Author: Alex
 *
 *  2023 SEP 03
 *  	Added manage flash routines: save/load config files, load nls data
 *  2023 SEP 08, v 1.03
 *  	Changed the MENU_PID::loop(): added call of auto_pid routine
 *  2024 OCT 10, v.1.07
 *  	Added 'display type' setup menu item to MSETUP::init() and MSETUP::loop()
 *  2024 OCT 13
 *  	Added MENU_GUN::init() and MENU_GUN::loop() methods
 *  2024 NOV 05, v.1.08
 *  	Implemented 'safe_iron_mode' menu item into MSETUP:init() and MSETUP::loop()
 *
 */
#include "menu.h"

//---------------------- The Menu mode -------------------------------------------
MMENU::MMENU(HW* pCore, MODE *m_change_tip, MODE *m_params, MODE* m_act,
		MODE* m_t12_menu, MODE* m_jbc_menu, MODE* m_gun_menu, MODE *m_about) : MODE(pCore) {
	mode_change_tip		= m_change_tip;
	mode_menu_setup		= m_params;
	mode_activate_tips	= m_act;
	mode_t12_menu		= m_t12_menu;
	mode_jbc_menu		= m_jbc_menu;
	mode_gun_menu		= m_gun_menu;
	mode_about			= m_about;
}

void MMENU::init(void) {
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t menu_len = pCore->dspl.menuSize(MSG_MENU_MAIN);
	pEnc->reset(mode_menu_item, 0, menu_len-1, 1, 1, true);
	update_screen = 0;
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_MENU_MAIN);					// "Main menu"
}

MODE* MMENU::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t item 		= pEnc->read();
	uint8_t  button		= pEnc->buttonStatus();

	// Change the configuration parameters value in place
	if (mode_menu_item != item) {							// The encoder has been rotated
		mode_menu_item = item;
		update_screen = 0;									// Force to redraw the screen
	}

	// Going through the main menu
	if (button > 0) {										// The button was pressed, current menu item can be selected for modification
		switch (item) {										// item is a menu item
			case MM_PARAMS:
				return mode_menu_setup;
			case MM_CHANGE_TIP:								// Change tip
				if (mode_change_tip) {
					mode_change_tip->useDevice(d_unknown);	// Change the T12 TIP manually
					return mode_change_tip;
				}
				break;
			case MM_ACTIVATE_TIPS:							// activate tips
				return mode_activate_tips;
			case MM_T12_MENU:								// tune the IRON potentiometer
				return mode_t12_menu;
			case MM_JBC_MENU:								// tune the IRON potentiometer
				return mode_jbc_menu;
			case MM_GUN_MENU:								// Calibrate Hot Air Gun
				return mode_gun_menu;
			case MM_RESET_CONFIG:							// Initialize the configuration
				pCFG->clearConfig();
				mode_menu_item = 0;							// We will not return from tune mode to this menu
				return mode_return;
			case MM_ABOUT:									// About dialog
				mode_menu_item = 0;
				return mode_about;
			default:										// quit
				mode_menu_item = 0;
				return mode_return;
		}
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 10000;
	pD->menuShow(MSG_MENU_MAIN, item, 0, false);
	return this;
}

//---------------------- The Setup menu mode -------------------------------------
void MSETUP::init(void) {
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;
	buzzer			= pCFG->isBuzzerEnabled();
	celsius			= pCFG->isCelsius();
	temp_step		= pCFG->isBigTempStep();
	u_clock_wise	= pCFG->isUpperEncClockWise();
	l_clock_wise	= pCFG->isLowerEncClockWise();
	ips_display		= pCFG->isIPS();
	safe_iron_mode	= pCFG->isSafeIronMode();
	lang_index		= pCore->nls.languageIndex();
	num_lang		= pCore->nls.numLanguages();
	dspl_bright		= pCFG->getDsplBrightness();				// Brightness [1-255]
	dspl_rotation	= pCFG->getDsplRotation();
	set_param		= 0;
	uint8_t menu_len = pCore->dspl.menuSize(MSG_MENU_SETUP);
	pEnc->reset(mode_menu_item, 0, menu_len-1, 1, 1, true);
	update_screen = 0;
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_MENU_SETUP);						// "Parameters"
}

MODE* MSETUP::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t item 		= pEnc->read();
	uint8_t  button		= pEnc->buttonStatus();

	// Change the configuration parameters value in place
	if (mode_menu_item != item) {								// The encoder has been rotated
		mode_menu_item = item;
		switch (set_param) {									// Setup new value of the parameter in place
			case MM_BRIGHT:
				dspl_bright = constrain(item, 1, 255);
				pCore->dspl.BRGT::set(dspl_bright);
				break;
			case MM_ROTATION:
				dspl_rotation = constrain(item, 0, 3);
				pCore->dspl.rotate((tRotation)dspl_rotation);
				pCore->dspl.clear();
				pCore->dspl.drawTitle(MSG_MENU_SETUP);			// "Parameters"
				break;
			case MM_LANGUAGE:
				lang_index = item;
				break;
			default:
				break;
		}
		update_screen = 0;										// Force to redraw the screen
	}

	// Select setup menu Item
	if (!set_param) {											// Going through the menu
		if (button > 0) {										// The button was pressed, current menu item can be selected for modification
			switch (item) {										// item is a menu item
				case MM_UNITS:									// units C/F
					celsius	= !celsius;
					break;
				case MM_BUZZER:									// buzzer ON/OFF
					buzzer	= !buzzer;
					break;
				case MM_I_ENC:
					u_clock_wise = !u_clock_wise;
					break;
				case MM_G_ENC:
					l_clock_wise = !l_clock_wise;
					break;
				case MM_DSPL_TYPE:
					ips_display = !ips_display;
					break;
				case MM_SAFE_MODE:
					safe_iron_mode = !safe_iron_mode;
					break;
				case MM_TEMP_STEP:								// Preset temperature step (1/5)
					temp_step  = !temp_step;
					break;
				case MM_BRIGHT:
					set_param = item;
					pEnc->reset(dspl_bright, 1, 255, 1, 5, false);
					break;
				case MM_ROTATION:
					set_param = item;
					pEnc->reset(dspl_rotation, 0, 3, 1, 1, true);
					break;
				case MM_LANGUAGE:
					if (num_lang > 0) {
						set_param = item;
						pEnc->reset(lang_index, 0, num_lang-1, 1, 1, true);
					}
					break;
				case MM_SAVE:									// save
				{
					pD->clear();
					pD->errorMessage(MSG_SAVE_ERROR, 100);
					pCFG->umount();								// Prepare to load font data, clear status of the configuration file
					pD->dim(50);								// decrease the display brightness while saving configuration data
					// Perhaps, we should change the language: load messages and font
					if (lang_index != pCore->nls.languageIndex()) {
						pCore->nls.loadLanguageData(lang_index);
						// Check if the language successfully changed
						if (lang_index == pCore->nls.languageIndex()) {
							uint8_t *font = pCore->nls.font();
							pD->setLetterFont(font);
							std::string	l_name = pCore->nls.languageName(lang_index);
							pCFG->setLanguage(l_name.c_str());
						}
					}
					pCFG->setDsplRotation(dspl_rotation);
					pCFG->setup(buzzer, celsius, temp_step, u_clock_wise, l_clock_wise, ips_display, safe_iron_mode, dspl_bright);
					pCFG->saveConfig();
					pCore->u_enc.setClockWise(u_clock_wise);
					pCore->l_enc.setClockWise(l_clock_wise);
					pCore->buzz.activate(buzzer);
					mode_menu_item = 0;
					return mode_return;
				}
				case MM_PID:
					return mode_pid_tune;
				default:										// cancel
					pCFG->restoreConfig();
					mode_menu_item = 0;
					return mode_return;
			}
		}
	} else {													// Finish modifying  parameter, return to menu mode
		if (button == 1) {
			item 			= set_param;
			mode_menu_item 	= set_param;
			set_param = 0;
			uint8_t menu_len = pD->menuSize(MSG_MENU_SETUP);
			pEnc->reset(mode_menu_item, 0, menu_len-1, 1, 1, true);
		}
	}

	// Prepare to modify menu item in-place using built-in editor
	bool modify = false;
	if (set_param >= in_place_start && set_param <= in_place_end) {
		item = set_param;
		modify 	= true;
	}

	if (button > 0) {											// Either short or long press
		update_screen 	= 0;									// Force to redraw the screen
	}
	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 10000;

	// Build current menu item value
	const uint8_t value_length = 20;
	char item_value[value_length+1];
	item_value[1] = '\0';
	const char *msg_on 	= pD->msg(MSG_ON);
	const char *msg_off	= pD->msg(MSG_OFF);
	switch (item) {
		case MM_UNITS:											// units: C/F
			item_value[0] = 'F';
			if (celsius)
				item_value[0] = 'C';
			break;
		case MM_BUZZER:											// Buzzer setup
			sprintf(item_value, buzzer?msg_on:msg_off);
			break;
		case MM_TEMP_STEP:										// Preset temperature step (1/5)
			sprintf(item_value, "%1d ", temp_step?5:1);
			strncpy(&item_value[2], pD->msg(MSG_DEG), value_length-2);
			break;
		case MM_I_ENC:
			strncpy(item_value, pD->msg((u_clock_wise)?MSG_CW:MSG_CCW), value_length);
			break;
		case MM_G_ENC:
			strncpy(item_value, pD->msg((l_clock_wise)?MSG_CW:MSG_CCW), value_length);
			break;
		case MM_DSPL_TYPE:
			strncpy(item_value, pD->msg((ips_display)?MSG_DSPL_IPS:MSG_DSPL_TFT), value_length);
			break;
		case MM_SAFE_MODE:
			sprintf(item_value, "%3d", pCFG->tempMax(d_t12, celsius, safe_iron_mode));	// Maximum temperature is the same for both irons
			break;
		case MM_BRIGHT:
			{
			uint8_t pcnt = map(dspl_bright, 0, 255, 0, 100);
			sprintf(item_value, "%3d%c", pcnt, '%');
			}
			break;
		case MM_ROTATION:
			sprintf(item_value, "%3d", dspl_rotation*90);
			break;
		case MM_LANGUAGE:
		{
			std::string	l_name = pCore->nls.languageName(lang_index);
			strncpy(item_value, l_name.c_str(), value_length);
		}
			break;
		default:
			item_value[0] = '\0';
			break;
	}

	pD->menuShow(MSG_MENU_SETUP, item, item_value, modify);
	return this;
}

//---------------------- Calibrate tip menu --------------------------------------
MCALMENU::MCALMENU(HW* pCore, MODE* cal_auto, MODE* cal_manual) : MODE(pCore) {
	mode_calibrate_tip = cal_auto; mode_calibrate_tip_manual = cal_manual;
}

void MCALMENU::init(void) {
	DSPL* pD = &pCore->dspl;
	uint8_t menu_len = pD->menuSize(MSG_MENU_CALIB);
	pCore->l_enc.reset(0, 0, menu_len-1, 1, 1, true);
	pCore->dspl.clear();
	std::string title = pD->str(MSG_MENU_CALIB);				// "Calibrate"
	title += " ";
	if (dev_type == d_gun) {
		title += pD->str(MSG_HOT_AIR_GUN);
	} else if (dev_type == d_jbc) {
		title += pD->str(MSG_JBC_IRON);
	} else {
		title += pD->str(MSG_T12_IRON);
	}
	pCore->dspl.drawTitleString(title.c_str());
	update_screen	= 0;
}

MODE* MCALMENU::loop(void) {
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t item 	= pEnc->read();
	uint8_t button	= pEnc->buttonStatus();

	if (button == 1) {
		update_screen = 0;										// Force to redraw the screen
	} else if (button == 2) {									// The button was pressed for a long time
	   	return mode_lpress;
	}

	if (pEnc->changed() != 0) {
		update_screen = 0;										// Force to redraw the screen
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 30000;

	if (button == 1) {											// The button was pressed
		switch (item) {
			case MC_AUTO:										// Calibrate tip automatically
				mode_calibrate_tip->useDevice(dev_type);
				return mode_calibrate_tip;
			case MC_MANUAL:										// Calibrate tip manually
				mode_calibrate_tip_manual->useDevice(dev_type);
				return mode_calibrate_tip_manual;
			case MC_CLAER:										// Initialize tip calibration data
				pCFG->resetTipCalibration(dev_type);
				pCore->buzz.shortBeep();
				pEnc->write(0);
				return this;
			default:											// exit
				return mode_return;
		}
	}

	pCore->dspl.menuShow(MSG_MENU_CALIB, item, 0, false);
	return this;
}

//---------------------- T12 IRON setup menu -------------------------------------
void MENU_T12::init(void) {
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;
	reed			= pCFG->isReedType();
	auto_start		= pCFG->isAutoStart();
	off_timeout		= pCFG->getOffTimeout(d_t12);
	low_temp		= pCFG->getLowTemp(d_t12);
	low_to			= pCFG->getLowTO();
	delta_temp		= pCFG->boostTemp();							// The boost temp is in the internal units
	duration		= pCFG->boostDuration();
	set_param		= 0;
	uint8_t m_len 	= pCore->dspl.menuSize(MSG_MENU_T12);
	uint8_t pos		= pCFG->isTipCalibrated(d_t12)?0:MT_CALIBRATE;
	pEnc->reset(pos, 0, m_len-1, 1, 1, true);
	update_screen = 0;
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_MENU_T12);						// "T12 iron setup"
}

MODE* MENU_T12::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t item 		= pEnc->read();
	uint8_t  button		= pEnc->buttonStatus();

	// Change the configuration parameters value in place
	if (pEnc->changed() != 0) {									// The encoder has been rotated
		switch (set_param) {									// Setup new value of the parameter in place
			case MT_AUTO_OFF:									// Setup auto off timeout
				if (item) {
					off_timeout	= item + 2;
				} else {
					off_timeout = 0;
				}
				break;
			case MT_STANDBY_TEMP:								// Setup low power (standby) temperature
				if (item >= min_standby_C) {
					low_temp = item;
				} else {
					low_temp = 0;
				}
				break;
			case MT_STANDBY_TIME:								// Setup low power (standby) timeout
				low_to	= item;
				break;
			case MT_BOOST_TEMP:
				delta_temp	= item;
				break;
			case MT_BOOST_TIME:
				duration	= item;
				break;
			default:											// cancel
				break;
		}
		update_screen = 0;										// Force to redraw the screen
	}

	// Select setup menu Item
	if (!set_param) {											// Menu item (parameter) to modify was not selected yet
		if (button > 0) {										// The button was pressed, current menu item can be selected for modification
			switch (item) {										// item is a menu item
				case MT_SWITCH_TYPE:							// units C/F
					reed = !reed;
					break;
				case MT_AUTO_START:
					auto_start = !auto_start;
					break;
				case MT_AUTO_OFF:								// auto off timeout
					{
					set_param = item;
					uint8_t to = off_timeout;
					if (to > 2) to -=2;
					pEnc->reset(to, 0, 28, 1, 1, false);
					break;
					}
				case MT_STANDBY_TEMP:							// Standby temperature
					{
					set_param = item;
					uint16_t max_standby_C = pCFG->referenceTemp(0, d_t12);
					// When encoder value is less than min_standby_C, disable low power mode
					pEnc->reset(low_temp, min_standby_C-1, max_standby_C, 1, 5, false);
					break;
					}
				case MT_STANDBY_TIME:							// Standby timeout
					set_param = item;
					pEnc->reset(low_to, 1, 255, 1, 1, false);
					break;
				case MT_BOOST_TEMP:								// BOOST delta temperature
					set_param = item;
					pEnc->reset(delta_temp, 0, 75, 5, 5, false);
					break;
				case MT_BOOST_TIME:								// BOOST duration
					set_param = item;
					pEnc->reset(duration, 20, 320, 20, 20, false);
					break;
				case MT_SAVE:									// save
					pD->BRGT::dim(50);							// Turn-off the brightness, processing
					pCFG->setupT12(reed, auto_start, off_timeout, low_temp, low_to, delta_temp, duration);
					pCFG->saveConfig();
					return mode_return;
				case MT_CALIBRATE:
					if (mode_calibrate) {
						mode_calibrate->useDevice(d_t12);
						return mode_calibrate;
					}
					break;
				default:										// cancel
					pCFG->restoreConfig();
					mode_menu_item = 0;
					return mode_return;
			}
		}
	} else {													// Finish modifying  parameter, return to menu mode
		if (button == 1) {
			item 			= set_param;
			mode_menu_item 	= set_param;
			set_param = 0;
			uint8_t menu_len = pD->menuSize(MSG_MENU_T12);
			pEnc->reset(mode_menu_item, 0, menu_len-1, 1, 1, true);
		}
	}

	// Prepare to modify menu item in-place using built-in editor
	bool modify = false;
	if (set_param >= in_place_start && set_param <= in_place_end) {
		item = set_param;
		modify 	= true;
	}

	if (button > 0) {											// Either short or long press
		update_screen 	= 0;									// Force to redraw the screen
	}
	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 10000;

	// Build current menu item value
	const uint8_t value_length = 20;
	char item_value[value_length+1];
	item_value[1] = '\0';
	const char *msg_on 	= pD->msg(MSG_ON);
	const char *msg_off	= pD->msg(MSG_OFF);
	switch (item) {
		case MT_SWITCH_TYPE:									// TILT/REED
			strncpy(item_value, pD->msg((reed)?MSG_REED:MSG_TILT), value_length);
			break;
		case MT_AUTO_START:										// Auto start ON/OFF
			sprintf(item_value, auto_start?msg_on:msg_off);
			break;
		case MT_AUTO_OFF:										// auto off timeout
			if (off_timeout) {
				sprintf(item_value, "%2d ", off_timeout);
				strncpy(&item_value[3], pD->msg(MSG_MINUTES), value_length - 3);
			} else {
				strncpy(item_value, pD->msg(MSG_OFF), value_length);
			}
			break;
		case MT_STANDBY_TEMP:									// Standby temperature
			if (low_temp) {
				if (pCFG->isCelsius()) {
					sprintf(item_value, "%3d C", low_temp);
				} else {
					sprintf(item_value, "%3d F", celsiusToFahrenheit(low_temp));
				}
			} else {
				strncpy(item_value, pD->msg(MSG_OFF), value_length);
			}
			break;
		case MT_STANDBY_TIME:									// Standby timeout (5 secs intervals)
			if (low_temp) {
				uint16_t to = (uint16_t)low_to * 5;				// Timeout in seconds
				if (to < 60) {
					sprintf(item_value, "%2d ", to);
					strncpy(&item_value[3], pD->msg(MSG_SECONDS), value_length - 3);
				} else if (to %60) {
					sprintf(item_value, "%2d ", to/60);
					uint8_t p = 3;
					strncpy(&item_value[p], pD->msg(MSG_MINUTES), value_length-p);
					p += strlen(pD->msg(MSG_MINUTES));
					if (p < value_length) {
						sprintf(&item_value[p], " %2d ", to%60);
						p += 4;
						if (p < value_length) {
							strncpy(&item_value[p], pD->msg(MSG_SECONDS), value_length-p);
						}
					}
				} else {
					sprintf(item_value, "%2d ", to/60);
					strncpy(&item_value[3], pD->msg(MSG_MINUTES), value_length-3);
				}
			} else {
				strncpy(item_value, pD->msg(MSG_OFF), value_length);
			}
			break;
		case MT_BOOST_TEMP:								// BOOST delta temperature
			if (delta_temp) {
				uint16_t delta_t = delta_temp;
				char sym = 'C';
				if (!pCFG->isCelsius()) {
					delta_t = (delta_t * 9 + 3) / 5;
					sym = 'F';
				}
				sprintf(item_value, "+%2d %c", delta_t, sym);
			} else {
				strncpy(item_value, pCore->dspl.msg(MSG_OFF), value_length);
			}
			break;
		case MT_BOOST_TIME:								// BOOST duration
		    sprintf(item_value, "%3d ", duration);
		    strncpy(&item_value[4], pCore->dspl.msg(MSG_SECONDS), value_length-4);
			break;
		default:
			item_value[0] = '\0';
			break;
	}

	pD->menuShow(MSG_MENU_T12, item, item_value, modify);
	return this;
}

//---------------------- JBC IRON setup menu -------------------------------------
void MENU_JBC::init(void) {
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;
	off_timeout		= pCFG->getOffTimeout(d_jbc);
	stby_temp		= pCFG->getLowTemp(d_jbc);
	set_param		= -1;
	uint8_t m_len 	= pCore->dspl.menuSize(MSG_MENU_JBC);
	uint8_t pos		= pCFG->isTipCalibrated(d_jbc)?0:MJ_CALIBRATE;
	pEnc->reset(pos, 0, m_len-1, 1, 1, true);
	update_screen = 0;
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_MENU_JBC);						// "JBC iron setup"
}

MODE* MENU_JBC::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t item 		= pEnc->read();
	uint8_t  button		= pEnc->buttonStatus();

	// Change the configuration parameters value in place
	if (pEnc->changed() != 0) {									// The encoder has been rotated
		switch (set_param) {									// Setup new value of the parameter in place
			case MJ_AUTO_OFF:									// Setup auto off timeout
				off_timeout	= item;
				break;
			case MJ_STANDBY_TEMP:								// Setup low power (standby) temperature
				if (item >= min_standby_C) {
					stby_temp = item;
				} else {
					stby_temp = 0;
				}
				break;
			default:											// cancel
				break;
		}
		update_screen = 0;										// Force to redraw the screen
	}

	// Select setup menu Item
	if (set_param < 0) {										// Menu item (parameter) to modify was not selected yet
		if (button > 0) {										// The button was pressed, current menu item can be selected for modification
			switch (item) {										// item is a menu item
				case MJ_AUTO_OFF:								// auto off timeout
					set_param = item;
					pEnc->reset(off_timeout, 0, 30, 1, 1, false);
					break;
				case MJ_STANDBY_TEMP:							// Standby temperature
					{
					set_param = item;
					uint16_t max_standby_C = pCFG->referenceTemp(0, d_jbc);
					// When encoder value is less than min_standby_C, disable standby mode
					pEnc->reset(stby_temp, min_standby_C-1, max_standby_C, 1, 5, false);
					break;
					}
				case MJ_SAVE:									// save
					pD->BRGT::dim(50);							// Turn-off the brightness, processing
					pCFG->setupJBC(off_timeout, stby_temp);
					pCFG->saveConfig();
					return mode_return;
				case MJ_CALIBRATE:
					if (mode_calibrate) {
						mode_calibrate->useDevice(d_jbc);
						return mode_calibrate;
					}
					break;
				default:										// cancel
					pCFG->restoreConfig();
					mode_menu_item = 0;
					return mode_return;
			}
		}
	} else {													// Finish modifying  parameter, return to menu mode
		if (button == 1) {
			item 			= set_param;
			mode_menu_item 	= set_param;
			set_param = -1;
			uint8_t menu_len = pD->menuSize(MSG_MENU_JBC);
			pEnc->reset(mode_menu_item, 0, menu_len-1, 1, 1, true);
		}
	}

	// Prepare to modify menu item in-place using built-in editor
	bool modify = false;
	if (set_param >= in_place_start && set_param <= in_place_end) {
		item = set_param;
		modify 	= true;
	}

	if (button > 0) {											// Either short or long press
		update_screen 	= 0;									// Force to redraw the screen
	}
	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 10000;

	// Build current menu item value
	const uint8_t value_length = 20;
	char item_value[value_length+1];
	item_value[1] = '\0';
	switch (item) {
		case MJ_AUTO_OFF:										// auto off timeout
			if (off_timeout) {
				sprintf(item_value, "%2d ", off_timeout);
				strncpy(&item_value[3], pD->msg(MSG_MINUTES), value_length - 3);
			} else {
				strncpy(item_value, pD->msg(MSG_OFF), value_length);
			}
			break;
		case MJ_STANDBY_TEMP:									// Standby temperature
			if (stby_temp) {
				if (pCFG->isCelsius()) {
					sprintf(item_value, "%3d C", stby_temp);
				} else {
					sprintf(item_value, "%3d F", celsiusToFahrenheit(stby_temp));
				}
			} else {
				strncpy(item_value, pD->msg(MSG_OFF), value_length);
			}
			break;
		default:
			item_value[0] = '\0';
			break;
	}

	pD->menuShow(MSG_MENU_JBC, item, item_value, modify);
	return this;
}

//---------------------- Hot Air Gun setup menu ----------------------------------
void MENU_GUN::init(void) {
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;
	fast_gun_chill	= pCFG->isFastGunCooling();
	stby_timeout	= pCFG->getOffTimeout(d_gun);
	stby_temp		= pCFG->getLowTemp(d_gun);
	set_param		= -1;
	uint8_t m_len 	= pCore->dspl.menuSize(MSG_MENU_GUN);
	uint8_t pos		= pCFG->isTipCalibrated(d_gun)?0:MG_CALIBRATE;
	pEnc->reset(pos, 0, m_len-1, 1, 1, true);
	update_screen = 0;
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_MENU_GUN);						// "Hot Gun setup"
}

MODE* MENU_GUN::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t item 		= pEnc->read();
	uint8_t  button		= pEnc->buttonStatus();

	// Change the configuration parameters value in place
	if (pEnc->changed() != 0) {									// The encoder has been rotated
		switch (set_param) {									// Setup new value of the parameter in place
			case MG_STBY_TO:									// Setup standby timeout
				stby_timeout	= item;
				break;
			case MG_STANDBY_TEMP:								// Setup low power (standby) temperature
				if (item >= min_standby_C) {
					stby_temp = item;
				} else {
					stby_temp = 0;
				}
				break;
			default:											// cancel
				break;
		}
		update_screen = 0;										// Force to redraw the screen
	}

	// Select setup menu Item
	if (set_param < 0) {										// Menu item (parameter) to modify was not selected yet
		if (button > 0) {										// The button was pressed, current menu item can be selected for modification
			switch (item) {										// item is a menu item
				case MG_FAST_CHILL:								// Fast Hot Gun chill
					fast_gun_chill	= !fast_gun_chill;
					break;
				case MG_STBY_TO:								// standby timeout
					set_param = item;
					pEnc->reset(stby_timeout, 0, 30, 1, 1, false);
					break;
				case MG_STANDBY_TEMP:							// Standby temperature
					{
					set_param = item;
					uint16_t max_standby_C = pCFG->referenceTemp(0, d_gun);
					// When encoder value is less than min_standby_C, disable standby mode
					pEnc->reset(stby_temp, min_standby_C-1, max_standby_C, 1, 5, false);
					break;
					}
				case MG_SAVE:									// save
					pD->BRGT::dim(50);							// Turn-off the brightness, processing
					pCFG->setupGUN(fast_gun_chill, stby_timeout, stby_temp);
					pCFG->saveConfig();
					return mode_return;
				case MG_CALIBRATE:
					if (mode_calibrate) {
						mode_calibrate->useDevice(d_gun);
						return mode_calibrate;
					}
					break;
				default:										// cancel
					pCFG->restoreConfig();
					mode_menu_item = 0;
					return mode_return;
			}
		}
	} else {													// Finish modifying  parameter, return to menu mode
		if (button == 1) {
			item 			= set_param;
			mode_menu_item 	= set_param;
			set_param = -1;
			uint8_t menu_len = pD->menuSize(MSG_MENU_GUN);
			pEnc->reset(mode_menu_item, 0, menu_len-1, 1, 1, true);
		}
	}

	// Prepare to modify menu item in-place using built-in editor
	bool modify = false;
	if (set_param >= in_place_start && set_param <= in_place_end) {
		item = set_param;
		modify 	= true;
	}

	if (button > 0) {											// Either short or long press
		update_screen 	= 0;									// Force to redraw the screen
	}
	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 10000;

	// Build current menu item value
	const uint8_t value_length = 20;
	char item_value[value_length+1];
	item_value[1] = '\0';
	switch (item) {
		case MG_FAST_CHILL:										// Chill the Gun at a maximum fan speed
			strncpy(item_value, pD->msg((fast_gun_chill)?MSG_ON:MSG_OFF), value_length);
			break;
		case MG_STBY_TO:										// standby timeout
			if (stby_timeout) {
				sprintf(item_value, "%2d ", stby_timeout);
				strncpy(&item_value[3], pD->msg(MSG_MINUTES), value_length - 3);
			} else {
				strncpy(item_value, pD->msg(MSG_OFF), value_length);
			}
			break;
		case MG_STANDBY_TEMP:									// Standby temperature
			if (stby_temp) {
				if (pCFG->isCelsius()) {
					sprintf(item_value, "%3d C", stby_temp);
				} else {
					sprintf(item_value, "%3d F", celsiusToFahrenheit(stby_temp));
				}
			} else {
				strncpy(item_value, pD->msg(MSG_OFF), value_length);
			}
			break;
		default:
			item_value[0] = '\0';
			break;
	}

	pD->menuShow(MSG_MENU_GUN, item, item_value, modify);
	return this;
}

//---------------------- PID setup menu ------------------------------------------
void MENU_PID::init(void) {
	uint8_t menu_len = pCore->dspl.menuSize(MSG_PID_MENU);
	pCore->l_enc.reset(0, 0, menu_len-1, 1, 1, true);
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_PID_MENU);						// "Tune PID"
	update_screen	= 0;
}

MODE* MENU_PID::loop(void) {
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t item 	= pEnc->read();
	uint8_t button	= pEnc->buttonStatus();
	uint8_t butt_up	= pCore->u_enc.buttonStatus();				// Use upper encoder to call auto_pid routine

	if (button == 1 || butt_up == 1) {
		update_screen = 0;										// Force to redraw the screen
	} else if (button == 2) {									// The button was pressed for a long time
	   	return mode_lpress;
	}

	if (pEnc->changed() != 0) {
		update_screen = 0;										// Force to redraw the screen
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 30000;

	if (button == 1) {											// The button was pressed
		if (!mode_pid) return this;
		switch (item) {
			case MP_T12:										// Tune PID of T12 IRON
				mode_pid->useDevice(d_t12);
				return mode_pid;
			case MP_JBC:										// Tune JBC parameters
				mode_pid->useDevice(d_jbc);
				return mode_pid;
			case MP_GUN:										// Tune Hot Air Gun PID parameters
				mode_pid->useDevice(d_gun);
				return mode_pid;
			default:											// exit
				return mode_return;
		}
	} else if (butt_up == 1) {									// Upper encoder used to call auto_pid routine
		if (!mode_auto_pid) return this;
		switch (item) {
			case MP_T12:										// Tune PID of T12 IRON
				mode_auto_pid->useDevice(d_t12);
				return mode_auto_pid;
			case MP_JBC:										// Tune JBC parameters
				mode_auto_pid->useDevice(d_jbc);
				return mode_auto_pid;
			default:											// exit
				return this;
		}
	}

	pCore->dspl.menuShow(MSG_PID_MENU, item, 0, false);
	return this;
}

//---------------------- FLASH manage menu ---------------------------------------
void MENU_FLASH::init(void) {
	uint8_t menu_len = pCore->dspl.menuSize(MSG_FLASH_MENU);
	pCore->l_enc.reset(0, 0, menu_len-1, 1, 1, true);
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_FLASH_MENU);						// "Manage Config"
	update_screen	= 0;
}

MODE* MENU_FLASH::loop(void) {
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t item 	= pEnc->read();
	uint8_t button	= pEnc->buttonStatus();

	if (button == 1) {
		update_screen = 0;										// Force to redraw the screen
	} else if (button == 2) {									// The button was pressed for a long time
	   	return mode_lpress;
	}

	if (pEnc->changed() != 0) {
		update_screen = 0;										// Force to redraw the screen
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 30000;

	if (button == 1) {											// The button was pressed
		if (item == MF_QUIT)
			return mode_return;
		pCore->cfg.umount();									// SPI FLASH will be mounted later to copy data files
		pCore->dspl.clear();
		pCore->dspl.dim(50);
		pCore->dspl.debugMessage("Copying files", 10, 100, 100);
		t_msg_id e = MSG_LAST;
		switch (item) {
			case MF_LOAD_LANG:									// Load nls language files from SD card
				e = lang_loader.loadNLS();
				break;
			case MF_LOAD_CFG:									// Load main configuration files from SD card
				e = lang_loader.loadCfg(pCore);
				break;
			case MF_SAVE_CFG:									// Save main configuration files to SD card
				e = lang_loader.saveCfg(pCore);
				break;
			default:											// exit
				break;
		}
		if (e == MSG_LAST) {
			pCore->buzz.shortBeep();
		} else {
			char sd_status[5];
			sprintf(sd_status, "%3d", lang_loader.sdStatus());	// SD status initialized by SD_Init() function (see sdspi.c)
			pFail->setMessage(e, sd_status);
			return pFail;
		}
		return mode_return;
	}

	pCore->dspl.menuShow(MSG_FLASH_MENU, item, 0, false);
	return this;
}
