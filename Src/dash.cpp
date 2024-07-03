/*
 * dash.cpp
 *   Author: Alex
 *
 * 2023 MAR 01, v1.01
 *	Modified the DASH::drawStatus(), DASH::showOffTimeout(), DASH::initEncoders()
 * 2024 JUN 28, v.1.04
 *     Modified methods dealing with main screen for convenience
 *     DASH::enableJBC()
 *     DASH::disableJBC()
 *     DASH::enableGUN()
 *     DASH::disableGUN()
 *     DASH::enableT12()
 *     DASH::disableT12()
 */

#include "dash.h"
#include "display.h"
#include "unit.h"

void DASH::init(void) {
	fan_animate		= 0;
	fan_blowing		= !pCore->hotgun.isCold();
	is_extra_tip	= pCore->cfg.isExtraTip();
	no_ambient		= pCore->noAmbientSensor();
	not_t12 		= no_ambient && !pCore->t12.isConnected() && !is_extra_tip;
	not_jbc			= false;
	if (not_t12) {
		u_dev	= d_jbc;
		l_dev	= d_gun;
		h_dev	= d_unknown;
	}
}

void DASH::drawStatus(tIronPhase t12_phase, tIronPhase jbc_phase, int16_t ambient) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;

	tIronPhase u_phase = (u_dev == d_t12)?t12_phase:jbc_phase;
	tIronPhase l_phase = (l_dev == d_t12)?t12_phase:IRPH_OFF;

	// Get parameters of the upper device
	UNIT*	pUU 		= (UNIT*)(u_dev == d_t12)?&pCore->t12:&pCore->jbc; // Select the upper IRON unit accordingly
	uint16_t temp  		= pUU->averageTemp();
	uint16_t u_temp_h 	= pCFG->tempToHuman(temp, ambient, u_dev);
	temp				= pUU->presetTemp();
	uint16_t u_temp_s	= pCFG->tempToHuman(temp, ambient, u_dev);
	if (u_phase == IRPH_LOWPWR) {
		u_temp_s		= pCFG->getLowTemp(u_dev);
	}
	uint8_t	 u_pwr		= pUU->avgPowerPcnt();

	// Get Parameters of the lower device
	UNIT *pLU 			= (UNIT*)(l_dev == d_t12)?&pCore->t12:(UNIT*)&pCore->hotgun; // Select the lower device accordingly
	temp				= pLU->averageTemp();
	uint16_t l_temp_h	= pCFG->tempToHuman(temp, ambient, l_dev);
	temp				= pLU->presetTemp();
	uint16_t l_temp_s	= pCFG->tempToHuman(temp, ambient, l_dev);
	if (l_phase == IRPH_LOWPWR) {
		u_temp_s		= pCFG->getLowTemp(l_dev);
	}
	uint8_t	 l_pwr		= pLU->avgPowerPcnt();

	bool celsius = pCFG->isCelsius();

	// Draw status of upper device
	bool no_iron = (u_dev == d_t12)?not_t12:not_jbc;
	bool iron_on = (u_phase != IRPH_OFF && u_phase != IRPH_COLD);
	pD->drawTempGauge(u_temp_h-u_temp_s, u_upper, iron_on);
	pD->drawPower(u_pwr, u_upper);
	// Show the IRON temperature 
	if (u_phase == IRPH_HEATING) {
		pD->animatePower(u_upper, u_temp_h - u_temp_s);		// Draw colored power icon
	} else if (no_iron || u_phase == IRPH_OFF) {			// Prevent the temperature changing when the IRON is OFF
		u_temp_h = ambient;
	}
	if (u_phase == IRPH_COOLING) {
		pD->animateTempCooling(u_temp_h, celsius, u_upper);
	} else {
		pD->drawTemp(u_temp_h, u_upper);
	}
	// Correct the preset tempeature depending on current IRON mode
	if (u_phase == IRPH_LOWPWR) {
		uint16_t l_temp	= pCore->cfg.getLowTemp(u_dev);
		if (l_temp > 0)
			u_temp_s = l_temp;
	} else if (u_dev == d_t12 && u_phase == IRPH_BOOST) {
		uint8_t  bt		= pCore->cfg.boostTemp();			// Additional temperature (Degree)
		if (!pCore->cfg.isCelsius())
			bt = (bt * 9 + 3) / 5;
		uint16_t tset = pCore->t12.presetTemp();			// Current preset temperature, internal units
		u_temp_s = pCore->cfg.tempToHuman(tset, ambient, d_t12) + bt;
	}

	// Draw the status of the lower device
	no_iron = (l_dev == d_t12) && not_t12;
	iron_on = (l_phase != IRPH_OFF && l_phase != IRPH_COLD);
	if (l_dev == d_t12) {
		if (l_phase == IRPH_HEATING) {
			pD->animatePower(u_lower, l_temp_h - l_temp_s);
		} else if (no_iron || l_phase == IRPH_OFF) {			// Prevent the temperature changing when the IRON or Hot Gun is off and cold
			l_temp_h = ambient;
		}

		if (l_phase == IRPH_COOLING) {
			pD->animateTempCooling(l_temp_h, celsius, u_lower);
		} else {
			pD->drawTemp(l_temp_h, u_lower);
		}
		if (u_phase == IRPH_LOWPWR) {
			uint16_t l_temp	= pCore->cfg.getLowTemp(l_dev);
			if (l_temp > 0)
				u_temp_s = l_temp;
		}
		pD->drawTempGauge(l_temp_h-l_temp_s, u_lower, iron_on);
		pD->drawPower(l_pwr, u_lower);
	} else {													// The lower device is the Hot Air Gun
		if (!pCore->hotgun.isFanWorking()) {
			if (fan_blowing) {
				pD->stopFan();
				fan_blowing = false;
				pD->msgOFF(u_lower);
			}
			l_temp_h = ambient;
		} else {
			fan_blowing	= true;
			if (pCore->hotgun.isOn()) {
				pD->animatePower(u_lower, l_temp_h - l_temp_s);
			}
		}

		if (fan_blowing && !pCore->hotgun.isOn()) {
			pD->animateTempCooling(l_temp_h, celsius, u_lower);
		} else {
			pD->drawTemp(l_temp_h, u_lower);
		}
		pD->drawTempGauge(l_temp_h-l_temp_s, u_lower, fan_blowing);
		pD->drawPower(l_pwr, u_lower);
	}

	// Show the temperature of the alternative device (if it is hot)
	if (h_dev == d_unknown) {
		pD->drawAlternate(0, false, h_dev);				// Clear the area
		return;
	}

	tIronPhase	h_phase	= t12_phase;
	UNIT*	pHU			= (UNIT*)&pCore->t12;
	bool h_dev_active 	= !pCore->t12.isCold();
	if (h_dev == d_jbc) {
		pHU	= &pCore->jbc;
		h_phase = jbc_phase;
		h_dev_active = !pCore->jbc.isCold();
	} else if (h_dev == d_gun){
		pHU = &pCore->hotgun;
		h_phase = pHU->isOn()?IRPH_NORMAL:IRPH_COOLING;
		h_dev_active = pCore->hotgun.isFanWorking();
	}
	if (h_dev_active) {
		uint16_t temp = pHU->averageTemp();
		temp = pCFG->tempToHuman(temp, ambient, h_dev);
		pD->drawAlternate(temp, h_phase != IRPH_COOLING, h_dev);
	} else {
		pD->drawAlternate(0, false, h_dev);				// Clear area
	}
}

