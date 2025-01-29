/*
 * hw.cpp
 *
 *  Created on: 13 June 2022
 *      Author: Alex
 *
 *  2022 DEC 23
 *     added the initialization of the hardware temperature into HW:init(); Initialize both IRONs, Hot Gun and ambient temperature.
 *     now the controller can detect the T12 iron handle is disconnected at very beginning
 *  2024 OCT 01, v.1.07
 *  	Modified the HW::init() to implement fast Hot Gun cooling feature
 *  2024 OCT 10, v.1.07
 *		Added support of IPS display type in HW::init()
 *	2024 NOV 10, v.1.08
 *		Modified HW::init() to initialize the vrefint and t_stm32
 *		Modified HW::ambientTemp() to calculate the MCU internal temperature
 *	2024 NOV 14
 *		Modified HW::ambientTemp() to correctly calculate the stm32 internal temperature
 *  2025 JAN 28
 *		Separate ambientTemp() into two routines to calculate stm32 temperature and steinhart sensor temperature inside Hakko T12 handle
 *		Save MCU internal temperature at startup to adjust internal temperature. As soon as the MCU temperature is higher than actual ambient temperature,
 *		return average value between MCU temperature and MCU temperature at startup.
 *		Changed the internalTemp() algorithm to read the calibration data from the controller registers
 */

#include <math.h>
#include "hw.h"
#include "tools.h"

CFG_STATUS HW::init(uint16_t t12_temp, uint16_t jbc_temp, uint16_t gun_temp, uint16_t ambient, uint16_t vref, uint32_t t_mcu) {
	t_amb.length(ambient_emp_coeff);
	t_amb.reset(ambient);
	vrefint.length(ambient_emp_coeff);
	vrefint.reset(vref);
	t_stm32.length(ambient_emp_coeff);
	t_stm32.reset(t_mcu);
	t12.init(d_t12, t12_temp);
	jbc.init(d_jbc, jbc_temp);
	hotgun.init();
	hotgun.updateTemp(gun_temp);
	u_enc.start();
	l_enc.start();
	u_enc.addButton(I_ENC_B_GPIO_Port, I_ENC_B_Pin);
	l_enc.addButton(G_ENC_B_GPIO_Port, G_ENC_B_Pin);
	start_temp = internalTemp(t_mcu);						// Save temperature at controller startup

	cfg.keepMounted(true);									// Do not umount FLASH drive until complete initialization
	CFG_STATUS cfg_init = 	cfg.init();
	if (cfg_init == CFG_OK || cfg_init == CFG_NO_TIP) {		// Load NLS configuration data
		dspl.init(cfg.isIPS() && !l_enc.buttonPressed());
		nls.init(&dspl);									// Setup pointer to NLS_MSG class instance to setup messages by NLS_MSG::set() method
		const char *l = cfg.getLanguage();					// Configured language name (string)
		nls.loadLanguageData(l);
		uint8_t *font = nls.font();
		dspl.setLetterFont(font);
		uint8_t r = cfg.getDsplRotation();
		dspl.rotate((tRotation)r);
	} else {
		dspl.init();
		dspl.setLetterFont(0);								// Set default font, reallocate the bitmap for 3-digits field
		dspl.rotate(TFT_ROTATION_90);
	}
	cfg.keepMounted(false);									// Now the FLASH drive can be unmount for safety data
	cfg.umount();
	PIDparam pp   		= 	cfg.pidParams(d_t12);			// load T12 IRON PID parameters
	t12.load(pp);
	pp   				= 	cfg.pidParams(d_jbc);			// load JBC IRON PID parameters
	jbc.load(pp);
	pp					=	cfg.pidParams(d_gun);			// load Hot Air Gun PID parameters
	hotgun.load(pp);
	bool fast_cooling	=	cfg.isFastGunCooling();
	hotgun.setFastGunCooling(fast_cooling);
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
	static int32_t	raw_ambient 			= 0;			// Previous value of ambient temperature (RAW)
	static int32_t 	cached_ambient 			= 0;			// Previous value of the ambient temperature
	static int32_t	raw_stm32				= 0;			// Previous value of MCU temperature (RAW)
	static int32_t	cached_stm32			= 0;			// Previous value of the MCU temperature

	if (noAmbientSensor()) {								// No T12 IRON handle is connected, calculate MCU internal temperature
		if (abs(t_stm32.read() - raw_stm32) < 4)			// About 1 Celsius degree
			return cached_stm32;

		raw_stm32 = t_stm32.read();
		cached_stm32 = internalTemp(raw_stm32);
		cached_stm32 += start_temp + 1;					// Return average temperature because the MCU is hotter than soldering handle
		cached_stm32 >>= 1;
		return cached_stm32;
	}
	if (abs(t_amb.read() - raw_ambient) < 25)				// About 1 Celsius degree
		return cached_ambient;
	raw_ambient = t_amb.read();

	if (raw_ambient < max_ambient_value) {					// prevent division by zero; About -30 degrees
		cached_ambient = steinhartTemp(raw_ambient);
	} else {
		cached_ambient	= default_ambient;
	}
	return cached_ambient;
}

/*
int32_t HW::internalTemp(int32_t raw_stm32) {
	// The following constants are from STM32F446 datasheet; Do not use for other MCU
	static const int32_t	v_ref_int		= 12100;			// Internal voltage reference (1.21 * 10000)
	static const int32_t	v_at_25c		= 7600;				// Internal voltage at 25 degrees (0.76 * 10000)
	static const int32_t	avg_slope		= 25000;			// AVG Slope (2.5 * 10000)

	// v_sense = (float)(raw_stm32 * v_ref_int/vrefint_v)
    // Temperature = (((v_at_25c - v_sense) * 1000.0) /avg_slope) + 25.0;
	int32_t vrefint_v = vrefint.read();
	int32_t v_sense = (raw_stm32 * v_ref_int + (vrefint_v>>1)) / vrefint_v;  // *10000
	return ((v_at_25c - v_sense) * 1000 + (avg_slope>>1)) / avg_slope + 25;
}
*/

int32_t HW::internalTemp(int32_t raw_stm32) {
	const uint16_t v_ref_int	= *(uint16_t*)0x1FFF7A2A;
	const uint16_t ts_cal1		= *(uint16_t*)0x1FFF7A2C;
	const uint16_t ts_cal2		= *(uint16_t*)0x1FFF7A2E;

	int32_t vrefint_v = vrefint.read();
	int32_t v_sense = (raw_stm32 * v_ref_int + (vrefint_v>>1)) / vrefint_v;
	return emap(v_sense, ts_cal1, ts_cal2, 30, 110);
}

int32_t HW::steinhartTemp(int32_t raw_ambient) {
	static const uint16_t	add_resistor	= 10000;		// The additional resistor value (10koHm)
	static const float 	  	normal_temp[2]= { 10000, 25 };	// nominal resistance and the nominal temperature
	static const uint16_t 	beta 			= 3950;     	// The beta coefficient of the thermistor (usually 3000-4000)

	// convert the value to resistance
	float resistance = 4095.0 / (float)raw_ambient - 1.0;
	resistance = (float)add_resistor / resistance;

	float steinhart = resistance / normal_temp[0];		// (R/Ro)
	steinhart = log(steinhart);							// ln(R/Ro)
	steinhart /= beta;									// 1/B * ln(R/Ro)
	steinhart += 1.0 / (normal_temp[1] + 273.15);  		// + (1/To)
	steinhart = 1.0 / steinhart;						// Invert
	steinhart -= 273.15;								// convert to Celsius
	return round(steinhart);
}

