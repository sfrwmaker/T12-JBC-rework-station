/*
 * work_mode.cpp
 *
 *		Created: 2022 DEC 19
 *      Author: Alex
 *
 *      The whole configuration include preset temperatures of all devices can be dumped to the FLASH
 *      by CFG::saveSonfig() procedure when:
 *      - the Hot Air Gun turned off (see MWORK::manageHardwareSwitches())
 *      - the JBC iron is on-hook (see MWORK::manageHardwareSwitches())
 *      - the JBC iron is turned-off by timeout (see MWORK::jbcPhaseEnd())
 *      - the Hakko T12 iron is turned-off (see MWORK::t12PressShort() and MWORK::t12PhaseEnd())
 *
 * 2023 MAR 01, v1.01
 *     Heavily revisited the code, many changes:
 *     MWORK::init(), MWORK::loop(), MWORK::manageHardwareSwitches(), MWORK::manageEncoders()
 * 2023 SEP 08, v1.03
 * 	   MWORK::init() added call pD->drawAmbient() to show ambient temperature and horizontal line when mode activated for sure
 * 2024 JUB 28, v1.04
 *     MWORK::jbcPressShort() when the jbc is powered off and the hot gun is running, turn to the T12_GUN display mode
 * 2024 OCT 12, v.1.07
 * 	   Changed MWORK::manageHardwareSwitches(), MWORK::manageEncoders() to implement Hot Gun standby mode
 * 2024 NOV 06, v.1.08
 * 		Modified MWORK::manageEncoders() to change Fan speed by 1%
 */

#include "work_mode.h"
#include "core.h"

//-------------------- The iron main working mode, keep the temperature ----------
// Do initialize the IRONs and Hot Air Gun preset temperature when the device is turned on
void MWORK::init(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	HOTGUN*	pHG		= &pCore->hotgun;

	ambient					= pCore->ambientTemp();
	uint16_t 	fan			= pCFG->gunFanPreset();
	pHG->setFan(fan);										// Setup the Hot Air Gun fan speed to be able to use the pHG->presetFanPcnt() (see below)
	// Initialize devices with a preset temperature
	uint16_t temp	= pCFG->tempPresetHuman(d_jbc);
	uint16_t temp_i	= pCFG->humanToTemp(temp, ambient, d_jbc);
	pCore->jbc.setTemp(temp_i);
	temp			= pCFG->tempPresetHuman(d_t12);
	temp_i			= pCFG->humanToTemp(temp, ambient, d_t12);
	pCore->t12.setTemp(temp_i);
	temp			= pCFG->tempPresetHuman(d_gun);
	temp_i			= pCFG->humanToTemp(temp, ambient, d_gun);
	pCore->hotgun.setTemp(temp_i);
	pD->drawAmbient(ambient, pCFG->isCelsius());

	DASH::init();
	if (start && !not_t12 && pCFG->isAutoStart()) {			// The T12 IRON can be started just after power-on. Default DASH mode is DM_T12_GUN
		pCore->t12.switchPower(true);
		t12_phase	= IRPH_HEATING;
		start = false;										// Prevent to start IRON automatically when return from menu
	} else if (not_t12) {
		t12_phase 	= IRPH_OFF;
	} else {
		t12_phase	= pCore->t12.isCold()?IRPH_OFF:IRPH_COOLING;
	}
	jbc_phase		= pCore->jbc.isCold()?IRPH_OFF:IRPH_COOLING;
	update_screen	= 0;									// Force to redraw the screen
	tilt_time		= 0;									// No tilt change
	lowpower_time	= 0;									// Reset the low power mode time
	t12_phase_end	= 0;
	jbc_phase_end	= 0;
	swoff_time		= 0;
	check_jbc_tm	= 0;
	edit_temp		= true;
	return_to_temp	= 0;
	gun_switch_off	= 0;
	pD->clear();
	initDevices(true, true);
	if (!not_t12)
		pCore->t12.setCheckPeriod(6);						// Start checking the current through T12 IRON
}