void DASH::animateFan(void) {
	HOTGUN *pHG = &pCore->hotgun;
	if (l_dev == d_gun && pHG->isFanWorking() && HAL_GetTick() >= fan_animate && pHG->isConnected()) {
		int16_t  temp		= pHG->averageTemp();
		int16_t  temp_s		= pHG->presetTemp();
		pCore->dspl.animateFan(temp-temp_s);
		fan_animate = HAL_GetTick() + 100;
	}
}

void DASH::ironT12Used(bool active) {
	tUnitPos pos = devPos(d_t12);
	pCore->dspl.ironActive(active, pos);
}

bool DASH::enableJBC(void) {
	if (u_dev == d_jbc)
		return false;
	if (not_t12 || pCore->hotgun.isOn())
		return setMode(DM_JBC_GUN);
	return setMode(DM_JBC_T12);
}

bool DASH::disableJBC(void) {
	if (u_dev != d_jbc)
		return false;
	if (not_t12)
		return setMode(DM_JBC_GUN);
	if (pCore->jbc.isOn())
		return setMode(DM_JBC_T12);
	return setMode(DM_T12_GUN);
}

bool DASH::enableGUN(void) {
	if (l_dev == d_gun)
		return false;
	if (not_jbc)
		return setMode(DM_T12_GUN);
	if (not_t12)
		return setMode(DM_JBC_GUN);
	if (pCore->jbc.isOn())
		return setMode(DM_JBC_GUN);
	return setMode(DM_T12_GUN);
}

