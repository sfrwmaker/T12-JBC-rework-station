/*
 * core.cpp
 *
 *  Created on: 2022 Dec 3
 *      Author: Alex
 *
 *  Hardware configuration:
 *  Analog pins:
 *  A2	- T12 current, ADC3
 *  A3	- JBC current, ADC3
 *  A4	- T12 temperature, ADC1 & ADC2
 *  A5	- JBC temperature, ADC1 & ADC2
 *  A6	- GUN temperature, ADC1 & ADC2
 *  A7  - Ambient temperature, ADC1 & ADC2
 *  TIM1:
 *  A12	- TIM1_ETR, AC zero signal read - the timer reset signal
 *  A11	- TIM1_CH4, Hot Air Gun power (0 or 70) per each half-period shape
 *  TIM3:
 *  C6	- TIM3_CH1, I_ENC_L
 *  C7	- TIM3_CH2, I_ENC_R
 *  TIM4:
 *  B6	- TIM4_CH1, G_ENC_L
 *  B7	- TIM4_CH2, G_ENC_R
 *  TIM5:
 *  A0	- TIM5_CH1, T12 power [0-1999]
 *  A1	- TIM5_CH2,	JBC_power [0-1999]
 *  	  TIM5_CH3, Output compare (1) to check current
 *  	  TIM5_CH4, Output compare (1960) to check temperature
 *  TIM10:
 *  B8	- TIM10_CH1, Buzzer
 *  TIM11:
 *  B9	- TIM11_CH1, Fan power
 *  TIM12:
 *  B14	- TIM12_CH1, TFT brightness
 *
 *  The hardware (Hot Air Gun, T12 iron and JBC iron) can work simultaneously.
 *  Both T12 and JBC irons require very high power so, the power supplied to them consequently
 *  TIM5 timer manages the supplied power to both irons, CH1 is used to power T12 iron, CH2 is used to power JBC iron.
 *  To reduce the power consumption, the TIM5 implements two phases: JBC_PHASE and T12_PHASE.
 *  During JBC_PHASE, the power is supplying to the JBC iron only, at the end of the phase (CH4 compare interrupt)
 *  the controller checks the T12 temperature and calculates the required power of T12 iron and switch the phase to the T12.
 *
 *  How the Hot Air Gun powered since v.1.11
 *  The AC power outlet frequency is 50 Hz in Europe and 60 Hz in USA. The AC signal after full bridge rectifier has a positive half-period
 *  shapes and its frequency is 100 Hz or 120 Hz respectively. This signal is coming from the power board as a AC_ZERO interrupts. The TIM1
 *  timer is clocked by the internal clock and has period of 25.600 mS (the timer counter ticks from 0 to 255).  The period between two
 *  AC_ZERO signals is 10 mS (8.33 ms in USA). The AC_ZERO signal resets the TIM1 counter, so the TIM1 counter never reached its period for sure.
 *  To avoid the AC source distortion, the TRIAC in the power board are managed to propagate whole halh-period shape or to completely block it.
 *  To manage the power of the Hot Air Gun, the controller uses a means of 120 half-period shapes (1.2 secs in Europe or 1 sec is USA) because
 *  the number 120 has a multiple dividers: 2,3,4,5,6, etc. It is easy to distribute many active half-period shapes from 0 to 120 in 120-element
 *  array evenly. The 120-element array power data is sent via DMA channel to the TIM1_CH4 channel and manages the TRIAC. Every half of period (60)
 *  is a time of checking the Hot Air Gun temperature and calculate the required power to be applied. See HAL_TIM_PWM_PulseFinishedHalfCpltCallback()
 *  and HAL_TIM_PWM_PulseFinishedCallback() callbacks. As soon as required power calculated (0-120 half-period shapes) this number of half-period
 *  shapes is distributed into 120 elements DMA buffer (gun_pwr) by the calculateGunPowerData() routine. Active pulse encoded as 70 (TIM1 ticks)
 *  and inactive pulse is encoded as 0. As mentioned before, the TIM1 timer counts from 0 to 100 (or 83 in USA) before AC_ZERO interrupt resets
 *  the timer and the 70-ticks long active pulse activates the TRIAC at the AV wave beginning and goes down before the sine pulse ends, but
 *  the TRIAC keeps open until the AC sine wave goes through the zero, so the half-period shape will propagate to the Hot Air Gun completely.
 *  From the other side, the TIM1 PWM channel goes down at 70-th timer tick and power-off the Hot Air Gun at the beginning of the next half-period
 *  shape for sure.
 *
 *  2024 MAR 30
 *  	Changed void setup(). When the FLASH failed to read, the fail mode would return to flash_debug mode only
 *  	Added active.setFail() call
 *  2024 OCT 13, v.1.07
 *  	Added gun_menu instance
 *  	Modified parameters in the main_menu constructor
 *  	Modified setup() function to configure gun_menu instance
 *  2024 NOV 10, v.1.08
 *  	Added internal temperature and vrefint readings on ADC1
 *  		Modified setup() and HAL_ADC_ConvCpltCallback()
 *  2025 SEP 15, v.1.10
 * 		Changed the algorithm how the Hot Air Gun managed, see detailed description below
 * 		Changed the TIM1 initialization
 * 		Created gun_pwr[] DMA buffer to transfer the power parameter to the TIM1_CH4
 * 		Created calculateGunPowerData() and powerOffGun() routines
 */

