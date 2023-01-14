/*
 * mode.cpp
 *
 * Dec 20 2022
 *     Added restore_power_ms to the MCALIB_MANUAL::loop() method to stop powering the iron when you try to decrease the preset temperature
 *
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mode.h"
#include "cfgtypes.h"
#include "core.h"
#include "unit.h"

//---------------------- The Menu mode -------------------------------------------
void MODE::setup(MODE* return_mode, MODE* short_mode, MODE* long_mode) {
	mode_return	= return_mode;
	mode_spress	= short_mode;
	mode_lpress	= long_mode;
}

MODE* MODE::returnToMain(void) {
	if (mode_return && time_to_return && HAL_GetTick() >= time_to_return)
		return mode_return;
	return this;
}

void MODE::resetTimeout(void) {
	if (timeout_secs) {
		time_to_return = HAL_GetTick() + timeout_secs * 1000;
	}
}
void MODE::setTimeout(uint16_t t) {
	timeout_secs = t;
}

UNIT* MODE::unit(void) {
	UNIT*	pUnit	= 0;
	switch (dev_type) {
		case d_t12:
			pUnit = (UNIT*)&pCore->t12;
			break;
		case d_jbc:
			pUnit = &pCore->jbc;
			break;
		case d_gun:
		default:
			pUnit = (UNIT*)&pCore->hotgun;
			break;
	}
	return pUnit;
}

//---------------------- The tip selection mode ----------------------------------
void MSLCT::init(void) {;
	CFG*	pCFG	= &pCore->cfg;

	manual_change		= false;
	tip_disconnected	= 0;
	if (dev_type == d_unknown) {							// Manual TIP selection mode
		manual_change = true;
		dev_type = d_t12;
	}
	if (!manual_change) {
		if (dev_type == d_t12) {
			pCore->t12.setCheckPeriod(3);					// Start checking the current through the T12 IRON rapidly
			tip_disconnected  = HAL_GetTick();				// As soon as the current would detected, the mode can be finished
		} else {
			pCore->jbc.setCheckPeriod(3);					// Start checking the current through the JBC IRON
		}
	}
	uint8_t tip_index 	= pCFG->currentTipIndex(dev_type);
	// Build list of the active tips; The current tip is the second element in the list
	uint8_t list_len 	= pCFG->tipList(tip_index, tip_list, MSLCT_LEN, true, dev_type);

	// The current tip could be inactive, so we should find nearest tip (by ID) in the list
	uint8_t closest		= 0;								// The index of the list of closest tip ID
	uint8_t diff  		= 0xff;
	for (uint8_t i = 0; i < list_len; ++i) {
		uint8_t delta;
		if ((delta = abs(tip_index - tip_list[i].tip_index)) < diff) {
			diff 	= delta;
			closest = i;
		}
	}
	pCore->l_enc.reset(closest, 0, list_len-1, 1, 1, false);
	tip_begin_select = HAL_GetTick();						// We stared the tip selection procedure
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_SELECT_TIP);
	update_screen	= 0;									// Force to redraw the screen
}

MODE* MSLCT::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;
	UNIT*	pUnit	= unit();								// Unit can be Hakko T12 or JBC IRON

	uint8_t	 index	= pEnc->read();
	uint8_t	button	= pEnc->buttonStatus();

	if (pEnc->changed()) {
		tip_begin_select 	= 0;
		update_screen 		= 0;
	}

	if (button > 0 && (manual_change || dev_type != d_t12)) { // The button was pressed
		changeTip(index);
		return mode_return;
	}

	if (!manual_change && tip_disconnected > 0 && (pUnit->isConnected() || !isACsine())) {	// See core.cpp for isACsine()
		// Prevent bouncing event, when the IRON connection restored back too quickly.
		if (tip_begin_select && (HAL_GetTick() - tip_begin_select) < 1000) {
			return 0;
		}
		if (HAL_GetTick() > tip_disconnected + 1000) {		// Wait at least 1 second before reconnect the IRON tip again
			changeTip(index);
			return mode_return;
		}
	}

    if (button == 2) {										// The button was pressed for a long time
	    return mode_lpress;
	}

    if (tip_disconnected == 0 && !pUnit->isConnected()) {	// Finally, the IRON tip has been disconnected
    	tip_disconnected = HAL_GetTick();					// Now the tip is disconnected
    }

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 20000;

	for (int8_t i = index; i >= 0; --i) {
		if (tip_list[(uint8_t)i].name[0]) {
			index = i;
			break;
		}
	}
	uint8_t tip_index = tip_list[index].tip_index;
	for (uint8_t i = 0; i < MSLCT_LEN; ++i)
		tip_list[i].name[0] = '\0';
	uint8_t list_len = pCFG->tipList(tip_index, tip_list, MSLCT_LEN, true, dev_type);
	if (list_len == 0)										// There is no active tip in the list
		return mode_spress;									// Activate tips mode

	uint8_t ii = 0;
	for (uint8_t i = 0; i < list_len; ++i) {
		if (tip_index == tip_list[i].tip_index) {
			ii = i;
			break;
		}
	}
	pCore->l_enc.reset(ii, 0, list_len-1, 1, 1, false);
	pD->drawTipList(tip_list, list_len, tip_index, true);
	return this;
}

void MSLCT::changeTip(uint8_t index) {
	uint8_t tip_index = tip_list[index].tip_index;
	pCore->cfg.changeTip(tip_index);
	// Clear temperature history and switch iron mode to "power off"
	if (dev_type == d_jbc) {
		pCore->jbc.reset();
	} else {
		pCore->t12.reset();
	}
}

//---------------------- The Activate tip mode: select tips to use ---------------
void MTACT::init(void) {
	CFG*	pCFG	= &pCore->cfg;

	uint8_t tip_index = pCFG->currentTipIndex(d_t12);		// Always draw complete TIPS list: T12 + JBC
	pCore->l_enc.reset(tip_index, 0, pCFG->TIPS::total()-1, 1, 1, false);
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_ACTIVATE_TIPS);
	update_screen = 0;
}

MODE* MTACT::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;

	uint8_t	tip_index 	= pEnc->read();
	uint8_t	button		= pEnc->buttonStatus();

	if (button == 1) {										// The button pressed
		pD->BRGT::dim(50);									// Turn-off the brightness, processing
		if (!pCFG->toggleTipActivation(tip_index)) {
			pD->errorMessage(MSG_EEPROM_WRITE, 50);
			return 0;
		}
		pD->BRGT::on();										// Restore the display brightness
		update_screen = 0;									// Force redraw the screen
	} else if (button == 2) {								// Tip activation finished
		pCFG->close();										// Finish tip list editing
		pCFG->reloadTips();
		// The current tip can be deactivated, so we should find the nearest tip instead
		uint8_t curr_tip = pCFG->currentTipIndex(d_t12);
		curr_tip = pCFG->nearActiveTip(curr_tip);
		pCFG->changeTip(curr_tip);
		curr_tip = pCFG->currentTipIndex(d_jbc);
		curr_tip = pCFG->nearActiveTip(curr_tip);
		pCFG->changeTip(curr_tip);
		return mode_lpress;
	}

	if (pEnc->changed()) {
		update_screen = 0;
	}

	if (HAL_GetTick() >= update_screen) {
		TIP_ITEM	tip_list[7];
		uint8_t loaded = pCFG->tipList(tip_index, tip_list, 7, false, d_unknown); // Build the list of T12 and JBC tips
		pD->drawTipList(tip_list, loaded, tip_index, false);
		update_screen = HAL_GetTick() + 60000;
	}
	return this;
}


//---------------------- The automatic calibration tip mode ----------------------
// Used to automatically calibrate the iron tip
// There are 4 temperature calibration points of the tip in the controller,
// but during calibration procedure we will use more points to cover whole set
// of the internal temperature values. Then use the Ordinary Least Squares method
// to build a calibration line and calculate the temperature in the reference points.
void MCALIB::init(void) {
	CFG*	pCFG	= &pCore->cfg;
	UNIT*	pUnit	= unit();

	// Prepare to enter real temperature
	uint16_t min_t 		= 50;
	uint16_t max_t		= 600;
	if (!pCFG->isCelsius()) {
		min_t 	=  122;
		max_t 	= 1111;
	}
	PIDparam pp = pCFG->pidParamsSmooth(dev_type);				// Load PID parameters to stabilize the temperature of unknown tip
	pUnit->PID::load(pp);
	pCore->l_enc.reset(0, min_t, max_t, 1, 1, false);
	for (uint8_t i = 0; i < MCALIB_POINTS; ++i) {
		calib_temp[0][i] = 0;									// Real temperature. 0 means not entered yet
		calib_temp[1][i] = map(i, 0, MCALIB_POINTS-1, start_int_temp, int_temp_max / 2); // Internal temperature
	}
	check_device_tm	= 0;
	ref_temp_index 	= 0;
	tuning			= false;
	phase			= MC_OFF;
	ready_to		= 0;
	phase_change	= 0;
	update_screen 	= 0;
	tip_temp_max 	= int_temp_max / 2;							// The maximum possible temperature defined in iron.h
	const char *calibrate = pCore->dspl.msg(MSG_MENU_CALIB);	// "Calibrate <tip name>"
	uint8_t len = strlen(calibrate);
	if (len > 19) len = 19;										// Limit maximum string length
	std::string title(calibrate, len);
	title = title + " " + pCFG->tipName(dev_type);
	pCore->dspl.clear();
	pCore->dspl.drawTitleString(title.c_str());
}

/*
 * Calculate tip calibration parameter using linear approximation by Ordinary Least Squares method
 * Y = a * X + b, where
 * Y - internal temperature, X - real temperature. a and b are double coefficients
 * a = (N * sum(Xi*Yi) - sum(Xi) * sum(Yi)) / ( N * sum(Xi^2) - (sum(Xi))^2)
 * b = 1/N * (sum(Yi) - a * sum(Xi))
 */
