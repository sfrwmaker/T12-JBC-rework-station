/*
 * hw.cpp
 *
 *  Created on: 13 June 2022
 *      Author: Alex
 *
 *  2022 DEC 23
 *     added the initialization of the hardware temperature into HW:init(); Initialize both IRONs, Hot Gun and ambient temperature.
 *     now the controller can detect the T12 iron handle is disconnected at very beginning
 */

#include <math.h>
#include "hw.h"

CFG_STATUS HW::init(uint16_t t12_temp, uint16_t jbc_temp, uint16_t gun_temp, uint16_t ambient) {
	dspl.init();
	t_amb.length(ambient_emp_coeff);
	t_amb.reset(ambient);
	t12.init(d_t12, t12_temp);
	jbc.init(d_jbc, jbc_temp);
	hotgun.init();
	hotgun.updateTemp(gun_temp);
	u_enc.start();
	l_enc.start();
	u_enc.addButton(I_ENC_B_GPIO_Port, I_ENC_B_Pin);
	l_enc.addButton(G_ENC_B_GPIO_Port, G_ENC_B_Pin);

	CFG_STATUS cfg_init = 	cfg.init();
	if (cfg_init == CFG_OK || cfg_init == CFG_NO_TIP) {		// Load NLS configuration data
		nls.init(&dspl);									// Setup pointer to NLS_MSG class instance to setup messages by NLS_MSG::set() method
		const char *l = cfg.getLanguage();					// Configured language name (string)
		nls.loadLanguageData(l);
		uint8_t *font = nls.font();
		dspl.setLetterFont(font);
		uint8_t r = cfg.getDsplRotation();
		dspl.rotate((tRotation)r);
	} else {
		dspl.setLetterFont(0);								// Set default font, reallocate the bitmap for 3-digits field
		dspl.rotate(TFT_ROTATION_90);
	}
	cfg.umount();
	PIDparam pp   		= 	cfg.pidParams(d_t12);			// load T12 IRON PID parameters
	t12.load(pp);
	pp   				= 	cfg.pidParams(d_jbc);			// load JBC IRON PID parameters
	jbc.load(pp);
	pp					=	cfg.pidParams(d_gun);			// load Hot Air Gun PID parameters
	hotgun.load(pp);
	buzz.activate(cfg.isBuzzerEnabled());
	u_enc.setClockWise(cfg.isUpperEncClockWise());
	l_enc.setClockWise(cfg.isLowerEncClockWise());
	return cfg_init;
}

/*
 * Return ambient temperature in Celsius
 * Caches previous result to skip expensive calculations
 */
int32_t	HW::ambientTemp(void) {
static const uint16_t add_resistor	= 10000;				// The additional resistor value (10koHm)
static const float 	  normal_temp[2]= { 10000, 25 };		// nominal resistance and the nominal temperature
static const uint16_t beta 			= 3950;     			// The beta coefficient of the thermistor (usually 3000-4000)
static int32_t	average 			= 0;					// Previous value of analog read
static int 		cached_ambient 		= 0;					// Previous value of the temperature

	if (noAmbientSensor()) return default_ambient;			// If IRON handle is not connected, return default ambient temperature
	if (abs(t_amb.read() - average) < 20)
		return cached_ambient;

	average = t_amb.read();

	if (average < max_ambient_value) {						// prevent division by zero; About -30 degrees
		// convert the value to resistance
		float resistance = 4095.0 / (float)average - 1.0;
		resistance = (float)add_resistor / resistance;

		float steinhart = resistance / normal_temp[0];		// (R/Ro)
		steinhart = log(steinhart);							// ln(R/Ro)
		steinhart /= beta;									// 1/B * ln(R/Ro)
		steinhart += 1.0 / (normal_temp[1] + 273.15);  		// + (1/To)
		steinhart = 1.0 / steinhart;						// Invert
		steinhart -= 273.15;								// convert to Celsius
		cached_ambient	= round(steinhart);
	} else {
		cached_ambient	= default_ambient;
	}
	return cached_ambient;
}