bool DASH::disableGUN(void) {
	if (l_dev != d_gun)
		return false;
	if (not_jbc)
		return setMode(DM_T12_GUN);
	if (not_t12)
		return setMode(DM_JBC_GUN);
	if (pCore->jbc.isOn())
		return setMode(DM_JBC_T12);
	return setMode(DM_T12_GUN);
}

bool DASH::enableT12(void) {
	if (u_dev == d_t12 || l_dev == d_t12)
		return false;
	if (not_jbc)
		return setMode(DM_T12_GUN);
	if (pCore->jbc.isOn())
		return setMode(DM_JBC_T12);
	return setMode(DM_T12_GUN);
}

bool DASH::disableT12(void) {
	if (u_dev == d_t12)
		return setMode(DM_JBC_GUN);
	if (l_dev == d_t12)
		return setMode(DM_JBC_GUN);
	if (h_dev == d_t12)
		h_dev = d_unknown;
	return false;
}

bool DASH::setMode(tDashMode dm) {
	tDevice u_dev_n = d_t12;
	tDevice l_dev_n = d_gun;
	h_dev = d_jbc;
	not_t12 = no_ambient && !pCore->t12.isConnected() && !is_extra_tip;

	switch (dm) {
		case DM_JBC_T12:
			u_dev_n	= d_jbc;
			l_dev_n = d_t12;
			h_dev 	= d_gun;
			break;
		case DM_JBC_GUN:
			u_dev_n = d_jbc;
			l_dev_n = d_gun;
			h_dev 	= (not_t12)?d_unknown:d_t12;
			break;
		case DM_T12_GUN:
		default:
			break;
	}
	bool init_upper = (u_dev_n != u_dev);
	bool init_lower = (l_dev_n != l_dev);
	u_dev = u_dev_n;
	l_dev = l_dev_n;
	return initDevices(init_upper, init_lower);
}

bool DASH::initDevices(bool init_upper, bool init_lower) {
	DSPL 	*pD 	= &pCore->dspl;
	CFG		*pCFG 	= &pCore->cfg;
	bool mode_changed = false;
	uint16_t u_preset = init_upper?pCFG->tempPresetHuman(u_dev):0;
	uint16_t l_preset = init_lower?pCFG->tempPresetHuman(l_dev):0;
	initEncoders(u_dev, l_dev, u_preset, l_preset);
	if (init_upper) {
		mode_changed = true;
		pD->drawTipName(pCFG->tipName(u_dev), pCFG->isTipCalibrated(u_dev), u_upper);
		pD->drawTempSet(u_preset, u_upper);
		pD->ironActive(false, u_upper);
		// Draw new device power status icon
		bool no_t12 = u_dev == d_t12 && no_ambient && !is_extra_tip;
		tIronPhase phase = (u_dev == d_t12)?t12_phase:jbc_phase;
		if (no_t12 || phase == IRPH_OFF || phase == IRPH_COOLING) {
			pD->msgOFF(u_upper);
		} else if (phase == IRPH_LOWPWR) {
			pD->msgStandby(u_upper);
		} else if (phase == IRPH_NORMAL) {
			pD->msgNormal(u_upper);
		}
	}
	if (init_lower) {
		mode_changed = true;
		if (l_dev == d_gun) {
			int16_t ambient	= pCore->ambientTemp();
			bool celsius 	= pCore->cfg.isCelsius();
			pD->drawFanPcnt(pCore->hotgun.presetFanPcnt());
			pD->drawAmbient(ambient, celsius);
			pD->stopFan();
			if (pCore->hotgun.isOn())
				pD->msgON(u_lower);
			else
				pD->msgOFF(u_lower);
		} else {											// l_dev is d_t12
			pD->drawTipName(pCFG->tipName(l_dev), pCFG->isTipCalibrated(l_dev), u_lower);
			pD->noFan();
			bool no_t12 = no_ambient && !is_extra_tip;
			if (no_t12 || t12_phase == IRPH_OFF || t12_phase == IRPH_COOLING) {
				pD->msgOFF(u_lower);
			} else if (t12_phase == IRPH_LOWPWR) {
				pD->msgStandby(u_upper);
			} else if (t12_phase == IRPH_NORMAL) {
				pD->msgNormal(u_lower);
			}
		}
		pD->drawTempSet(l_preset, u_lower);
		fan_blowing		= false;
		fan_animate		= 0;
	}
	return mode_changed;
}