MODE* MWORK::loop(void) {
	CFG*	pCFG	= &pCore->cfg;
	IRON*	pT12	= &pCore->t12;
	IRON*	pJBC	= &pCore->jbc;
	HOTGUN*	pHG		= &pCore->hotgun;

	manageHardwareSwitches(pCFG, pT12, pJBC, pHG);

	// Check the JBC IRON tip change switch
	if (mode_spress && pJBC->isChanging()) {
		mode_spress->useDevice(d_jbc);
		return mode_spress;
	}

	if (manageEncoders()) {									// Lower encoder long pressed
		if (mode_lpress) {									// Go to the main menu
			pCore->buzz.shortBeep();
			return mode_lpress;
		}
	}
	animateFan();											// Draw the fan animated icon. The color depends on the Gun temperature

    if (HAL_GetTick() < update_screen) return this;
    update_screen = HAL_GetTick() + period;

	if (t12_phase_end > 0 && HAL_GetTick() >= t12_phase_end) {
		t12PhaseEnd();
	}
	if (jbc_phase_end > 0 && HAL_GetTick() >= jbc_phase_end) {
		jbcPhaseEnd();
	}
	if (gun_switch_off > 0 && HAL_GetTick() >= gun_switch_off) {
		gun_switch_off = 0;
		pHG->switchPower(false);
		int16_t temp	= pCore->cfg.tempPresetHuman(d_gun);
		presetTemp(d_gun, temp);							// Restore Hot Air Gun preset temperature
		fanSpeed(false);									// Restore Hot Air Gun fan speed
		disableGUN();
		pCFG->saveConfig();									// Save configuration when the Hot Air Gun is turned-off
		ironPhase(d_gun, IRPH_OFF);
	}

    // Check the T12 IRON handle connectivity
	if (no_ambient) {										// The T12 handle was disconnected
		if (!pCore->noAmbientSensor()) {					// The T12 handle attached again
			no_ambient = false;
			pT12->setCheckPeriod(6);						// Start checking the current through the T12 IRON
			disableJBC();
		}
	} else {												// The T12 handle attached
		if (pCore->noAmbientSensor()) { 					// The T12 handle disconnected
			no_ambient = true;
			pT12->setCheckPeriod(0);						// Stop checking the current through the T12 IRON
			if (!is_extra_tip)
				enableJBC();
		}
	}

	// if T12 IRON tip is disconnected, activate Tip selection mode
	if (mode_spress && !no_ambient && !pT12->isConnected() && isACsine()) {
		if (isIronCold(t12_phase)) {						// The T12 IRON is not active
			mode_spress->useDevice(d_t12);
			return mode_spress;
		}
	}

    ambient	= pCore->ambientTemp();
	if (t12_phase == IRPH_COOLING && pT12->isCold()) {
		pCore->buzz.lowBeep();
		t12_phase = IRPH_COLD;
		t12_phase_end = HAL_GetTick() + 20000;
		ironPhase(d_t12, t12_phase);
	}

	if (t12_phase != IRPH_OFF && HAL_GetTick() > tilt_time) { // Time to redraw tilt switch status
		if (t12IdleMode()) {								// tilt switch is active
			tilt_time = HAL_GetTick() + tilt_show_time;
			ironT12Used(true);								// draw the 'iron used' icon
		} else if (tilt_time > 0) {
			tilt_time = 0;
			ironT12Used(false);								// clear the 'iron used' icon
		}
	}

	if (t12_phase == IRPH_LOWPWR && pCore->cfg.getLowTemp(d_t12) > 0) {
		uint32_t to = (t12_phase_end - HAL_GetTick()) / 1000;
		if (to < 100)
			pCore->dspl.timeToOff(devPos(d_t12), to);
	}

	if (jbc_phase == IRPH_COOLING && pJBC->isCold()) {
		pCore->buzz.lowBeep();
		jbc_phase = IRPH_COLD;
		jbc_phase_end = HAL_GetTick() + 20000;
		ironPhase(d_jbc, jbc_phase);
	}

	if (jbc_phase == IRPH_HEATING) {
		if (check_jbc_tm && HAL_GetTick() >= check_jbc_tm) {
			check_jbc_tm = 0;
			not_jbc = !pCore->jbc.isConnected();
			if (not_jbc) {
				pCore->jbc.switchPower(false);
				jbc_phase = IRPH_COOLING;
				ironPhase(d_jbc, jbc_phase);
				disableJBC();
			}
		} else {
			jbcReadyMode();									// Check the JBC IRON reaches the preset temperature
		}
	}

	if (jbc_phase == IRPH_LOWPWR && pCore->cfg.getLowTemp(d_jbc) > 0) {
		uint32_t to = (jbc_phase_end - HAL_GetTick()) / 1000;
		if (to < 100)
			pCore->dspl.timeToOff(devPos(d_jbc), to);
	}

	adjustPresetTemp();
	drawStatus(t12_phase, jbc_phase, ambient);
	return this;
}