bool MCALIB::calibrationOLS(uint16_t* tip, uint16_t min_temp, uint16_t max_temp) {
	long sum_XY = 0;											// sum(Xi * Yi)
	long sum_X 	= 0;											// sum(Xi)
	long sum_Y  = 0;											// sum(Yi)
	long sum_X2 = 0;											// sum(Xi^2)
	long N		= 0;

	for (uint8_t i = 0; i < MCALIB_POINTS; ++i) {
		uint16_t X 	= calib_temp[0][i];
		uint16_t Y	= calib_temp[1][i];
		if (X >= min_temp && X <= max_temp) {
			sum_XY 	+= X * Y;
			sum_X	+= X;
			sum_Y   += Y;
			sum_X2  += X * X;
			++N;
		}
	}

	if (N <= 2)													// Not enough real temperatures have been entered
		return false;

	double	a  = (double)N * (double)sum_XY - (double)sum_X * (double)sum_Y;
			a /= (double)N * (double)sum_X2 - (double)sum_X * (double)sum_X;
	double 	b  = (double)sum_Y - a * (double)sum_X;
			b /= (double)N;

	for (uint8_t i = 0; i < 4; ++i) {
		double temp = a * (double)pCore->cfg.referenceTemp(i, dev_type) + b;
		tip[i] = round(temp);
	}
	if (tip[3] > int_temp_max) tip[3] = int_temp_max;			// Maximal possible temperature (main.h)
	return true;
}

// Find the index of the reference point with the closest temperature
uint8_t MCALIB::closestIndex(uint16_t temp) {
	uint16_t diff = 1000;
	uint8_t index = MCALIB_POINTS;
	for (uint8_t i = 0; i < MCALIB_POINTS; ++i) {
		uint16_t X = calib_temp[0][i];
		if (X > 0 && abs(X-temp) < diff) {
			diff = abs(X-temp);
			index = i;
		}
	}
	return index;
}

void MCALIB::updateReference(uint8_t indx) {					// Update reference points
	CFG*	pCFG	= &pCore->cfg;
	uint16_t expected_temp 	= map(indx, 0, MCALIB_POINTS, pCFG->tempMinC(dev_type), pCFG->tempMaxC(dev_type));
	uint16_t r_temp			= calib_temp[0][indx];
	if (indx < 5 && r_temp > (expected_temp + expected_temp/4)) {	// The real temperature is too high
		tip_temp_max -= tip_temp_max >> 2;						// tip_temp_max *= 0.75;
		if (tip_temp_max < int_temp_max / 4)
			tip_temp_max = int_temp_max / 4;					// Limit minimum possible value of the highest temperature

	} else if (r_temp > (expected_temp + expected_temp/8)) { 	// The real temperature is biger than expected
		tip_temp_max += tip_temp_max >> 3;						// tip_temp_max *= 1.125;
		if (tip_temp_max > int_temp_max)
			tip_temp_max = int_temp_max;
	} else if (indx < 5 && r_temp < (expected_temp - expected_temp/4)) { // The real temperature is too low
		tip_temp_max += tip_temp_max >> 2;						// tip_temp_max *= 1.25;
		if (tip_temp_max > int_temp_max)
			tip_temp_max = int_temp_max;
	} else if (r_temp < (expected_temp - expected_temp/8)) { 	// The real temperature is lower than expected
		tip_temp_max += tip_temp_max >> 3;						// tip_temp_max *= 1.125;
		if (tip_temp_max > int_temp_max)
			tip_temp_max = int_temp_max;
	} else {
		return;
	}

	// rebuild the array of the reference temperatures
	for (uint8_t i = indx+1; i < MCALIB_POINTS; ++i) {
		calib_temp[1][i] = map(i, 0, MCALIB_POINTS-1, start_int_temp, tip_temp_max);
	}
}


void MCALIB::buildFinishCalibration(void) {
	CFG* 	pCFG 	= &pCore->cfg;
	uint16_t tip[4];
	if (calibrationOLS(tip, 150, pCFG->referenceTemp(2, dev_type))) {
		uint8_t near_index	= closestIndex(pCFG->referenceTemp(3, dev_type));
		tip[3] = map(pCFG->referenceTemp(3, dev_type), pCFG->referenceTemp(2, dev_type), calib_temp[0][near_index],
				tip[2], calib_temp[1][near_index]);
		if (tip[3] > int_temp_max) tip[3] = int_temp_max;		// Maximal possible temperature (main.h)

		uint8_t tip_index 	= pCFG->currentTipIndex(dev_type);
		int16_t ambient 	= pCore->ambientTemp();
		pCFG->applyTipCalibtarion(tip, ambient, dev_type);
		pCFG->saveTipCalibtarion(tip_index, tip, TIP_ACTIVE | TIP_CALIBRATED, ambient);
		pCore->buzz.shortBeep();
	} else {
		pCore->buzz.failedBeep();
		pCFG->resetTipCalibration(dev_type);
	}
}