#include <math.h>
#include "core.h"
#include "hw.h"
#include "mode.h"
#include "work_mode.h"
#include "menu.h"
#include "vars.h"

#define ADC_T12 	(4)										// Activated ADC Ranks Number (hadc1.Init.NbrOfConversion)
#define ADC_JBC 	(2)										// Activated ADC Ranks Number (hadc2.Init.NbrOfConversion)
#define ADC_CUR		(3)										// Activated ADC Ranks Number (hadc3.Init.NbrOfConversion)
#define MAX_GUN_POWER		(120)

extern ADC_HandleTypeDef	hadc1;
extern ADC_HandleTypeDef	hadc2;
extern ADC_HandleTypeDef	hadc3;
extern TIM_HandleTypeDef	htim1;
extern TIM_HandleTypeDef	htim5;
extern TIM_HandleTypeDef	htim11;

typedef enum { ADC_IDLE, ADC_CURRENT, ADC_TEMP } t_ADC_mode;
volatile static t_ADC_mode	adc_mode = ADC_IDLE;
volatile static uint16_t	t12_buff[ADC_T12];				// Hakko T12 temperature and ambient temperature sensor in T12 handle
volatile static uint16_t	jbc_buff[ADC_JBC];				// JBC temperature and Hot Air Gun temperature
volatile static uint16_t	cur_buff[ADC_CUR];				// Currents: T12, JBC, Fan
volatile static	uint32_t	tim1_cntr	= 0;				// Previous value of TIM1 counter. Using to check the TIM1 value changing
volatile static	bool		ac_sine		= false;			// Flag indicating that TIM1 is driven by AC power interrupts on AC_ZERO pin
volatile static bool		jbc_phase	= true;				// JBC or T12 active phase, see description above
volatile static uint16_t	t12_power	= 0;				// Calculated power of T12 iron
volatile static uint16_t	jbc_power	= 0;				// Calculated power of JBC iron
volatile static uint16_t	gun_pwr[MAX_GUN_POWER*2] = {0};	// The HOT GUN power PWM buffer
static	EXPA				gtim_period;					// gun timer period (ms)
static  uint16_t  			max_iron_pwm	= 0;			// Max value should be less than TIM5.CH3 value by 40. Will be initialized later
volatile static uint32_t	gtim_last_ms	= 0;			// Time when the gun timer became zero
const static	uint16_t  	max_gun_pwm		= 99;			// TIM1 period. Full power can be applied to the HOT GUN
const static	uint32_t	check_sw_period = 100;			// IRON switches check period, ms

static HW		core;										// Hardware core (including all device instances)

// MODE instances
static	MWORK			work(&core);
static	MSLCT			iselect(&core);
static	MTACT			activate(&core);
static	MCALIB			calib_auto(&core);
static	MCALIB_MANUAL	calib_manual(&core);
static	MCALMENU		calib_menu(&core, &calib_auto, &calib_manual);
static	MFAIL			fail(&core);
static	MTPID			manual_pid(&core);
static 	MAUTOPID		auto_pid(&core);
static	MENU_PID		pid_menu(&core, &manual_pid, &auto_pid);
static  MENU_FLASH		flash_menu(&core, &fail);
static	FDEBUG			flash_debug(&core, &flash_menu);
static  MABOUT			about(&core, &flash_debug);
static  MDEBUG			debug(&core);
static	FFORMAT			format(&core);
static	MSETUP			param_menu(&core, &pid_menu);
static	MENU_T12		t12_menu(&core, &calib_menu);
static	MENU_JBC		jbc_menu(&core, &calib_menu);
static  MENU_GUN		gun_menu(&core, &calib_manual);
static	MMENU			main_menu(&core, &iselect, &param_menu, &activate, &t12_menu, &jbc_menu, &gun_menu, &about);
static	MODE*           pMode = &work;