// Check the hardware switches: REED switch of the Hot Air Gun and STANDBY switch of JBC iron. Returns True if the switch status changed
void MWORK::manageHardwareSwitches(CFG* pCFG, IRON *pT12, IRON *pJBC, HOTGUN *pHG) {
	bool no_t12 = no_ambient && !pT12->isConnected() && !is_extra_tip;	// The T12 iron handle is not connected flag: no ambient sensor, no current through the iron and no extra tip
	if (no_t12 != not_t12) {								// The T12 connection changed
		not_t12 = no_t12;
		if (not_t12) {
			disableT12();
		} else {
			enableT12();									// The T12 iron requires rotary encoder to be managed
		}
	}

	//  Manage the Hot Air Gun by The REED switch
	if (pHG->isReedSwitch(true)) {							// The Reed switch is open, switch the Hot Air Gun ON
		if (!pHG->isOn()) {
			uint16_t temp	= pCFG->tempPresetHuman(d_gun);
			temp 			= pCFG->humanToTemp(temp, ambient, d_gun);
			uint16_t fan	= pCFG->gunFanPreset();
			pHG->setTemp(temp);
			pHG->setFan(fan);
			pHG->switchPower(true);
			enableGUN();
			edit_temp		= true;
			return_to_temp	= 0;
			update_screen	= 0;
		}
	} else {												// The Reed switch is closed, switch the Hot Air Gun OFF
		if (pHG->isOn()) {
			uint8_t off_timeout = pCFG->getOffTimeout(d_gun);
			if (off_timeout) { 								// Put the JBC IRON to the low power mode
				uint16_t l_temp	= pCFG->getLowTemp(d_gun);
				int16_t temp	= pCore->cfg.tempPresetHuman(d_gun);
				if (l_temp >= temp)
					l_temp = temp - 10;
				temp = pCore->cfg.humanToTemp(l_temp, ambient, d_gun, true);
				pHG->lowPowerMode(temp);
				gun_switch_off = HAL_GetTick() + off_timeout * 60000;
				presetTemp(d_gun, l_temp);
				gunStandby();
			} else {										// Switch-off the JBC IRON immediately
				pHG->switchPower(false);
				disableGUN();
				pCFG->saveConfig();							// Save configuration when the Hot Air Gun is turned-off
				ironPhase(d_gun, IRPH_OFF);
			}
			update_screen	= 0;
		}
	}

	// Manage JBC IRON
	bool jbc_offhook = pJBC->isReedSwitch(true);			// The JBC IRON is off-hook
	if (jbc_offhook) {										// The JBC IRON is off-hook, try to activate JBC IRON
		if (!not_jbc) {										// Do not try to switch JBC iron on if it is not connected
			if (enableJBC()) {								// The dashboard mode changed
				check_jbc_tm = HAL_GetTick() + check_jbc_to; // When to check JBC current
				if (t12_phase == IRPH_NORMAL) {				// Force to re-establish the temperature of complementary device
					t12_phase = IRPH_HEATING;
					ironPhase(d_t12, t12_phase);
				}
			}
			uint16_t temp	= pCore->cfg.tempPresetHuman(d_jbc);
			if (!pJBC->isOn()) {							// The JBC IRON is not powered on, setup the preset temperature
				uint16_t temp_i = pCore->cfg.humanToTemp(temp, ambient, d_jbc);
				pCore->jbc.setTemp(temp_i);
				pJBC->switchPower(true);
				jbc_phase = IRPH_HEATING;
				ironPhase(d_jbc, jbc_phase);
				update_screen	= 0;
			} else if (jbc_phase == IRPH_LOWPWR) {			// The JBC IRON was on but in low power mode
				pJBC->switchPower(true);
				presetTemp(d_jbc, temp);					// Update the preset temperature
				jbc_phase = IRPH_HEATING;
				ironPhase(d_jbc, jbc_phase);
				update_screen	= 0;
			}
		}
	} else {												// The JBC IRON is on-hook, try to switch it OFF or go into low power mode
		if (pJBC->isOn() && isIronWorking(jbc_phase)) {
			uint8_t off_timeout = pCFG->getOffTimeout(d_jbc);
			if (off_timeout) { 								// Put the JBC IRON to the low power mode
				uint16_t l_temp	= pCFG->getLowTemp(d_jbc);
				int16_t temp	= pCore->cfg.tempPresetHuman(d_jbc);
				if (l_temp >= temp)
					l_temp = temp - 10;
				temp = pCore->cfg.humanToTemp(l_temp, ambient, d_jbc, true);
				pJBC->lowPowerMode(temp);
				jbc_phase_end = HAL_GetTick() + off_timeout * 60000;
				jbc_phase = IRPH_LOWPWR;
				ironPhase(d_jbc, jbc_phase);
				presetTemp(d_jbc, l_temp);
			} else {										// Switch-off the JBC IRON immediately
				pJBC->switchPower(false);
				jbc_phase = IRPH_COOLING;
				ironPhase(d_jbc, jbc_phase);
			}
			disableJBC();
			pCFG->saveConfig();								// Save configuration when the JBC IRON is turned-off
			update_screen	= 0;
		}
		not_jbc = false;									// Re-enable JBC iron
	}
}