/*
 * The calibration procedure consists of 8 steps (number of temperature reference points)
 * Each step is to check the real temperature at given reference point/ The step begins when the rotary encoder pressed
 * see 'Next step begins here'. As soon as PID algorithm used, there is some bouncing process before reference reached, damped oscillations.
 * Reaching required reference temperature process is split into several phases:
 * MC_GET_READY (if the temperature is higher) -> MC_HEATING (over temperature) -> MC_COOLING (under temperature) -> MC_HEATING_AGAIN -> MC_READY
 * The oscillation process limited by some timeout, ready_to
 */
MODE* MCALIB::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	UNIT*	pUnit	= unit();
	RENC*	pEnc	= &pCore->l_enc;

	uint16_t encoder	= pEnc->read();
    uint8_t  button		= pEnc->buttonStatus();

    if (pEnc->changed()) {
    	update_screen = 0;
    }

	if (button == 1) {											// The button pressed
		if (tuning) {											// New reference temperature was entered
			pUnit->switchPower(false);
		    if (phase == MC_READY) {							// The temperature was stabilized and real data can be entered
		    	phase = MC_OFF;
			    uint16_t temp	= pUnit->averageTemp();			// The temperature of the IRON in internal units
			    uint16_t r_temp = encoder;						// The real temperature entered by the user
			    if (!pCFG->isCelsius())							// Always save the human readable temperature in Celsius
			    	r_temp = fahrenheitToCelsius(r_temp);
			    uint8_t ref			= ref_temp_index;
			    calib_temp[0][ref]	= r_temp;
			    calib_temp[1][ref]	= temp;
			    if (r_temp < pCFG->tempMaxC(dev_type) - 50) {	// Continue with the next reference temperature
			    	updateReference(ref_temp_index);			// Update reference temperature points
			    	++ref_temp_index;
			    	// Try to update the current tip calibration
			    	uint16_t tip[4];
			    	 if (calibrationOLS(tip, 100, 600)) {		// Take into the account temperature values in specified interval 100 <= t <= 600 only Finish calibration use another parameters
			    		 pCFG->applyTipCalibtarion(tip, pCore->ambientTemp(), dev_type);
			    		 if (r_temp > 350) {					// Double check the next reference temperature point
			    			 int16_t ambient 	= pCore->ambientTemp();
			    			 uint16_t temp		= pCFG->tempToHuman(calib_temp[1][ref_temp_index], ambient, dev_type);
			    			 if (temp > 450) {
			    				 calib_temp[1][ref_temp_index] = pCFG->humanToTemp(430, ambient, dev_type);
			    			 }
			    		 }
			    	 }
			    } else {										// Finish calibration
			    	ref_temp_index = MCALIB_POINTS;
			    }
		    	ready_to		= 0;
		    	phase_change	= 0;
		    } else {											// Stop heating, return from tuning mode
		    	tuning = false;
		    	update_screen = 0;
		    	return this;
		    }
		    tuning = false;
		}
		if (!tuning) {											// Next step begins here. Heating to the next reference temperature
			if (ref_temp_index < MCALIB_POINTS) {
				tuning = true;
				uint16_t temp_set	= calib_temp[1][ref_temp_index];
				uint16_t temp 		= pUnit->averageTemp();
				phase = (temp_set < temp)?MC_GET_READY:MC_HEATING;	// Start heating the Iron
				pUnit->setTemp(temp_set);
				pUnit->switchPower(true);
				ready_to 		= HAL_GetTick() + 120000;		// We should be ready in 2 minutes
				phase_change	= HAL_GetTick() + phase_change_time;
				check_device_tm = HAL_GetTick() + check_device_to;
			} else {											// All reference points are entered
				buildFinishCalibration();
				PIDparam pp = pCFG->pidParams(dev_type);		// Restore default PID parameters
				pUnit->PID::load(pp);;
				pD->endCalibration();							// Free the allocated BITMAP
				return mode_lpress;
			}
		}
		update_screen = 0;
	} else if (!tuning && button == 2) {						// The button was pressed for a long time, save tip calibration
		buildFinishCalibration();
		PIDparam pp = pCFG->pidParams(dev_type);				// Restore default PID parameters
		pUnit->PID::load(pp);
		pD->endCalibration();									// Free the allocated BITMAP
	    return mode_lpress;
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 500;

	if (check_device_tm > 0 && HAL_GetTick() > check_device_tm && !pUnit->isConnected())
		return 0;

	int16_t	 ambient	= pCore->ambientTemp();
	uint16_t real_temp 	= encoder;
	uint16_t temp_set	= pUnit->presetTemp();
	uint16_t temp 		= pUnit->averageTemp();
	uint8_t  power		= pUnit->avgPowerPcnt();
	uint16_t tempH 		= pCFG->tempToHuman(temp, ambient, dev_type);

	if (temp >= int_temp_max) {									// Prevent soldering IRON overheat, save current calibration
		buildFinishCalibration();
		PIDparam pp = pCFG->pidParams(dev_type);				// Restore default PID parameters
		pUnit->PID::load(pp);
		pD->endCalibration();									// Free the allocated BITMAP
		return mode_lpress;
	}

	if (phase_change && HAL_GetTick() >= phase_change) {
		if (tuning && (abs(temp_set - temp) <= 16) && (pUnit->pwrDispersion() <= 200) && power > 1)  {
			switch (phase) {
				case MC_HEATING:								// First wave heating, over the preset temperature, start cooling
					phase = MC_COOLING;
					phase_change	= HAL_GetTick() + phase_change_time;
					break;
				case MC_HEATING_AGAIN:							// Second wave of heating, ready to enter the real temperature
					pCore->buzz.shortBeep();
					pEnc->write(tempH);
					phase = MC_READY;
					phase_change	= HAL_GetTick() + phase_change_time;
					break;
				case MC_READY:
				case MC_OFF:
					break;
				default:
					break;
			}
		}
		if (phase == MC_COOLING && (temp_set > temp + 8)) {		// Cooled lower than the preset temperature, start heating again
			phase = MC_HEATING_AGAIN;
			phase_change	= HAL_GetTick() + phase_change_time;
		}
		if (phase == MC_GET_READY && (temp_set > temp + 8)) {
			phase = MC_HEATING;
			phase_change	= HAL_GetTick() + phase_change_time;
		}
	}
	// Check the timeout
	if (ready_to > 0 && phase != MC_OFF && phase != MC_READY && HAL_GetTick() > ready_to) {
		pCore->buzz.lowBeep();
		phase = MC_READY;
	}

	uint8_t	int_temp_pcnt = 0;
	if (temp >= start_int_temp)
		int_temp_pcnt = map(temp, start_int_temp, int_temp_max, 0, 100);	// int_temp_max defined in vars.cpp
	pD->calibShow(ref_temp_index+1, tempH, real_temp, pCFG->isCelsius(), power, tuning, phase == MC_READY, int_temp_pcnt);
	return this;
}