void DASH::initEncoders(tDevice u_dev, tDevice l_dev, int16_t u_value, uint16_t l_value) {
	CFG*	pCFG	= &pCore->cfg;

	bool	celsius 	= pCFG->isCelsius();
	uint16_t it_min		= pCFG->tempMinC(d_t12);			// The minimum IRON preset temperature, defined in main.h
	uint16_t it_max		= pCFG->tempMaxC(d_t12);			// The maximum IRON preset temperature
	uint16_t gt_min		= pCFG->tempMinC(d_gun);			// The minimum GUN preset temperature, defined in main.h
	uint16_t gt_max		= pCFG->tempMaxC(d_gun);			// The maximum GUN preset temperature
	if (!celsius) {											// The preset temperature saved in selected units
		it_min	= celsiusToFahrenheit(it_min);
		it_max	= celsiusToFahrenheit(it_max);
		gt_min	= celsiusToFahrenheit(gt_min);
		gt_max	= celsiusToFahrenheit(gt_max);
	}
	uint8_t temp_step = 1;
	if (pCFG->isBigTempStep()) {							// The preset temperature step is 5 degrees
		temp_step = 5;
		u_value -= u_value % 5;								// The preset temperature should be rounded to 5
		l_value -= l_value % 5;
	}

	if (u_value)											// The upper device can be T12 or JBC. It cannot manage the Hot Air Gun
		pCore->u_enc.reset(u_value, it_min, it_max, temp_step, temp_step, false);
	if (l_value)
		pCore->l_enc.reset(l_value, (l_dev == d_gun)?gt_min:it_min, (l_dev == d_gun)?gt_max:it_max, temp_step, temp_step, false);
}

tUnitPos DASH::devPos(tDevice dev) {
	if (dev == u_dev)
		return u_upper;
	if (dev == l_dev)
		return u_lower;
	return u_none;
}

void DASH::ironPhase(tDevice dev, tIronPhase phase) {
	tUnitPos pos = devPos(dev);
	if (pos == u_none)
		return;
	DSPL 	*pD 	= &pCore->dspl;

	switch (phase) {
		case IRPH_HEATING:
			pD->msgON(pos);
			break;
		case IRPH_READY:
			pD->msgReady(pos);
			break;
		case IRPH_NORMAL:
			pD->msgNormal(pos);
			break;
		case IRPH_BOOST:
			pD->msgBoost(pos);
		case IRPH_LOWPWR:
			pD->msgStandby(pos);
			break;
		case IRPH_GOINGOFF:
			pD->msgIdle(pos);
			break;
		case IRPH_COLD:
			pD->msgCold(pos);
			break;
		case IRPH_OFF:
		case IRPH_COOLING:
		default:
			pD->msgOFF(pos);
			break;
	}
}

void DASH::presetTemp(tDevice dev, uint16_t temp) {
	tUnitPos pos = devPos(dev);
	if (pos == u_none)
		return;
	pCore->dspl.drawTempSet(temp, pos);
}

void DASH::fanSpeed(bool modify) {
	if (l_dev == d_gun)
		pCore->dspl.drawFanPcnt(pCore->hotgun.presetFanPcnt(), modify);
}