void MWORK::adjustPresetTemp(void) {
	CFG*	pCFG	= &pCore->cfg;
	IRON*	pT12	= &pCore->t12;
	IRON*	pJBC	= &pCore->jbc;

	bool update_ambient = false;
	uint16_t presetTemp	= pT12->presetTemp();
	uint16_t tempH     	= pCFG->tempPresetHuman(d_t12);
	uint16_t temp  		= pCFG->humanToTemp(tempH, ambient, d_t12); // Expected temperature of IRON in internal units
	if (temp != presetTemp) {								// The ambient temperature have changed, we need to adjust preset temperature
		pT12->adjust(temp);
		update_ambient = true;
	}
	presetTemp	= pJBC->presetTemp();
	tempH     	= pCFG->tempPresetHuman(d_jbc);
	temp  		= pCFG->humanToTemp(tempH, ambient, d_jbc); // Expected temperature of IRON in internal units
	if (temp != presetTemp) {								// The ambient temperature have changed, we need to adjust preset temperature
		pJBC->adjust(temp);
		update_ambient = true;
	}
	if (update_ambient)
		pCore->dspl.drawAmbient(ambient, pCFG->isCelsius());
}

bool MWORK::hwTimeout(bool tilt_active) {
	CFG*	pCFG	= &pCore->cfg;

	uint32_t now_ms = HAL_GetTick();
	if (lowpower_time == 0 || tilt_active) {				// If the IRON is used, reset standby time
		lowpower_time = now_ms + pCFG->getLowTO() * 5000;	// Convert timeout (5 secs interval) to milliseconds
	}
	if (now_ms >= lowpower_time) {
		return true;
	}
	return false;
}

// Use applied power analysis to automatically power-off the IRON
void MWORK::swTimeout(uint16_t temp, uint16_t temp_set, uint16_t temp_setH, uint32_t td, uint32_t pd, uint16_t ap) {
	CFG*	pCFG	= &pCore->cfg;

	int ip = idle_pwr.read();
	if ((temp <= temp_set) && (temp_set - temp <= 4) && (td <= 200) && (pd <= 25)) {
		// Evaluate the average power in the idle state
		ip = idle_pwr.average(ap);
	}

	// Check the IRON current status: idle or used
	if (abs(ap - ip) >= 150) {								// The applied power is different than idle power. The IRON being used!
		swoff_time 		= HAL_GetTick() + pCFG->getOffTimeout(d_t12) * 60000;
		t12_phase = IRPH_NORMAL;
		ironPhase(d_t12, t12_phase);
	} else {												// The IRON is in its idle state
		if (swoff_time == 0)
			swoff_time 	= HAL_GetTick() + pCFG->getOffTimeout(d_t12) * 60000;
		uint32_t to = (swoff_time - HAL_GetTick()) / 1000;
		if (to < 100) {
			pCore->dspl.timeToOff(devPos(d_t12), to);
		} else {
			ironPhase(d_t12, IRPH_GOINGOFF);
		}
	}
}