//---------------------- The manual calibration tip mode -------------------------
// Here the operator should 'guess' the internal temperature readings for desired temperature.
// Rotate the encoder to change temperature preset in the internal units
// and controller would keep that temperature.
// This method is more accurate one, but it requires more time.
void MCALIB_MANUAL::init(void) {
	CFG*	pCFG		= &pCore->cfg;
	PIDparam pp 		= pCFG->pidParamsSmooth(dev_type);
	if (dev_type == d_t12) {
		pCore->t12.PID::load(pp);
	} else if (dev_type == d_jbc) {
		pCore->jbc.PID::load(pp);
	} else {
		pCore->hotgun.PID::load(pp);
		pCore->hotgun.setFan(fan_speed);
	}
	ref_temp_index 		= 1;									// Start with 260 degrees
	ready				= false;
	tuning				= false;
	temp_setready_ms	= 0;
	update_screen		= 0;
	pCore->l_enc.reset(ref_temp_index, 0, 3, 1, 1, true);		// Select reference temperature point (240) using Encoder
	pCFG->getTipCalibtarion(calib_temp, dev_type);				// Load current calibration data
	for (uint8_t i = 0; i < 3; ++i)								// The reference temperatures are not calibrated yet
		calib_flag[i] = false;
	const char *calibrate = pCore->dspl.msg(MSG_MENU_CALIB);	// "Calibrate <tip name>"
	uint8_t len = strlen(calibrate);
	if (len > 19) len = 19;										// Limit maximum string length
	std::string title(calibrate, len);
	title += " ";
	if (dev_type != d_gun)
		title += pCFG->tipName(dev_type);
	else
		title += pCore->dspl.msg(MSG_HOT_AIR_GUN);
	pCore->dspl.clear();
	pCore->dspl.drawTitleString(title.c_str());
}

// Make sure the tip[0] < tip[1] < tip[2] < tip[3];
// And the difference between next points is greater than req_diff
// Change neighborhood temperature data to keep this difference
void MCALIB_MANUAL::buildCalibration(int8_t ablient, uint16_t tip[], uint8_t ref_point) {
	if (tip[3] > int_temp_max) tip[3] = int_temp_max;			// int_temp_max is a maximum possible temperature (vars.cpp)

	const int req_diff = 200;
	if (ref_point <= 3) {										// tip[0-3] - internal temperature readings for the tip at reference points (200-400)
		for (uint8_t i = ref_point; i <= 2; ++i) {				// ref_point is 0 for 200 degrees and 3 for 400 degrees
			int diff = (int)tip[i+1] - (int)tip[i];
			if (diff < req_diff) {
				tip[i+1] = tip[i] + req_diff;					// Increase right neighborhood temperature to keep the difference
			}
		}
		if (tip[3] > int_temp_max)								// The high temperature limit is exceeded, temp_max. Lower all calibration
			tip[3] = int_temp_max;

		for (int8_t i = 3; i > 0; --i) {
			int diff = (int)tip[i] - (int)tip[i-1];
			if (diff < req_diff) {
				int t = (int)tip[i] - req_diff;					// Decrease left neighborhood temperature to keep the difference
				if (t < 0) t = 0;
				tip[i-1] = t;
			}
		}
	}
}

void MCALIB_MANUAL::restorePIDconfig(CFG *pCFG, UNIT* pUnit) {
	PIDparam pp = pCFG->pidParams(dev_type);
	pUnit->PID::load(pp);
}

