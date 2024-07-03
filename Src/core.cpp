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
 *  A12	- TIM1_ETR, AC zero signal read - clock source
 *  	  TIM1_CH3, Output compare (97) to calculate power to the Hot Air Gun
 *  A11	- TIM1_CH4, Hot Air Gun power [0-99]
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
 *  Mar 30 2024
 *     Changed void setup(). When the FLASH failed to read, the fail mode would return to flash_debug mode only
 *     Added active.setFail() call
 */

#include <math.h>
#include "core.h"
#include "hw.h"
#include "mode.h"
#include "work_mode.h"
#include "menu.h"
#include "vars.h"

#define ADC_T12 	(2)										// Activated ADC Ranks Number (hadc1.Init.NbrOfConversion)
#define ADC_JBC 	(2)										// Activated ADC Ranks Number (hadc2.Init.NbrOfConversion)
#define ADC_CUR		(3)										// Activated ADC Ranks Number (hadc3.Init.NbrOfConversion)

extern ADC_HandleTypeDef	hadc1;
extern ADC_HandleTypeDef	hadc2;
extern ADC_HandleTypeDef	hadc3;
extern TIM_HandleTypeDef	htim1;
extern TIM_HandleTypeDef	htim5;
extern TIM_HandleTypeDef 	htim10;
extern TIM_HandleTypeDef	htim11;

volatile uint32_t errors	= 0;
volatile uint16_t tim5[100] = {0}; // 1990
volatile uint8_t  tim5_cnt	= 0;

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
static 	EMP_AVERAGE			gtim_period;					// gun timer period (ms)
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
static	MMENU			main_menu(&core, &iselect, &param_menu, &activate, &t12_menu, &jbc_menu, &calib_manual, &about);
static	MODE*           pMode = &work;

bool 		isACsine(void)		{ return ac_sine; 				}
uint16_t	gtimPeriod(void)	{ return gtim_period.read();	}

// Synchronize TIM5 timer to AC power. The main timer managing T12 & JBC irons
static uint16_t syncAC(uint16_t tim_cnt) {
	uint32_t to = HAL_GetTick() + 300;						// The timeout
	uint16_t nxt_tim1	= TIM1->CNT + 2;
	if (nxt_tim1 > 99) nxt_tim1 -= 99;						// TIM1 is clocked by AC zero crossing signal, period is 99.
	while (HAL_GetTick() < to) {							// Prevent hang
		if (TIM1->CNT == nxt_tim1) {
			TIM5->CNT = tim_cnt;							// Synchronize TIM5 to AC power zero crossing signal
			break;
		}
	}
	// Checking the TIM5 has been synchronized
	to = HAL_GetTick() + 300;
	nxt_tim1 = TIM1->CNT + 2;
	if (nxt_tim1 > 99) nxt_tim1 -= 99;
	while (HAL_GetTick() < to) {
		if (TIM1->CNT == nxt_tim1) {
			return TIM5->CNT;
		}
	}
	return TIM5->ARR+1;										// This value is bigger than TIM5 period, the TIM5 has not been synchronized
}

extern "C" void setup(void) {
	TIM12->CCR1 = 0;										// Do turn-off the display backlight
	// Read temperature values
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 100);
	uint16_t t12_temp = HAL_ADC_GetValue(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 100);
	uint16_t ambient = HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);
	HAL_ADC_Start(&hadc2);
	HAL_ADC_PollForConversion(&hadc2, 100);
	uint16_t jbc_temp = HAL_ADC_GetValue(&hadc2);
	HAL_ADC_PollForConversion(&hadc2, 100);
	uint16_t gun_temp = HAL_ADC_GetValue(&hadc2);
	HAL_ADC_Stop(&hadc2);
	gtim_period.length(10);
	gtim_period.reset(1000);								// Default TIM1 period, ms
	max_iron_pwm	= htim5.Instance->CCR4 - 40;			// Max value should be less than TIM5.CH4 value by 40.

	CFG_STATUS cfg_init = core.init(t12_temp, jbc_temp, gun_temp, ambient);
	core.t12.setCheckPeriod(3);								// Start checking the T12 IRON connectivity
	HAL_TIM_PWM_Start(&htim1, 	TIM_CHANNEL_4);				// PWM signal of Hot Air Gun
	HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_3);				// Calculate power of Hot Air Gun interrupt
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

	syncAC(1500);											// Synchronize TIM5 timer to AC power. Parameter is TIM5 counter value when TIM1 become zero
	uint8_t br = core.cfg.getDsplBrightness();
	core.dspl.BRGT::set(br);
	// Turn-on the display backlight immediately in the debug mode
#ifdef DEBUG_ON
	core.dspl.BRGT::on();
#endif
	HAL_Delay(200);
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
    	++errors;
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
    	++errors;
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
 * on TIM1 Output channel #3 to calculate required power for Hot Air Gun
 * on TIM5 Output channel #3 to read the current through the IRONs and Fan of Hot Air Gun
 * also check that TIM1 counter changed driven by AC_ZERO interrupt
 * on TIM5 Output channel #4 to read the IRONs, HOt Air Gun and ambient temperatures
 */
extern "C" void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {
		uint16_t gun_power	= core.hotgun.power();
		if (gun_power > max_gun_pwm) gun_power = max_gun_pwm;
		TIM1->CCR4	= gun_power;							// Apply Hot Air Gun power
		uint32_t n = HAL_GetTick();
		if (ac_sine && gtim_last_ms > 0) {
			gtim_period.update(n - gtim_last_ms);
		}
		gtim_last_ms = n;
	} else if (htim->Instance == TIM5) {
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
 *   The result is in jbc_buff[] array: JBC temperature, Gun temperature, BC temperature
 * When jbc_phase is false, , the T12 temperature and ambient temperature are checked by ADC1
 *   The result is in t12_buff[] array: T12 temperature, Ambient temperature, T12 temperature
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

		}
		jbc_phase = !jbc_phase;
		tim5[tim5_cnt] = TIM5->CNT;
		if (++tim5_cnt >= 100) {
			tim5_cnt = 0;
		}
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

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) 				{ }
extern "C" void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc) 	{ }