void MWORK::t12PhaseEnd(void) {
	uint16_t t	= pCore->t12.presetTemp();
	t	= pCore->cfg.tempToHuman(t, ambient, d_t12);

	switch (t12_phase) {
		case IRPH_READY:
			t12_phase = IRPH_NORMAL;
			break;
		case IRPH_BOOST:
			pCore->t12.switchPower(true);
			t12_phase	= IRPH_HEATING;
			pCore->buzz.lowBeep();
			presetTemp(d_t12, t);							// redraw actual temperature
			break;
		case IRPH_LOWPWR:
		case IRPH_GOINGOFF:
			t12_phase = IRPH_COOLING;
			pCore->t12.switchPower(false);
			presetTemp(d_t12, t); 							// redraw actual temperature
			pCore->cfg.saveConfig();						// Save configuration when the T12 IRON is turned-off
			break;
		case IRPH_COLD:
			t12_phase = IRPH_OFF;
			break;
		default:
			break;
	}
	ironPhase(d_t12, t12_phase);
	t12_phase_end = 0;
}

void MWORK::jbcPhaseEnd(void) {
	uint16_t t	= pCore->jbc.presetTemp();
	t	= pCore->cfg.tempToHuman(t, ambient, d_jbc);

	switch (jbc_phase) {
		case IRPH_READY:
			jbc_phase = IRPH_NORMAL;
			break;
		case IRPH_LOWPWR:									// The JBC IRON was on-hook for a while
			jbc_phase = IRPH_COOLING;
			pCore->buzz.shortBeep();
			pCore->jbc.switchPower(false);
			presetTemp(d_jbc, t);							// redraw actual temperature
			pCore->cfg.saveConfig();						// Save configuration when the JBC IRON is turned-off
			break;
		case IRPH_COLD:
			jbc_phase = IRPH_OFF;
			break;
		default:
			break;
	}
	ironPhase(d_jbc, jbc_phase);
	jbc_phase_end = 0;
}

bool MWORK::t12IdleMode(void) {
	IRON*	pIron		= &pCore->t12;
	int temp			= pIron->averageTemp();
	int temp_set		= pIron->presetTemp();				// Now the preset temperature in internal units!!!
	uint16_t temp_set_h = pCore->cfg.tempPresetHuman(d_t12);

	uint32_t td			= pIron->tmpDispersion();			// The temperature dispersion
	uint32_t pd 		= pIron->pwrDispersion();			// The power dispersion
	int ap      		= pIron->avgPower();				// Actually applied power to the IRON

	// Check the IRON reaches the preset temperature
	if ((abs(temp_set - temp) < 6) && (td <= 500) && (ap > 0))  {
	    if (t12_phase == IRPH_HEATING) {
	    	t12_phase = IRPH_READY;
			t12_phase_end = HAL_GetTick() + 2000;
	    	pCore->buzz.shortBeep();
	    	ironPhase(d_t12, t12_phase);
	    }
	}

	bool low_power_enabled = pCore->cfg.getLowTemp(d_t12) > 0;
	bool tilt_active = false;
	if (low_power_enabled)									// If low power mode enabled, check tilt switch status
		tilt_active = pIron->isReedSwitch(pCore->cfg.isReedType());	// True if iron was used

	// If the low power mode is enabled, check the IRON status
	if (t12_phase == IRPH_NORMAL) {							// The IRON has reaches the preset temperature and 'Ready' message is already cleared
		if (low_power_enabled) {							// Use hardware tilt switch if low power mode enabled
			if (hwTimeout(tilt_active)) {					// Time to activate low power mode
				uint16_t l_temp	= pCore->cfg.getLowTemp(d_t12);
				int16_t temp	= pCore->cfg.tempPresetHuman(d_t12);
				if (l_temp >= temp)
					l_temp = temp - 10;
				temp = pCore->cfg.humanToTemp(l_temp, ambient, d_t12, true);
				pCore->t12.lowPowerMode(temp);
				t12_phase 		= IRPH_LOWPWR;				// Switch to low power mode
				ironPhase(d_t12, t12_phase);
				presetTemp(d_t12, l_temp);
				t12_phase_end	= HAL_GetTick() + pCore->cfg.getOffTimeout(d_t12) * 60000;
			}
		} else if (pCore->cfg.getOffTimeout(d_t12) > 0) {	// Do not use tilt switch, use software auto-off feature
			swTimeout(temp, temp_set, temp_set_h, td, pd, ap); // Update time_to_return value based IRON status
		}
	} else if (t12_phase == IRPH_LOWPWR && tilt_active) {	// Re-activate the IRON in normal mode
		pCore->t12.switchPower(true);
		t12_phase = IRPH_HEATING;
		uint16_t t_set = pCore->t12.presetTemp();
		t_set = pCore->cfg.tempToHuman(t_set, ambient, d_t12);
		ironPhase(d_t12, t12_phase);
		presetTemp(d_t12, t_set);							// Redraw the preset temperature
		lowpower_time = 0;									// Reset the low power mode timeout
	}
	return tilt_active;
}