MODE* MCALIB_MANUAL::loop(void) {
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;
	UNIT*	pUnit	= unit();

	uint16_t encoder	= pEnc->read();
    uint8_t  button		= pEnc->buttonStatus();

    int16_t enc_change = pEnc->changed();
    if (enc_change) {
    	if (tuning) {											// Preset temperature (internal units)
    		pUnit->setTemp(encoder);
    		ready = false;
    		if (enc_change < 0) {								// The preset temperature was decreased
    			if (restore_power_ms == 0)
    				pUnit->switchPower(false);
    			restore_power_ms = HAL_GetTick() + 500;
    		}
    		temp_setready_ms = HAL_GetTick() + 5000;    		// Prevent beep just right the new temperature setup
    	} else {
    		ref_temp_index = encoder;							// Update reference temperature index
    	}
    	update_screen = 0;
    }

	int16_t ambient = pCore->ambientTemp();

	if (button == 1) {											// The button pressed
		if (tuning) {											// New reference temperature was confirmed
			pUnit->switchPower(false);
		    if (ready) {										// The temperature has been stabilized
		    	ready = false;
		    	uint16_t temp	= pUnit->averageTemp();			// The temperature of the IRON of Hot Air Gun in internal units
			    uint8_t ref 	= ref_temp_index;
			    calib_temp[ref] = temp;
			    calib_flag[ref] = true;							// Mark this point as a calibrated
			    uint16_t tip[4];
			    for (uint8_t i = 0; i < 4; ++i) {
			    	tip[i] = calib_temp[i];
			    }
			    buildCalibration(ambient, tip, ref);			// ref is 0 for 200 degrees and 3 for 400 degrees
			    pCFG->applyTipCalibtarion(tip, ambient, dev_type);
		    }
		    tuning	= false;
			encoder = ref_temp_index;
		    pEnc->reset(encoder, 0, 3, 1, 1, true);				// Turn back to the reference temperature point selection mode
		} else {												// Reference temperature index was selected from the list
			tuning 			= true;
			uint16_t temp 	= calib_temp[encoder];
			pEnc->reset(temp, 100, int_temp_max, 1, 5, false); // int_temp_max declared in vars.cpp
			pUnit->setTemp(temp);
			pUnit->switchPower(true);
			temp_setready_ms = HAL_GetTick() + 10000;
		}
		update_screen		= 0;
		restore_power_ms	= 0;
	} else if (button == 2) {									// The button was pressed for a long time, save tip calibration
		uint8_t tip_index = pCFG->currentTipIndex(dev_type);	// IRON actual tip index or Hot Air Gun (tip index = 0)
		buildCalibration(ambient, calib_temp, 10); 				// 10 is bigger then the last index in the reference temp. Means build final calibration
		pCFG->applyTipCalibtarion(calib_temp, ambient, dev_type);
		pCFG->saveTipCalibtarion(tip_index, calib_temp, TIP_ACTIVE | TIP_CALIBRATED, ambient);
		restorePIDconfig(pCFG, pUnit);
		pCore->dspl.endCalibration();									// Free the allocated BITMAP
	    return mode_lpress;
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 500;

	if (restore_power_ms > 0 && HAL_GetTick() > restore_power_ms) {
		restore_power_ms = 0;
		pUnit->switchPower(true);
	}

	if (temp_setready_ms && (HAL_GetTick() > temp_setready_ms) && !pUnit->isConnected()) {
		restorePIDconfig(pCFG, pUnit);
		return 0;
	}

	uint16_t temp_set		= pUnit->presetTemp();				// Prepare the parameters to be displayed
	uint16_t temp			= pUnit->averageTemp();
	uint8_t  power			= pUnit->avgPowerPcnt();
	uint16_t pwr_disp		= pUnit->pwrDispersion();
	uint16_t pwr_disp_max	= (dev_type != d_gun)?200:40;
	if (tuning && (abs(temp_set - temp) <= 16) && (pwr_disp <= pwr_disp_max) && power > 0)  {
		if (!ready && temp_setready_ms && (HAL_GetTick() > temp_setready_ms)) {
			pCore->buzz.shortBeep();
			ready 				= true;
			temp_setready_ms	= 0;
	    }
	}

	uint16_t temp_setup = temp_set;
	if (!tuning) {
		temp_setup 		= calib_temp[ref_temp_index];
	}

	pCore->dspl.calibManualShow(pCFG->referenceTemp(ref_temp_index, dev_type), temp, temp_setup, pCFG->isCelsius(), power, tuning, ready, calib_flag[ref_temp_index]);
	return	this;
}

//---------------------- The PID coefficients tune mode --------------------------
void MTPID::init(void) {
	DSPL*	pD		= &pCore->dspl;
	RENC*	pEnc	= &pCore->l_enc;

	allocated 			= pD->pidStart();
	pEnc->reset(0, 0, 2, 1, 1, true);							// Select the coefficient to be modified
	data_update 		= 0;
	data_index 			= 0;
	modify				= false;
	on					= false;
	old_index			= 3;
	update_screen 		= 0;
	reset_dspl			= true;
}

MODE* MTPID::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	RENC*	pEnc	= &pCore->l_enc;
	UNIT*	pUnit	= unit();

	if (!allocated) {
		pCore->buzz.failedBeep();
		return mode_lpress;
	}

	if (pCore->u_enc.buttonStatus() > 0) {					// Emergency OFF
		on = false;
		pUnit->switchPower(on);
	}

	uint16_t index 	= pEnc->read();
	uint8_t  button	= pEnc->buttonStatus();

    if (!pUnit->isConnected()) {
    	if (dev_type != d_gun) {
    		return 0;
    	} else {
    		if (pCore->hotgun.isFanWorking())
    			return 0;
    	}
    }

	if (button || old_index != index)
		update_screen = 0;

	if (HAL_GetTick() >= data_update) {
		data_update = HAL_GetTick() + 100;
		int16_t  temp = pUnit->averageTemp() - pUnit->presetTemp();;
		uint32_t disp = pUnit->pwrDispersion();
		pD->GRAPH::put(temp, disp);
	}

	if (HAL_GetTick() < update_screen) return this;

	PID* pPID	= (PID*)unit();
	if (modify) {											// The Coefficient is selected, start to show the Graphs
		update_screen = HAL_GetTick() + 100;
		if (button == 1) {									// Short button press: select another PID coefficient
			modify = false;
			pEnc->reset(data_index, 0, 2, 1, 1, true);
			reset_dspl = true;
			return this;									// Restart the procedure
		} else if (button == 2) {							// Long button press: toggle the power
			on = !on;
			uint16_t temp	= pCFG->tempPresetHuman(dev_type);
			int16_t ambient = pCore->ambientTemp();
			temp 			= pCFG->humanToTemp(temp, ambient, dev_type);
			pUnit->setTemp(temp);
			pUnit->switchPower(on);
			if (on) pD->GRAPH::reset();						// Reset display graph history
			pCore->buzz.shortBeep();
		}
		if (reset_dspl) {									// Flag indicating we should completely redraw display
			reset_dspl = false;
			pD->clear();
			pD->pidAxis("manual PID", "T", "D(P)");
		}
		if (old_index != index) {
			old_index = index;
			pPID->changePID(data_index+1, index);
			pD->pidModify(data_index, index);
			update_screen = HAL_GetTick() + 1000;			// Show new parameter value for 1 second
			return this;
		}
		pD->pidShowGraph();
	} else {												// Selecting the PID coefficient to be tuned
		update_screen = HAL_GetTick() + 1000;

		if (old_index != index) {
			old_index	= index;
			data_index  = index;
		}

		if (button == 1) {									// Short button press: select another PID coefficient
			modify = true;
			data_index  = index;
			// Prepare to change the coefficient [index]
			uint16_t k = 0;
			k = pPID->changePID(index+1, -1);				// Read the PID coefficient from the IRON or Hot Air Gun
			uint8_t inc 	= 1;							// Calculate increments
			uint8_t inc_b	= 10;
			if (index == 0 || index == 2) {
				inc = 10;
				inc_b = 100;
			}
			on = false;
			pUnit->switchPower(on);
			pEnc->reset(k, 0, 30000, inc, inc_b, false);
			reset_dspl	= true;
			return this;									// Restart the procedure
		} else if (button == 2) {							// Long button press: save the parameters and return to menu
			if (confirm()) {
				PIDparam pp = pPID->dump();
				pCFG->savePID(pp, dev_type);
				pCore->buzz.shortBeep();
			} else {
				pCore->buzz.failedBeep();
			}
			return mode_lpress;
		}

		if (reset_dspl) {									// Flag indicating we should completely redraw display
			reset_dspl = false;
			pD->clear();
		}
		uint16_t pid_k[3];
		for (uint8_t i = 0; i < 3; ++i) {
			pid_k[i] = 	pPID->changePID(i+1, -1);
		}
		pD->pidShowMenu(pid_k, data_index);
	}
	return this;
}

bool MTPID::confirm(void) {
	pCore->l_enc.reset(0, 0, 1, 1, 1, true);
	pCore->dspl.clear();
	pCore->buzz.shortBeep();
	PID* pPID	= &pCore->hotgun;
	if (dev_type == d_t12)
		pPID	= &pCore->t12;
	else if (dev_type == d_jbc)
		pPID 	= &pCore->jbc;
	uint16_t pid_k[3];
	for (uint8_t i = 0; i < 3; ++i) {
		pid_k[i] = 	pPID->changePID(i+1, -1);
	}
	pCore->dspl.pidShowMenu(pid_k, 3);

	while (true) {
		if (pCore->dspl.adjust())							// Adjust display brightness
			HAL_Delay(5);
		uint8_t answer = pCore->l_enc.read();
		if (pCore->l_enc.buttonStatus() > 0)
			return answer == 0;
		pCore->dspl.showDialog(MSG_SAVE_Q, 150, answer == 0);
	}
	return false;
}

void MTPID::clean(void) {
	pCore->dspl.pidDestroyData();
}

//---------------------- The PID coefficients automatic tune mode ----------------
void MAUTOPID::init(void) {
	DSPL*	pD		= &pCore->dspl;

	PIDparam pp = pCore->cfg.pidParamsSmooth(dev_type);		// Load PID parameters to stabilize the temperature of unknown tip
	UNIT *pUnit	= unit();
	pUnit->PID::load(pp);

	pD->pidStart();
	if (dev_type == d_t12) {
		td_limit	= 6;
		pwr_ch_to	= 5000;									// Power change timeout (ms)
	} else if (dev_type == d_gun){
		td_limit	= 500;
		pwr_ch_to	= 20000;
		if (!pCore->hotgun.isConnected()) {					// Check the Hot Air connected
			pCore->hotgun.fanControl(true);
			HAL_Delay(1000);
			pCore->hotgun.fanControl(false);
		}
	} else {												// JBC IRON
		td_limit	= 50;
		pwr_ch_to	= 20000;
	}
	uint16_t temp	= pCore->cfg.tempPresetHuman(dev_type);
	int16_t ambient = pCore->ambientTemp();
	base_temp 		= pCore->cfg.humanToTemp(temp, ambient, dev_type);
	pCore->l_enc.reset(0, 0, max_pwr, 1, 10, false);		// Setup Encoder to provide heating power
	data_update 	= 0;
	data_period		= 250;
	phase_to		= 0;
	mode			= TUNE_OFF;
	pD->clear();
	pD->pidAxis("Auto PID", "T", "p");
	update_screen 	= 0;
}