bool 		isACsine(void)		{ return ac_sine; 				}
uint16_t	gtimPeriod(void)	{ return gtim_period.read();	}

// Synchronize TIM5 timer to AC power. The main timer managing T12 & JBC irons
static uint16_t syncAC(uint16_t tim_cnt) {
	uint32_t to = HAL_GetTick() + 300;						// The timeout
	while (HAL_GetTick() < to) {							// Prevent hang
		if (TIM1->CNT == 0) {								// AC_ZERO interrupt restarts the TIM1 timer
			TIM5->CNT = tim_cnt;							// Synchronize TIM5 to AC power zero crossing signal
			break;
		}
	}
	// Checking the TIM5 has been synchronized
	to = HAL_GetTick() + 300;
	while (HAL_GetTick() < to) {
		if (TIM1->CNT == 0) {
			return TIM2->CNT;
		}
	}
	return TIM5->ARR+1;										// This value is bigger than TIM5 period, the TIM5 has not been synchronized
}

// Calculates the PWM value data for TIMER to supply power to the heater
// Each AC-outlet peak (100 Hz in Russia and 60 Hz in US) resets the timer and make the timer to supply power
// The PWM values can be in two states: supply power for the half-period (peak) or not
static void calculateGunPowerData(volatile uint16_t *data, uint8_t max_power, uint8_t pwr) {
	const uint8_t active_pulse = 70;
	uint8_t on	= active_pulse;
	uint8_t off = 0;
	if (pwr > (max_power >> 1)) {							// In case the pwr is greater than half of maximum power, calculate positions of "empty" peaks
		if (pwr > max_power) pwr = max_power;
		on	= 0;
		off = active_pulse;
		pwr = max_power - pwr;
	}
	if (pwr == 0) {											// No power supplied at all, empty all PWM slots
		for (uint8_t i = 0; i < max_power; ++i)
			data[i] = off;
		return;
	}
	uint8_t slots	= max_power / pwr;						// Number of PWM slots per each "powered" peak (0 .. max_power/2)
	uint8_t remain	= max_power % pwr;						// The division remainder
	uint8_t pos		= slots >> 1;							// Put the "powered" peak in to the center of the slot
	int8_t extra	= 0;									// Extra position remainder (extra/pwr)
	for (uint8_t i = 0; i < max_power; ++i) {
		if (i < pos) {
			data[i] = off;
		} else {
			data[i] = on;
			pos += slots;
			extra += remain;
			if (extra + (remain>>1) >= pwr) {
				++pos;
				extra -= pwr;
			}
		}
	}
}

static void powerOffGun(void) {
	for (uint16_t i = 0; i < MAX_GUN_POWER * 2; ++i) {
		gun_pwr[i] = 0;
	}
}