void MWORK::jbcReadyMode(void) {
	IRON*	pIron		= &pCore->jbc;
	int temp			= pIron->averageTemp();
	int temp_set		= pIron->presetTemp();				// Now the preset temperature in internal units!!!
	uint32_t td			= pIron->tmpDispersion();			// The temperature dispersion
	int ap      		= pIron->avgPower();				// Actually applied power to the IRON

	// Check the IRON reaches the preset temperature
	if ((abs(temp_set - temp) < 6) && (td <= 500) && (ap > 0))  {
		jbc_phase = IRPH_READY;
		jbc_phase_end 	= HAL_GetTick() + 2000;
		pCore->buzz.shortBeep();
		ironPhase(d_jbc, jbc_phase);
	}
}

bool MWORK::manageEncoders(void) {
	HOTGUN 	*pHG		= &pCore->hotgun;
	CFG		*pCFG		= &pCore->cfg;

	uint16_t temp_set_h = pCore->u_enc.read();
    uint8_t  button		= pCore->u_enc.buttonStatus();
    if (button == 1) {										// The upper encoder button pressed shortly, change the working mode
    	if (u_dev == d_t12) {
    		t12PressShort();
    		lowpower_time = 0;
    	} else if (u_dev == d_jbc) {
    		jbcPressShort();
    	}
    	update_screen = 0;
    } else if (button == 2) {								// The upper encoder button was pressed for a long time
    	if (u_dev == d_t12) {
    		t12PressLong();
    		lowpower_time = 0;
    	}
    	update_screen = 0;
    }

    if (pCore->u_enc.changed()) {							// The IRON preset temperature changed
    	if (u_dev == d_t12) {
        	if (t12Rotate(temp_set_h)) {					// The t12 preset temperature has been changed
        		// Update the preset temperature in memory only. To save config to the flash, use saveConfig()
				pCFG->savePresetTempHuman(temp_set_h, d_t12);
				idle_pwr.reset();
				presetTemp(u_dev, temp_set_h);
			}
    	} else {
    		if (jbcRotate(temp_set_h)) {					// The jbc preset temperature has been changed
    			// Update the preset temperature in memory only. To save config to the flash, use saveConfig()
    			pCFG->savePresetTempHuman(temp_set_h, d_jbc);
    			presetTemp(u_dev, temp_set_h);
    		}
    	}
    	update_screen = 0;
    }

    temp_set_h		= pCore->l_enc.read();
    button			= pCore->l_enc.buttonStatus();
	if (button == 1) {
		if (l_dev == d_t12) {								// Manage T12 IRON
			t12PressShort();
			update_screen = 0;
			lowpower_time = 0;
		} else if (l_dev == d_gun) {						// Short press
			if (gun_switch_off > 0) {						// The Hot Air Gun is in standby mode, turn-off the Gun
				gun_switch_off = HAL_GetTick();
				return false;
			}
			// the HOT AIR GUN button was pressed, toggle temp/fan
			if (edit_temp) {								// Switch to edit fan speed
				uint16_t fan 	= pHG->presetFan();
				uint16_t min	= pHG->minFanSpeed();
				uint16_t max 	= pHG->maxFanSpeed();
				uint8_t	 step	= pHG->fanStepPcnt();
				pCore->l_enc.reset(fan, min, max, step, step<<2, false);
				edit_temp 		= false;
				temp_set_h		= fan;
				return_to_temp	= HAL_GetTick() + edit_fan_timeout;
				fanSpeed(true);
				update_screen 	= 0;
			} else {
				return_to_temp	= HAL_GetTick();			// Force to return to edit temperature
				return false;
			}
		}
	} else if (button == 2) {								// No BOOST mode for T12 in this case, Go to the main menu
		return true; 
	}

    if (pCore->l_enc.changed()) {
    	if (l_dev == d_t12) {
        	if (t12Rotate(temp_set_h)) {					// The t12 preset temperature has been changed
				pCFG->savePresetTempHuman(temp_set_h, d_t12);
				presetTemp(l_dev, temp_set_h);
				idle_pwr.reset();
			}
    	} else {											// Changed preset temperature or fan speed
			uint16_t g_temp = temp_set_h;					// In first loop the preset temperature will be setup for sure
			uint16_t t	= pHG->presetTemp();				// Internal units
			uint16_t f	= pHG->presetFan();
			t = pCFG->tempToHuman(t, ambient, d_gun);
			if (edit_temp) {
				t = temp_set_h;								// New temperature value
				presetTemp(l_dev, temp_set_h);
				uint16_t g_temp_set	= pCFG->humanToTemp(g_temp, ambient, d_gun);
				pHG->setTemp(g_temp_set);
			} else {
				f = temp_set_h;								// New fan value
				pHG->setFan(f);
				fanSpeed(true);
				return_to_temp	= HAL_GetTick() + edit_fan_timeout;
			}
			pCFG->saveGunPreset(t, f);
    	}
    }

    // The fan speed modification mode has 'return_to_temp' timeout
	if (return_to_temp && HAL_GetTick() >= return_to_temp) {// This reads the Hot Air Gun configuration Also
		// The temperature values returned in Celsius or Fahrenheit depending on the user preferences
		uint16_t g_temp		= pCFG->tempPresetHuman(d_gun);
		uint16_t t_min		= pCFG->tempMin(d_gun);			// The minimum preset temperature
		uint16_t t_max		= pCFG->tempMax(d_gun);			// The maximum preset temperature
		uint8_t temp_step = 1;
		if (pCFG->isBigTempStep()) {						// The preset temperature step is 5 degrees
			g_temp -= g_temp % 5;							// The preset temperature should be rounded to 5
			temp_step = 5;
		}
		pCore->l_enc.reset(g_temp, t_min, t_max, temp_step, temp_step, false);
		edit_temp		= true;
		fanSpeed(false);									// Redraw in standard mode
		return_to_temp	= 0;
	}
	return false;
}