MODE* MAUTOPID::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	RENC*	pEnc	= &pCore->l_enc;
	UNIT*	pUnit	= unit();

    if (!pUnit->isConnected()) {
    	if (dev_type != d_gun) {
    		return 0;
    	} else {
    		if (pCore->hotgun.isFanWorking())
    			return 0;
    	}
    }

	uint8_t  button		= pEnc->buttonStatus();
    if(button)
		update_screen = 0;

	if (HAL_GetTick() >= data_update) {
	    int16_t temp	= pUnit->averageTemp() - base_temp;
	    uint16_t p		= pUnit->avgPower();
		data_update 	= HAL_GetTick() + data_period;
		pD->GRAPH::put(temp, p);
	}

	uint16_t pwr = pEnc->read();
	if (pEnc->changed() != 0) {									// The encoder rotated
		if (mode == TUNE_OFF) {
			button = 1;											// Simulate the button has been pressed, start heating phase
			update_screen = 0;
		} else if (mode == TUNE_HEATING) {
			pUnit->fixPower(pwr);
		}
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 500;

	int16_t  temp		= pUnit->averageTemp();
	uint32_t td			= pUnit->tmpDispersion();
	uint32_t pd			= pUnit->pwrDispersion();
	int32_t  ap			= pUnit->avgPower();

	if (button == 1) {											// Short button press: switch on/off the power
		data_period	= 250;
		if (mode == TUNE_OFF) {
			mode = TUNE_HEATING;
			base_temp 		= pUnit->presetTemp();
			base_temp		= constrain(base_temp, 1100, 1600);
			pD->GRAPH::reset();									// Reset display graph history
			pUnit->fixPower(pwr);
			pD->pidShowMsg("Heating");
			uint32_t n 		= HAL_GetTick();
			update_screen 	= n + msg_to;
			phase_to		= 0;								// no timeout for this phase
			next_mode 		= 0;
			return this;
		} if (mode == TUNE_HEATING) {							// The base temperature stabilized
			if ((temp > base_temp) && (temp < base_temp + 7) && (pd <= 4) && (ap > 0)) {
				base_pwr = ap + (ap+10)/20;					// Add 5%
				pUnit->fixPower(base_pwr);					// Apply base power
				pD->pidShowMsg("Base power");
				pCore->buzz.shortBeep();
				uint32_t n = HAL_GetTick();
				update_screen = n + msg_to;
				next_mode = n + pwr_ch_to;					// Wait before go to supply fixed power
				phase_to  = n + 180000;
				mode = TUNE_BASE;
				old_temp = 0;
				pwr_change	= FIX_PWR_NONE;
				return this;
			}
		} else {												// Running mode
			pUnit->switchPower(false);
			if ((mode == TUNE_RELAY) && (tune_loops > 8) && updatePID(pUnit)) {
				if (mode_spress) {
					mode_spress->useDevice(dev_type);
					return mode_spress;
				}
			}
			mode = TUNE_OFF;
			pD->pidShowMsg("Stop");
			update_screen = HAL_GetTick() + msg_to;
			return this;
		}
	} else if (button == 2 && mode_lpress) {					// Long button press
		PIDparam pp = pCore->cfg.pidParams(dev_type);			// Restore standard PID parameters
		pUnit->PID::load(pp);
		mode_lpress->useDevice(dev_type);
		return mode_lpress;
	}

	if (mode_return && pCore->u_enc.buttonStatus() > 0) {		// The upper encoder button pressed
		return mode_return;
	}

	if (next_mode <= HAL_GetTick()) {
		switch (mode) {
			case TUNE_BASE:										// Applying base power
			{
				bool power_changed = false;
				if (old_temp == 0) {							// Setup starting temperature
					old_temp = temp;
					next_mode = HAL_GetTick() + 1000;
					return this;
				} else {
					next_mode = HAL_GetTick() + 1000;
					if (pwr_change != FIX_PWR_DONE && temp < base_temp && old_temp > temp) {
						if (dev_type != d_gun) {				// Add 1% of power
							base_pwr += pUnit->getMaxFixedPower()/100;
						} else {
							++base_pwr;
						}
						pUnit->fixPower(base_pwr);
						power_changed = true;
						next_mode = HAL_GetTick() + pwr_ch_to;
						if (pwr_change == FIX_PWR_DECREASED)
							pwr_change = FIX_PWR_DONE;
						else
							pwr_change = FIX_PWR_INCREASED;
					} else if (pwr_change != FIX_PWR_DONE && temp > base_temp && old_temp < temp) {
						if (dev_type != d_gun) {				// Subtract 1% of power
							base_pwr -= pUnit->getMaxFixedPower()/100;

						} else {
							--base_pwr;
						}
						pUnit->fixPower(base_pwr);
						power_changed = true;
						next_mode = HAL_GetTick() + pwr_ch_to;
						if (pwr_change == FIX_PWR_INCREASED)
							pwr_change = FIX_PWR_DONE;
						else
							pwr_change = FIX_PWR_DECREASED;
					}
					old_temp = temp;
					if (power_changed) return this;
				}
				if (old_temp && (td <= td_limit) && (pwr_change == FIX_PWR_DONE || abs(temp - base_temp) < 20)) {
					base_temp	= temp;
					delta_power = base_pwr/4;
					pD->GRAPH::reset();							// Redraw graph, because base temp has been changed!
					pD->pidShowMsg("pwr plus");
					pUnit->fixPower(base_pwr + delta_power);
					pCore->buzz.shortBeep();
					uint32_t n = HAL_GetTick();
					update_screen = n + msg_to;
					next_mode = n + 20000;						// Wait to change the temperature accordingly
					mode = TUNE_PLUS_POWER;
					phase_to	    = 0;
					if (td_limit < 150)
						td_limit = 150;
					return this;
				}
				break;
			}
			case TUNE_PLUS_POWER:								// Applying base_power+delta_power
				if ((td <= td_limit) && (pd <= 4)) {
					delta_temp	= temp - base_temp;
					pD->pidShowMsg("pwr minus");
					pUnit->fixPower(base_pwr - delta_power);
					pCore->buzz.shortBeep();
					uint32_t n = HAL_GetTick();
					update_screen = n + msg_to;
					next_mode = n + 40000;						// Wait to change the temperature accordingly
					mode = TUNE_MINUS_POWER;
					phase_to	= 0;
					return this;
				}
				break;
			case TUNE_MINUS_POWER:								// Applying base_power-delta_power
				if ((temp < (base_temp - delta_temp)) && (td <= td_limit) && (pd <= 4)) {
					tune_loops	= 0;
					uint16_t delta = base_temp - temp;
					if (delta < delta_temp)
						delta_temp = delta;						// delta_temp is minimum of upper and lower differences
					delta_temp <<= 1;							// use 2/3
					delta_temp /= 3;
					if (delta_temp < max_delta_temp)
						delta_temp = max_delta_temp;
					if (dev_type != d_gun && delta_temp > max_delta_temp)
						delta_temp = max_delta_temp;			// limit delta_temp in case of IRON
					pUnit->autoTunePID(base_pwr, delta_power, base_temp, delta_temp);
					pCore->buzz.doubleBeep();
					pD->pidShowMsg("start tuning");
					update_screen = HAL_GetTick() + msg_to;
					mode = TUNE_RELAY;
					phase_to	= 0;
					return this;
				}
				break;
			case TUNE_RELAY:									// Automatic tuning of PID parameters using relay method
				if (pUnit->autoTuneLoops() > tune_loops) {		// New oscillation loop
					tune_loops = pUnit->autoTuneLoops();
					if (tune_loops > 3) {
						if (tune_loops < 12) {
							uint16_t tune_period = pUnit->autoTunePeriod();
							tune_period += 250; tune_period -= tune_period%250;
							data_period	= constrain(tune_period/80, 50, 2000);	// Try to display two periods on the screen
						}
						uint16_t period = pUnit->autoTunePeriod();
						period = constrain((period+50)/100, 0, 999);
						pD->pidShowInfo(period, tune_loops);
					}
					if ((tune_loops >= 24) || ((tune_loops >= 16) && pUnit->periodStable())) {
						pUnit->switchPower(false);
						updatePID(pUnit);
						mode = TUNE_OFF;
						if (mode_spress) {
							mode_spress->useDevice(dev_type);
							return mode_spress;
						}
					}
				}
				break;
			case TUNE_OFF:
			case TUNE_HEATING:
			default:
				break;
		}
	}

	if (phase_to && HAL_GetTick() > phase_to) {
		pUnit->switchPower(false);
		mode = TUNE_OFF;
		pD->pidShowMsg("Stop");
		update_screen = HAL_GetTick() + msg_to;
		phase_to	  = 0;
		return this;
	}
	pD->pidShowGraph();
	return this;
}