extern "C" void setup(void) {
	TIM12->CCR1 = 0;										// Do turn-off the display backlight
	// Read temperature values
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 100);
	uint16_t t12_temp	= HAL_ADC_GetValue(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 100);
	uint16_t ambient	= HAL_ADC_GetValue(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 100);
	uint16_t vref	= HAL_ADC_GetValue(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 100);
	uint16_t t_mcu	= HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);
	HAL_ADC_Start(&hadc2);
	HAL_ADC_PollForConversion(&hadc2, 100);
	uint16_t jbc_temp 	= HAL_ADC_GetValue(&hadc2);
	HAL_ADC_PollForConversion(&hadc2, 100);
	uint16_t gun_temp 	= HAL_ADC_GetValue(&hadc2);
	HAL_ADC_Stop(&hadc2);
	gtim_period.length(10);
	gtim_period.reset(1000);								// Default TIM1 period, ms
	max_iron_pwm	= htim5.Instance->CCR4 - 40;			// Max value should be less than TIM5.CH4 value by 40.

	CFG_STATUS cfg_init = core.init(t12_temp, jbc_temp, gun_temp, ambient, vref, t_mcu);
	core.t12.setCheckPeriod(3);								// Start checking the T12 IRON connectivity
	HAL_TIM_Base_Start_IT(&htim1);
	HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_4, (const uint32_t*)gun_pwr, MAX_GUN_POWER*2); // PWM signal of Hot Air Gun
	HAL_TIM_PWM_Start(&htim5, 	TIM_CHANNEL_1);				// PWM signal of the T12 IRON
	HAL_TIM_PWM_Start(&htim5, 	TIM_CHANNEL_2);				// PWM signal of the JBC IRON
	HAL_TIM_OC_Start_IT(&htim5, TIM_CHANNEL_3);				// Check the current through the IRON and FAN
	HAL_TIM_OC_Start_IT(&htim5, TIM_CHANNEL_4);				// Calculate power of the IRON, also check ambient temperature
	HAL_TIM_PWM_Start(&htim11, 	TIM_CHANNEL_1);				// Fan power (was TIM2->CH3)

	// Setup main mode parameters: return mode, short press mode, long press mode
	work.setup(&main_menu, &iselect, &main_menu);
	iselect.setup(&work, &activate, &main_menu);
	activate.setup(&work, &work, &main_menu);
	activate.setFail(&fail);
	calib_auto.setup(&work, &work, &work);
	calib_manual.setup(&calib_menu, &work, &work);
	calib_menu.setup(&work, &work, &work);
	fail.setup(&work, &work, &work);
	manual_pid.setup(&work, &work, &work);
	auto_pid.setup(&work, &manual_pid, &manual_pid);
	pid_menu.setup(&main_menu, &work, &work);
	param_menu.setup(&main_menu, &work, &work);
	t12_menu.setup(&main_menu, &work, &work);
	jbc_menu.setup(&main_menu, &work, &work);
	gun_menu.setup(&main_menu, &work, &work);
	main_menu.setup(&work, &work, &work);
	about.setup(&work, &work, &debug);
	debug.setup(&work, &work, &work);
	flash_menu.setup(&work, &work, &work);
	flash_debug.setup(&fail, &work, &work);
	auto_pid.setup(&work, &manual_pid, &manual_pid);
	format.setup(&work, 0, 0);

	core.dspl.clear();
	switch (cfg_init) {
		case CFG_NO_TIP:
			pMode	= &activate;							// No tip configured, run tip activation menu
			break;
		case CFG_READ_ERROR:								// Failed to read FLASH
			fail.setMessage(MSG_EEPROM_READ);
			fail.setup(&fail, &fail, &flash_debug);			// Do not enter the main working mode
			pMode	= &fail;
			break;
		case CFG_NO_FILESYSTEM:
			fail.setMessage(MSG_FORMAT_FAILED);				// Prepare the fail message
			pMode	= &format;
			break;
		default:
			break;
	}

	syncAC(0);												// Synchronize TIM5 timer to AC power. Parameter is TIM5 counter value when TIM1 become zero
	uint8_t br = core.cfg.getDsplBrightness();
	core.dspl.BRGT::set(br);
	// Turn-on the display backlight immediately in the debug mode
#ifdef DEBUG_ON
	core.dspl.BRGT::on();
#endif
	HAL_Delay(500);											// Wait at least 0.5s to update the T12 iron tip connection status
	pMode->init();
}


