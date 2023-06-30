/*
 * iron.cpp
 *
 * 2023 FEB 18, v1.01
 *  Added stable constant that describes the power in stable mode. See POWER_HEATING case in IRON::power()
 * 2023 FEB 19, v1.01
 *  Changed PID::init() call in IRON::init(). Both irons do use the aggressive PID parameters when heat-up.
 * 2023 MAR 01, v1.01
 *  Changed IRON::lowPowerMode() switch mode to the POWER_ON in case low power mode activation
 */

#include "iron.h"
#include "tools.h"

void IRON::init(tDevice dev_type, uint16_t temp) {
	mode		= POWER_COOLING;
	fix_power	= 0;
	chill		= false;
	temp_boost	= 0;
	t_reset		= true;										// This flag indicating the temperature value was reset
	UNIT::init(iron_sw_len, iron_off_value,	iron_on_value,
			(dev_type == d_t12)?sw_tilt_len:sw_jbc_len,	sw_off_value, sw_on_value);
	max_power = (TIM5->CCR4 - 40) << 1;						// Max value should be less than TIM5.CH4 value by 40. The Irons are checked consequently, so the value doubled
	t_iron_short.length(iron_emp_coeff);
	t_iron_short.reset(temp);
	h_power.length(ec);
	h_temp.length(ec);
	h_temp.reset(temp);
	d_power.length(ec);
	d_temp.length(ec);

	uint32_t tim5_period = (TIM5->PSC + 1) * (TIM5->ARR + 1);
	uint32_t cpu_speed = SystemCoreClock / 1000;			// Calculate TIM5 period in ms
	tim5_period /= cpu_speed;
	tim5_period <<= 1;										// Double period because the IRONS are checking consequently (see core.cpp)
	PID::init(tim5_period, 11, true);						// Initialize PID for JBC or T12 IRON.
	resetPID();
}

void IRON::switchPower(bool On) {
	if (!On) {
		fix_power	= 0;
		if (mode != POWER_OFF) {
			mode = POWER_COOLING;							// Start the cooling process
			TIM5->CCR1	= 0;
			TIM5->CCR2	= 0;
		}
	} else {
		resetPID();
		uint16_t t = h_temp.read();
		if (t < temp_set && t + 200 < temp_set) {
			mode		= POWER_HEATING;
		} else {
			mode		= POWER_ON;
		}
	}
	h_power.reset();
	d_power.reset();
	temp_low	= 0;										// Disable low power mode
	temp_boost 	= 0;										// Disable boost mode
}

void IRON::autoTunePID(uint16_t base_pwr, uint16_t delta_power, uint16_t base_temp, uint16_t temp) {
	mode = POWER_PID_TUNE;
	h_power.reset();
	d_power.reset();
	PIDTUNE::start(base_pwr,delta_power, base_temp, temp);
}

void IRON::setTemp(uint16_t t) {
	if (mode == POWER_ON) resetPID();
	if (t > int_temp_max) t = int_temp_max;					// Do not allow over heating. int_temp_max is defined in vars.cpp
	temp_set = t;
	uint16_t ta = h_temp.read();
	chill = (ta > t + 20);                         			// The IRON must be cooled
}

uint16_t IRON::avgPower(void) {
	uint16_t p = h_power.read();
	if (mode == POWER_FIXED)
		p = fix_power;
	if (p > max_power) p = max_power;
	return p;
}

uint8_t IRON::avgPowerPcnt(void) {
	uint16_t p 		= h_power.read();
	uint16_t max_p 	= max_power;
	if (mode == POWER_FIXED) {
		p	  = fix_power;
		max_p = max_fix_power;
	} else if (mode == POWER_PID_TUNE) {
		max_p = max_fix_power;
	}
	p = constrain(p, 0, max_p);
	return map(p, 0, max_p, 0, 100);
}

void IRON::fixPower(uint16_t Power) {
	h_power.reset();
	d_power.reset();
	if (Power == 0) {										// To switch off the IRON, set the Power to 0
		fix_power 	= 0;
		mode		= POWER_COOLING;
		return;
	}

	if (Power > max_fix_power)
		fix_power 	= max_fix_power;

	fix_power 	= Power;
	mode		= POWER_FIXED;
}