/*
 * diff  = alpha^2 - epsilon^2, where
 * alpha	- the amplitude of temperature oscillations
 * epsilon	- the temperature hysteresis (see max_delta_temp)
 */
bool MAUTOPID::updatePID(UNIT *pUnit) {
	uint32_t alpha	= (pUnit->tempMax() - pUnit->tempMin() + 1) / 2;
	int32_t diff	= alpha*alpha - delta_temp*delta_temp;
	if (diff > 0) {
		pUnit->newPIDparams(delta_power, diff, pUnit->autoTunePeriod());
		pCore->buzz.shortBeep();
		return true;
	}
	return false;
}

void MAUTOPID::clean(void) {
	pCore->dspl.pidDestroyData();
}

//---------------------- The Fail mode: display error message --------------------
void MFAIL::init(void) {
	pCore->l_enc.reset(0, 0, 1, 1, 1, false);
	pCore->buzz.failedBeep();
	pCore->dspl.clear();
	pCore->dspl.errorMessage(message, 100);						// Write ERROR message specified with setMessage()
	if (parameter[0])
		pCore->dspl.debugMessage(parameter, 50, 200, 170);
	update_screen = 0;
}

MODE* MFAIL::loop(void) {
	if (pCore->l_enc.buttonStatus() || pCore->l_enc.buttonStatus()) {
		message = MSG_LAST;										// Clear message
		return mode_return;
	}
	return this;
}

void MFAIL::setMessage(const t_msg_id msg, const char *parameter) {
	message = msg;
	if (parameter) {
		strncpy(this->parameter, parameter, 19);
	} else {
		this->parameter[0] = '\0';
	}
}

//---------------------- The About dialog mode. Show about message ---------------
void MABOUT::init(void) {
	pCore->l_enc.reset(0, 0, 1, 1, 1, false);
	setTimeout(20);												// Show version for 20 seconds
	resetTimeout();
	pCore->dspl.clear();
	update_screen = 0;
}

MODE* MABOUT::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	uint8_t b_status = pCore->l_enc.buttonStatus();
	if (b_status == 1) {										// Short button press
		return mode_return;										// Return to the main menu
	} else if (b_status == 2) {
		return mode_lpress;										// Activate debug mode
	}
	if (pCore->u_enc.buttonStatus() > 0) {
		return flash_debug;
	}


	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 60000;

	pD->showVersion();
	return this;
}

//---------------------- The Debug mode: display internal parameters ------------
void MDEBUG::init(void) {
	pCore->u_enc.reset(0, 0, max_iron_power, 2, 10, false);
	pCore->l_enc.reset(min_fan_speed, min_fan_speed, max_fan_speed,  5, 10, false);
	pCore->dspl.clear();
	pCore->dspl.drawTitleString("Debug info");
	jbc_selected	= !pCore->jbc.isReedSwitch(true);			// The jbc IRON is in use, manage it, not T12
	gun_is_on		= false;
	iron_on			= true;
	update_screen = 0;
}

MODE* MDEBUG::loop(void) {
	DSPL*	pD		= &pCore->dspl;
	IRON*	pIron	= (jbc_selected)?&pCore->jbc:&pCore->t12;		// Use jbc if it is off-hook, otherwise use t12 iron
	HOTGUN*	pHG		= &pCore->hotgun;

	bool jbc_select = pCore->jbc.isReedSwitch(true);		// True means REED switch, not TILT one; if jbc_select, the JBC IRON is off-hook
	if (jbc_selected != jbc_select) {						// jbc iron has changed its status
		pIron->switchPower(false);							// Turn the current iron off, jbc or t12
		jbc_selected = jbc_select;
		pIron	= (jbc_selected)?&pCore->jbc:&pCore->t12;	// Change current iron
	}
	uint16_t pwr = pCore->u_enc.read();
	if (pwr != old_ip) {
		old_ip = pwr;
		update_screen = 0;
		iron_on = true;
		pIron->fixPower(pwr);
	}
	if (pCore->u_enc.buttonStatus()) {						// Switch the power off quickly
		iron_on = !iron_on;
		if (!iron_on)
			pIron->switchPower(false);
	}
	pwr = pCore->l_enc.read();
	if (pwr != old_fp) {
		old_fp = pwr;
		update_screen = 0;
		if (gun_is_on)
			pHG->setFan(pwr);
		else
			pHG->fixPower(0);
	}

	if (pHG->isReedSwitch(true)) {								// True means REED switch, not TILT one
		if (!gun_is_on) {
			pHG->setFan(old_fp);
			pHG->fixPower(gun_power);
			gun_is_on = true;
		}
	} else {
		if (gun_is_on) {
			pHG->fixPower(0);
			gun_is_on = false;
		}
	}

	if (pCore->l_enc.buttonStatus() == 2) {						// The Hot Air Gun button was pressed for a long time, exit debug mode
	   	return mode_lpress;
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + 491;						// The screen update period is a primary number to update TIM1 counter value

	uint16_t data[12];
	data[0]	= (jbc_selected)?0:old_ip;							// t12 power
	data[1]	= (jbc_selected)?old_ip:0;							// jbc power
	if (!iron_on) {
		data[0] = data[1] = 0;
	}

	data[2]	= pCore->t12.unitCurrent();							// t12 current
	data[3]	= pCore->jbc.unitCurrent();							// jbc current
	data[4]	= pCore->t12.temp();								// t12 temperature
	data[5]	= pCore->jbc.temp();								// jbc temperature
	data[6] = pIron->reedInternal();							// t12 or jbc internal tilt switch
	data[7]	= old_fp;											// Hot Air Gun power
	data[8]	= gtimPeriod();										// GUN_TIM period
	data[9]	= pHG->unitCurrent();
	data[10] = pCore->ambientInternal();
	data[11] = pHG->averageTemp();

	bool gtim_ok = isACsine() && (abs(data[8] - 1000) < 50);
	pD->debugShow(data, (!jbc_selected && old_ip > 0), (jbc_selected && old_ip > 0), pHG->isReedSwitch(true),
			pCore->t12.isConnected(), pCore->jbc.isConnected(), pHG->isConnected(),
			!pCore->hotgun.isReedSwitch(true), !pCore->jbc.isReedSwitch(true), pCore->jbc.isChanging(), gtim_ok);
	return this;
}

//---------------------- The Flash debug mode: display flash status & content ---
void FDEBUG::init(void) {
	msg = MSG_LAST;												// No error message yet
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_FLASH_DEBUG);
	if (!pCore->cfg.W25Q::mount()) {							// The flash can be already mounted
		if (!pCore->cfg.W25Q::reset()) {						// Check the flash size, ensure flash is connected
			msg		= MSG_EEPROM_READ;
			status	= FLASH_ERROR;
			return;
		}
		pCore->l_enc.reset(1, 0, 1, 1, 1, true);				// "Yes or No" dialog menu
		msg		= MSG_FORMAT_EEPROM;
		status	= FLASH_NO_FILESYSTEM;
		return;
	}
	readDirectory();
	update_screen = 0;
	status = FLASH_OK;
	delete_index	= -1;										// No file o be deleted selected
	confirm_format	= false;									// No confirmation dialog activated yet
}