extern "C" void loop(void) {
	static uint32_t AC_check_time	= 0;					// Time in ms when to check TIM1 is running
	static uint32_t	check_sw		= 0;					// Time when check iron switches status (ms)

	if (HAL_GetTick() > check_sw) {
		check_sw = HAL_GetTick() + check_sw_period;
		GPIO_PinState pin = HAL_GPIO_ReadPin(TILT_SW_GPIO_Port, TILT_SW_Pin);
		core.t12.updateReedStatus(GPIO_PIN_SET == pin);		// Update T12 TILT switch status
		pin = HAL_GPIO_ReadPin(JBC_STBY_GPIO_Port, JBC_STBY_Pin);
		core.jbc.updateReedStatus(GPIO_PIN_SET == pin);		// Switch active when the JBC handle is off-hook
		pin = HAL_GPIO_ReadPin(JBC_CHANGE_GPIO_Port, JBC_CHANGE_Pin);
		core.jbc.updateChangeStatus(GPIO_PIN_RESET == pin);	// Switch active when the JBC tip on change connector
		pin = HAL_GPIO_ReadPin(REED_SW_GPIO_Port, REED_SW_Pin);
		core.hotgun.updateReedStatus(GPIO_PIN_SET == pin);	// Switch active when the Hot Air Gun handle is off-hook
	}

	MODE* new_mode = pMode->returnToMain();
	if (new_mode && new_mode != pMode) {
		core.buzz.doubleBeep();
		core.t12.switchPower(false);
		core.jbc.switchPower(false);
		TIM5->CCR1	= 0;									// Switch-off the IRON power immediately
		TIM5->CCR2  = 0;
		pMode->clean();
		pMode = new_mode;
		pMode->init();
		return;
	}
	new_mode = pMode->loop();
	if (new_mode != pMode) {
		if (new_mode == 0) new_mode = &fail;				// Mode Failed
		core.t12.switchPower(false);
		core.jbc.switchPower(false);
		core.hotgun.switchPower(false);
		core.t12.setCheckPeriod(0);							// Stop checking t12 IRON
		core.jbc.setCheckPeriod(0);							// Stop checking JBC IRON
		TIM5->CCR1	= 0;									// Switch-off the IRON power immediately
		TIM5->CCR2	= 0;
		pMode->clean();
		pMode = new_mode;
		pMode->init();
	}

	// If TIM1 counter has been changed since last check, we received AC_ZERO events from AC power
	if (HAL_GetTick() >= AC_check_time) {
		ac_sine		= (TIM1->CNT != tim1_cntr);
		tim1_cntr	= TIM1->CNT;
		AC_check_time = HAL_GetTick() + 41;					// 50Hz AC line generates 100Hz events. The pulse period is 10 ms
	}

	// Adjust display brightness
	if (core.dspl.BRGT::adjust()) {
		HAL_Delay(5);
	}
}

static bool adcStartCurrent(void) {							// Check the current by ADC3
    if (adc_mode != ADC_IDLE) {								// Not ready to check analog data; Something is wrong!!!
    	TIM5->CCR1 = 0;										// Switch off the T12 IRON
    	TIM5->CCR2 = 0;										// Switch off the JBC IROM
    	TIM1->CCR4 = 0;										// Switch off the Hot Air Gun
		return false;
    }
    HAL_ADC_Start_DMA(&hadc3, (uint32_t*)cur_buff, ADC_CUR);
	adc_mode = ADC_CURRENT;
	return true;
}

static bool adcStartTemp(void) {							// Check the temperature by ADC1 & ADC2
    if (adc_mode != ADC_IDLE) {								// Not ready to check analog data; Something is wrong!!!
    	TIM5->CCR1 = 0;										// Switch off the T12 IRON
    	TIM5->CCR2 = 0;										// Switch off the JBC IROM
    	TIM1->CCR4 = 0;										// Switch off the Hot Air Gun
		return false;
    }
    if (jbc_phase) {
    	HAL_ADC_Start_DMA(&hadc2, (uint32_t*)jbc_buff, ADC_JBC);
    } else {
    	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)t12_buff, ADC_T12);
    }
	adc_mode = ADC_TEMP;
	return true;
}

/*
 * IRQ handler
 * on TIM5 Output channel #3 to read the current through the IRONs and Fan of Hot Air Gun
 * also check that TIM1 counter changed driven by AC_ZERO interrupt
 * on TIM5 Output channel #4 to read the IRONs, HOt Air Gun and ambient temperatures
 */
extern "C" void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM5) {
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {
			adcStartCurrent();
		} else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
			adcStartTemp();
		}
	}
}