void IRON::adjust(uint16_t t) {
	if (t > int_temp_max) t = int_temp_max;					// Do not allow over heating
	temp_set = t;
}

// Called from HAL_ADC_ConvCpltCallback() event handler. See core.cpp for details.
uint16_t IRON::power(int32_t t) {
	if (t_reset) {
		t_iron_short.reset(t);
		h_temp.reset(t);
		t_reset = false;
	}
	t	= tempShortAverage(t);								// Prevent temperature deviation using short term history average

	temp_curr		= t;
	int32_t at 		= h_temp.average(temp_curr);
	int32_t diff	= at - temp_curr;
	d_temp.update(diff*diff);
	bool overheat = false;
	if (t >= int_temp_max + 100) {							// Prevent global over heating
		int32_t	ap		= h_power.average(0);
		diff 			= ap - 0;
		d_power.update(diff*diff);
		overheat = true;
	}

	int32_t p = 0;
	switch (mode) {
		case POWER_COOLING:
			if (at < iron_cold)
				mode = POWER_OFF;							// no break in this case because of check current through the IRON
		case POWER_OFF:
			if (check_period && --check_time == 0) {		// It is time to check the current through the IRON
				check_time = check_period;
				p = 2;
			}
			break;
		case POWER_HEATING:
			if (t >= temp_set + 20) {
				mode = POWER_ON;
				PID::pidStable(stable);
			}
			p = PID::reqPower(temp_set, t);
			p = constrain(p, 0, max_power);
			break;
		case POWER_ON:
			if (!overheat) {
				uint16_t t_set = temp_set;
				if (temp_low) {
					t_set = temp_low;						// If temp_low setup, turn-on low power mode
				} else if (temp_boost) {
					t_set = temp_boost;
					if (t > (temp_boost + 100)) 			// Preset temperature overheating
						chill = true;
				} else 	if ((t >= int_temp_max + 100) || (t > (temp_set + 400))) {	// Prevent global over heating
					chill = true;
				}
				if (chill) {
					if (t < (t_set - 2)) {
						chill = false;
						resetPID(t);
					} else {								// Do not supply power, wait the IRON get colder
						break;
					}
				}
				p = PID::reqPower(t_set, t);
				p = constrain(p, 0, max_power);
			}
			break;
		case POWER_FIXED:
			if (!overheat) {
				p = fix_power;
			}
			break;
		case POWER_PID_TUNE:
			if (!overheat) {
				p = PIDTUNE::run(t);
			}
			break;
		case POWER_BOOST:									// Fast heating to the boost temperature
			if (!overheat) {
				if (temp_boost > 0 && temp_boost + 50 > t) {
					p = fix_power;
				} else {
					mode = POWER_ON;
					fix_power	= 0;
					resetPID(t);
				}
			}
			break;
		default:
			break;
	}

	int32_t	ap		= h_power.average(p);
	diff 			= ap - p;
	d_power.update(diff*diff);
	return p;
}

void IRON::reset(void) {
	t_reset		= true;										// This flag indicating the temperature value was reset
	resetShortTemp();
	h_power.reset();
	h_temp.reset();
	d_power.reset();
	d_temp.reset();
	mode = POWER_COOLING;									// New tip inserted, clear COOLING mode
}


void IRON::lowPowerMode(uint16_t t) {
    if ((mode == POWER_ON || mode == POWER_HEATING) && t < temp_set) {
        temp_low = t;                           			// Activate low power mode
        chill = true;										// Stop heating, when temp reaches standby one, reset PID
    	h_power.reset();
    	d_power.reset();
    	mode = POWER_ON;
    }
}

void IRON::boostPowerMode(uint16_t t) {
	if (mode == POWER_ON && t > temp_set) {
	    temp_boost = t;                           			// Activate boost power mode
	    mode = POWER_BOOST;
	   	h_power.reset();
	   	d_power.reset();
	   	fix_power = max_fix_power;
	}
}