// The T12 IRON encoder button short press callback
void MWORK::t12PressShort(void) {
	switch (t12_phase) {
		case IRPH_OFF:										// The IRON is powered OFF, switch it ON
		case IRPH_COLD:
			if (no_ambient && !is_extra_tip) {				// The IRON handle is not connected
				pCore->buzz.failedBeep();
				return;
			}
		case IRPH_COOLING:
		{
			uint16_t temp	= pCore->cfg.tempPresetHuman(d_t12);
			ambient 		= pCore->ambientTemp();
			temp 			= pCore->cfg.humanToTemp(temp, ambient, d_t12);
			pCore->t12.setTemp(temp);
			pCore->t12.switchPower(true);
			t12_phase	= IRPH_HEATING;
			ironPhase(d_t12, t12_phase);
		}
			break;
		default:											// Switch off the IRON
			pCore->t12.switchPower(false);
			t12_phase	= IRPH_COOLING;
			ironPhase(d_t12, t12_phase);
			pCore->cfg.saveConfig();						// Save configuration when the T12 IRON is turned-off
			presetTemp(d_t12, pCore->cfg.tempPresetHuman(d_t12));
			break;
	}
}

// The T12 IRON encoder button long press callback
void  MWORK::t12PressLong(void) {
	switch (t12_phase) {
		case IRPH_OFF:
		case IRPH_COLD:
			if (no_ambient && !is_extra_tip) {				// The IRON handle is not connected
				pCore->buzz.failedBeep();
				return;
			}
		case IRPH_COOLING:
			pCore->buzz.shortBeep();
			pCore->t12.switchPower(true);
			t12_phase	= IRPH_HEATING;
			ironPhase(d_t12, t12_phase);
			break;
		case IRPH_BOOST:									// The IRON is in the boost mode, return to the normal mode
			pCore->t12.switchPower(true);
			t12_phase		= IRPH_HEATING;
			t12_phase_end	= 0;
			ironPhase(d_t12, t12_phase);
			presetTemp(d_t12, pCore->cfg.tempPresetHuman(d_t12));
			pCore->buzz.shortBeep();
			break;
		default:											// The IRON is working, go to the BOOST mode
		{
			uint8_t  bt		= pCore->cfg.boostTemp();		// Additional temperature (Degree)
			uint32_t bd		= pCore->cfg.boostDuration();	// Boost duration (s)
			if (bt > 0 && bd > 0) {
				if (!pCore->cfg.isCelsius())
					bt = (bt * 9 + 3) / 5;
				uint16_t tset		= pCore->t12.presetTemp();	// Current preset temperature, internal units
				uint16_t l_temp		= pCore->cfg.tempToHuman(tset, ambient, d_t12) + bt;
				tset				= pCore->cfg.humanToTemp(l_temp, ambient, d_t12);
				pCore->t12.boostPowerMode(tset);
				t12_phase = IRPH_BOOST;
				t12_phase_end	= HAL_GetTick() + (uint32_t)bd * 1000;
				ironPhase(d_t12, t12_phase);
				presetTemp(d_t12, l_temp);
				pCore->buzz.shortBeep();
			}
		}
			break;
	}
}