/*
 * IRQ handler of ADC complete request.
 * The currents are checked by ADC3, the result is in the cur_buff[] array in the following order:
 * T12 current, JBC current, FAN current
 *
 * The temperatures are checked serially depending on jbc_phase value
 * When jbc_phase is true, the JBC temperature and Hot Air Gun temperature are checked by ADC2
 *   The result is in jbc_buff[] array: JBC temperature, Gun temperature
 * When jbc_phase is false, , the T12 temperature and ambient temperature are checked by ADC1
 *   The result is in t12_buff[] array: T12 temperature, Ambient temperature, VREFINT, MCU internal temperature
 */
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	HAL_ADC_Stop_DMA(hadc);
	if (adc_mode == ADC_TEMP) {								// Checking the temperature
		if (jbc_phase) {
			// Check the JBC temperature and calculate the power supplied to the JBC IRON
			jbc_power = core.jbc.power(jbc_buff[0]);
			if (jbc_power > max_iron_pwm) {					// The required power is greater than timer period (see vars.cpp)
					TIM5->CCR2	= max_iron_pwm;				// Use full period PWM
				jbc_power	-= max_iron_pwm;				// And save extra power to the next phase
			} else {
					TIM5->CCR2	= jbc_power;				// The timer period is enough to supply required power
				if (jbc_power > 0) t12_power = 0;			// Do not supply power to the both devices simultaneously
				jbc_power 	= 0;							// No extra power will be supply to the JBC IRON on the next phase
			}
			TIM5->CCR1	= t12_power;						// The T12 iron can be powered
			core.hotgun.updateTemp(jbc_buff[1]);			// The Hot Air Gun temperature
		} else {
			// Check the T12 temperature and calculate the power supplied to the T12 IRON
			t12_power = core.t12.power(t12_buff[0]);
			if (t12_power > max_iron_pwm) {					// The required power is greater than the single timer period
				TIM5->CCR1	= max_iron_pwm;					// Use full period PWM
				t12_power	-= max_iron_pwm;				// And save extra power to the next phase
			} else {
				TIM5->CCR1	= t12_power;					// The timer period is enough to supply required power
				if (t12_power > 0) jbc_power = 0;			// Do not supply power to the both devices simultaneously
				t12_power 	= 0;							// No extra power will be supply to the T12 IRON on the next phase
			}
			TIM5->CCR2	= jbc_power;						// The JBC iron can be powered
			core.updateAmbient(t12_buff[1]);				// The ambient (Hakko T12 handle sensor) temperature
			core.updateIntTemp(t12_buff[2], t12_buff[3]);	// The t12_buff[2] is VREFINT, t12_buff[3] is t_mcu
		}
		jbc_phase = !jbc_phase;
	} else if (adc_mode == ADC_CURRENT) {					// Read the currents
		if (TIM5->CCR1 > 1) {								// The T12 iron has been powered
			core.t12.updateCurrent(cur_buff[0]);
		}
		if (TIM5->CCR2 > 1) {								// The JBC iron has been powered
			core.jbc.updateCurrent(cur_buff[1]);
		}
		if (TIM11->CCR1 > 1) {								// The Hot Air Gun FAN has been powered
			core.hotgun.updateCurrent(cur_buff[2]);
			if (TIM11->CNT > 500) TIM11->CNT = 100;			// Synchronize timers to check the current through Hot Air Gun fan correctly
		}
	}
	adc_mode = ADC_IDLE;
}

// IRQ handler for Gun power timer (TIM1). Checking for AC interrupts
// TIM7 used to play songs
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1) {
		uint32_t tm		= HAL_GetTick();
		ac_sine			= gtim_last_ms + 30 > tm;			// Last TIM3 external interrupt was less than 30 ms before. 50Hz outlet interrupt period is 10 ms
		if (ac_sine && gtim_last_ms > 0) {
			gtim_period.update((tm - gtim_last_ms) * 100);
		}
		gtim_last_ms	= tm;
	} else if (htim->Instance == TIM7) {
		core.buzz.playSongCB();
	}
}

// Gun power DMA circular buffer routine
void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance != TIM1) return;
	uint16_t gun_power	= 0;								// First half of the pwr_buffer has been sent, calculate next buffer values
	if (ac_sine)
		gun_power	= core.hotgun.power();
	if (gun_power) {
		calculateGunPowerData(&gun_pwr[MAX_GUN_POWER], MAX_GUN_POWER, gun_power);
	} else {
		powerOffGun();
	}
}

// Gun power DMA circular buffer routine
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance != TIM1) return;
	uint16_t gun_power	= 0;								// Second half of the pwr_buffer has been sent, calculate next buffer values
	if (ac_sine)
		gun_power	= core.hotgun.power();
	if (gun_power) {
		calculateGunPowerData(gun_pwr, MAX_GUN_POWER, gun_power);
	} else {
		powerOffGun();
	}
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) 				{ }
extern "C" void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc) 	{ }