MODE* FDEBUG::loop(void) {
	if (pCore->u_enc.buttonStatus() == 2) {						// Iron encoder button long press, load data from the SD-card
		pCore->cfg.umount();									// SPI FLASH will be mounted later to copy data files
		pCore->dspl.clear();
		pCore->dspl.dim(50);
		pCore->dspl.debugMessage("Copying files", 10, 100, 100);
		t_msg_id e = lang_loader.load();
		if (e == MSG_LAST) {
			pCore->buzz.shortBeep();
		} else {
			char sd_status[5];
			sprintf(sd_status, "%3d", lang_loader.sdStatus());	// SD status initialized by SD_Init() function (see sdspi.c)
			pFail->setMessage(e, sd_status);
			return pFail;
		}
		return mode_lpress;
	}

	uint8_t b_status = pCore->l_enc.buttonStatus();
	if (b_status == 2) {										// The button was pressed for a long time, finish mode
	   	return mode_lpress;
	}

	if (status == FLASH_OK) {									// Flash is OK, draw the directory list
		uint8_t f_index = (delete_index > 0)?delete_index:old_ge;
		if (b_status == 1  && pCore->cfg.canDelete(dir_list[f_index].c_str())) {
			if (delete_index >= 0) {
				if (pCore->l_enc.read() == 0) {					// Confirmed to delete file
					f_unlink(dir_list[delete_index].c_str());
					readDirectory();
				} else {
					pCore->l_enc.reset(delete_index, 0, dir_list.size()-1, 1, 1, false);
				}
				pCore->dspl.clear();
				pCore->dspl.drawTitle(MSG_FLASH_DEBUG);
				delete_index = -1;
			} else {
				pCore->l_enc.reset(1, 0, 1, 1, 1, true);		// Prepare to show confirmation dialog, "No" by default
				pCore->dspl.clear();
				delete_index = old_ge;
				old_ge = 1;
			}
			update_screen = 0;
		}
		uint16_t entry = pCore->l_enc.read();					// Directory entry number
		if (entry != old_ge) {
			old_ge = entry;
			update_screen = 0;
		}
	} else if (status == FLASH_NO_FILESYSTEM) {
		if (b_status == 1 && confirm_format) {					// Button pressed
			if (old_ge == 0) {									// "Yes" selected
				if (pCore->cfg.formatFlashDrive()) {			// Format flash drive
					if (pCore->cfg.W25Q::mount()) {				// mount flash drive
						c_dir = "/";
						readDirectory();
						msg		= MSG_LAST;
						status	= FLASH_OK;
						confirm_format = false;
						update_screen = 0;
					}
				}
			} else {
				return mode_lpress;
			}
		}
	}

	if (HAL_GetTick() < update_screen) return this;
	update_screen = HAL_GetTick() + update_timeout;
	if (status == FLASH_OK) {
		if (delete_index >= 0) {
			pCore->dspl.showDialog(MSG_DELETE_FILE, 50, old_ge == 0, dir_list[delete_index].c_str());
		} else {
			std::string fn = std::string("0:/") + dir_list[old_ge];	// Full path of the current file
			FILINFO file_info;
			std::string f_status = "-";
			if (FR_OK == f_stat(fn.c_str(), &file_info)) {
				if (file_info.fattrib & AM_DIR) {
					f_status = std::string("Directory");
				} else if (file_info.fattrib & AM_ARC) {
					f_status = std::string("Size: ");
					if (file_info.fsize >= 0x100000) {			// MB
						file_info.fsize >>= 20;
						f_status += std::to_string(file_info.fsize);
						f_status += "MB";
					} else if (file_info.fsize > 0x400) {		// KB
						file_info.fsize >>= 10;
						f_status += std::to_string(file_info.fsize);
						f_status += "KB";
					} else {
						f_status += std::to_string(file_info.fsize);
						f_status += "B";
					}
				}
			}
			pCore->dspl.directoryShow(dir_list, old_ge, f_status);
		}
	} else if (status == FLASH_NO_FILESYSTEM) {
		if (!confirm_format) {
			pCore->l_enc.reset(1, 0, 1, 1, 1, true);			// Prepare to show confirmation dialog, "No" by default
			confirm_format = true;
			old_ge = 1;
		}
		pCore->dspl.showDialog(msg, 50, old_ge == 0);
	} else {
		pCore->dspl.errorMessage(msg, 50);
	}
	return this;
}

void FDEBUG::readDirectory(void) {
	DIR	dir;
	c_dir = "/";
	FRESULT res = f_opendir(&dir, c_dir.c_str());
	if (res != FR_OK) {
		msg 	= MSG_EEPROM_DIRECTORY;
		status	= FLASH_NO_DIRECTORY;
		old_ge	= 0;
		pCore->l_enc.reset(0, 0, 0, 0, 0, false);					// No encoder change
		return;
	}
	FILINFO file_info;
	dir_list.clear();
	for (;;) {
		res = f_readdir(&dir, &file_info);
	    if ((res != FR_OK) || (file_info.fname[0] == '\0')) {
	    	break;
	    }
	    std::string entry = std::string(file_info.fname);
	    dir_list.push_back(entry);
	}
	f_closedir(&dir);
	old_ge = dir_list.size();										// Force to redraw screen
	pCore->l_enc.reset(0, 0, old_ge-1, 1, 1, false);				// Select directory entry by the rotary encoder
}

//---------------------- The Flash format mode: Confirm and format the flash ----
void FFORMAT::init(void) {
	p = 2;															// Make sure the message sill be displayed for the first time in the loop
	pCore->l_enc.reset(1, 0, 1, 1, 1, true);
	pCore->dspl.clear();
	pCore->dspl.drawTitle(MSG_EEPROM_READ);
	pCore->dspl.BRGT::set(80);										// Turn on the display backlight
	pCore->dspl.BRGT::on();
}

MODE* FFORMAT::loop(void) {
	uint8_t answer = pCore->l_enc.read();
	if (answer != p) {
		p = answer;
		pCore->dspl.showDialog(MSG_FORMAT_EEPROM, 100, answer == 0);
	}
	if (pCore->l_enc.buttonStatus() > 0) {
		if (answer == 0) {
			if (!pCore->cfg.formatFlashDrive()) {
				return 0;											// Failed to format the FLASH
			}
		}
		return mode_return;											// The main working mode
	}
	return this;
}