// The T12 IRON encoder rotated callback, returns true if the temperature value should be altered
// If true, the preset temperature will be updated in T12 config and on the screen
bool MWORK::t12Rotate(uint16_t new_value) {
	switch (t12_phase) {
		case IRPH_BOOST:
			return false;
		case IRPH_OFF:
		case IRPH_COLD:
		case IRPH_COOLING:
			return true;
		case IRPH_LOWPWR:
		case IRPH_GOINGOFF:
			pCore->t12.switchPower(true);
			t12_phase = IRPH_HEATING;
			ironPhase(d_t12, t12_phase);
			return false;
		case IRPH_HEATING:
		case IRPH_READY:
		case IRPH_NORMAL:
		default:
		{
			uint16_t temp = pCore->cfg.humanToTemp(new_value, ambient, d_t12);
			pCore->t12.setTemp(temp);
			t12_phase = IRPH_HEATING;
			return true;
		}
	}
	return false;
}

// The JBC IRON encoder button short press callback
void MWORK::jbcPressShort(void) {
	switch (jbc_phase) {
		case IRPH_LOWPWR:									// Switch off the IRON
		{
			pCore->jbc.switchPower(false);
			jbc_phase = IRPH_COOLING;
			if (not_t12 || !pCore->hotgun.isOn()) {
				uint16_t temp	= pCore->cfg.tempPresetHuman(d_jbc);
				presetTemp(d_jbc, temp);
				ironPhase(d_jbc, jbc_phase);
			} else {
				setMode(DM_T12_GUN);
			}
			break;
		}
		default:
			break;
	}
}

//IRPH_HEATING, IRPH_READY, IRPH_NORMAL,
//

// The JBC IRON encoder rotated callback, returns true if the temperature value should be altered
// If true, the preset temperature will be updated in JBC config and on the screen
bool MWORK::jbcRotate(uint16_t new_value) {
	switch (jbc_phase) {
		case IRPH_BOOST:
		case IRPH_LOWPWR:
		case IRPH_GOINGOFF:
			return false;
		case IRPH_OFF:
		case IRPH_COLD:
		case IRPH_COOLING:
			return true;
		case IRPH_HEATING:
		case IRPH_READY:
		case IRPH_NORMAL:
		default:
		{
			uint16_t temp = pCore->cfg.humanToTemp(new_value, ambient, d_jbc);
			pCore->jbc.setTemp(temp);
			jbc_phase = IRPH_HEATING;
			return true;
		}
	}
	return false;
}

bool MWORK::isIronCold(tIronPhase phase) {
	return (phase == IRPH_OFF || phase == IRPH_COOLING || phase == IRPH_COLD);
}

bool MWORK::isIronWorking(tIronPhase phase) {
	return (phase == IRPH_HEATING || phase == IRPH_READY || phase == IRPH_NORMAL);
}
