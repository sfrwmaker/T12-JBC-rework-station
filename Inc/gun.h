/*
 * gun.h
 *
 * Released Jan 7 2023
 *
 * 2023 FEB 18, v.1.01
 *    Added POWER_HEATING mode to prevent high temperature while heating-up
 *    Added stable constant
 *    Changed HOTGUN::isON() method
 * 2024 JUN 28, v.1.04
 * 	  Changed the HOTGUN::sw_avg_len from 10 to 15
 * 2024 OCT 01, v.1.07
 * 	  Added fast_cooling parameter to HOTGUN
 * 	  Added HOTGUN::setFastGunCooling()
 * 	  Changed the HOTGUN::sw_avg_len from 15 to 13
 * 2024 OCT 13
 *    Added POWER_STBY into PowerMode enum
 * 	  Added low_temp to HOTGUN
 * 2024 NOV 05, v.1.08
 * 		Added HOTGUN::minFanSpeed()
 * 		Removed HOTGUN::min_working_fan constant
 * 		Added HOTFUN::fanStepPcnt()
 * 2024 NOV 14
 * 		Added HOTGUN::relay_activated boolean flag
 * 2025 SEP 15, v.1.10
 *  	Tailored the Hot Air Gun methods and parameters to the new power algorithm:
 *  	Added temp_len constant that defines the exponential average length of the Hot Air Gun temperature
 *  	Removed HOTGUN::stable constant (PID workaround)
 *  	Added HOTGUN::min_cool_temp and HOTGUN::min_cool_tm variables to manage cooling process
 *  	Added HOTGUN::fan_curr_avg_len constant to setup the average length of fan current SWITCH
 *  	Changed HOTGUN::sw_avg_len from 13 to 3 to increase speed of switching the Hot Air Gun on/off
 *  	Added HOTGUN::appplied_power variable to save the applied power value. Used in appliedPower() method
 */

#ifndef GUN_H_
#define GUN_H_

#include "stat.h"
#include "tools.h"
#include "unit.h"

#define FAN_TIM		htim11
extern TIM_HandleTypeDef FAN_TIM;

class HOTGUN : public UNIT {
    public:
		typedef enum { POWER_OFF, POWER_HEATING, POWER_ON, POWER_FIXED, POWER_STBY, POWER_COOLING, POWER_PID_TUNE } PowerMode;
        HOTGUN(void) 		{ }
        void        		init(void);
		virtual bool		isOn(void)						{ return (mode == POWER_ON || mode == POWER_HEATING || mode == POWER_FIXED); }
		virtual uint16_t	presetTemp(void)				{ return temp_set; 								}
		uint16_t			presetFan(void)					{ return fan_speed;								}
		virtual uint16_t 	averageTemp(void)				{ return avg_sync_temp; 						}
        virtual uint16_t	getMaxFixedPower(void)			{ return max_fix_power; 						}
        virtual bool		isCold(void)					{ return mode == POWER_OFF;						}
        bool				isFanWorking(void)				{ return (fanSpeed() >= min_fan_speed);			}
        uint16_t			minFanSpeed(void)				{ return min_fan_speed;							}
        uint16_t			maxFanSpeed(void)				{ return max_fan_speed;							}
        uint8_t				fanStepPcnt(void)				{ return (max_fan_speed + 50) / 100;			}
        virtual uint16_t	pwrDispersion(void)				{ return d_power.read(); 						}
        virtual uint16_t 	tmpDispersion(void)				{ return d_temp.read(); 						}
		virtual void		setTemp(uint16_t temp)			{ temp_set	= constrain(temp, 0, int_temp_max);	}
		void				setFan(uint16_t fan)			{ fan_speed = constrain(fan, min_fan_speed, max_fan_speed);	}
		void				setFastGunCooling(bool on)		{ fast_cooling = on;							}
		void				fanFixed(uint16_t fan);
		void				fanControl(bool on);
		void				updateTemp(uint16_t value);
        virtual void		switchPower(bool On);
        virtual void		autoTunePID(uint16_t base_pwr, uint16_t delta_power, uint16_t base_temp, uint16_t temp);
        virtual uint16_t	avgPower(void)					{ return avgPowerPcnt();						}
        virtual uint8_t		avgPowerPcnt(void);
		uint16_t			appliedPower(void);
		uint16_t			fanSpeed(void);					// Fan supplied to Fan, PWM duty
        virtual void        fixPower(uint16_t Power);		// Set the specified power to the the hot gun
		uint8_t				presetFanPcnt(void);
		uint16_t			power(void);					// Required Hot Air Gun power to keep the preset temperature
		void				safetyRelay(bool activate);
		void        		lowPowerMode(uint16_t t);		// Activate low power mode (preset temp.) To disable, use switchPower(true)
    private:
		void		shutdown(void);
		void		regMinCoolingTemp(void)					{ min_cool_temp	= avg_sync_temp; min_cool_tm = HAL_GetTick(); }
		PowerMode	mode				= POWER_OFF;
		uint8_t    	fix_power			= 0;				// Fixed power value of the Hot Air Gun (or zero if off)
		bool		chill				= false;			// Chill the Hot Air gun if it is over heating
		bool		reach_cold_temp		= true;				// Flag indicating the Hot Air Gun has reached the 'temp_gun_cold' temperature
		bool		fast_cooling		= false;			// Flag indicating maximum fan speed when cooling
		uint16_t	temp_set			= 0;				// The preset temperature of the hot air gun (internal units)
		uint16_t	fan_speed			= 0;				// Preset fan speed
		uint16_t	low_temp			= 0;				// The temperature in standby mode (if not zero)
		uint32_t	fan_off_time		= 0;				// Time when the fan should be powered off in cooling mode (ms)
		uint16_t	min_cool_temp		= 0;				// The minimum registered temperature in cooling mode
		uint32_t	min_cool_tm			= 0;				// The time when the minimum registered temperature in cooling mode reached
		EMP_AVERAGE	h_power;								// Exponential average of applied power
		EMP_AVERAGE	c_temp;									// Exponential average of Hot Air Gun current temperature. Updated in HAL_ADC_ConvCpltCallback() see core.cpp
		EMP_AVERAGE	h_temp;									// Exponential average of Hot Air Gun temperature. Updated in HAL_ADC_ConvCpltCallback() see core.cpp
		EMP_AVERAGE	d_power;								// Exponential average of power dispersion
		EMP_AVERAGE d_temp;									// Exponential temperature math dispersion
		EMP_AVERAGE	zero_temp;								// Exponential average of minimum (zero) temperature
		volatile    uint16_t	avg_sync_temp	= 0;		// Average temperature synchronized with TIM1 (used to calculate required power, see power() method)
		volatile 	uint8_t		relay_ready_cnt	= 0;		// The relay ready counter, see HOTHUN::power()
		volatile	uint16_t	applied_power	= 0;		// Calculated value of power to be applied (see power())
		bool		relay_activated				= false;	// The relay activated flag
        const       uint8_t     max_fix_power 	= 70;
		const		uint8_t		max_power		= 120;
		const		uint16_t	min_fan_speed	= 700;
		const		uint16_t	max_fan_speed	= 1999;
		const		uint16_t	max_cool_fan	= 1600;
        const       uint16_t    temp_gun_off   	= 125;		// (50) The temperature of the cold Hot Air Gun
        const		uint32_t	fan_off_timeout	= 6*60*1000;// The timeout to turn the fan off in cooling mode
        const		uint32_t	fan_extra_time	= 60000;	// Extra time to wait after the Hot Air Gun reaches the 'temp_gun_cold' temperature
		const		uint16_t	fan_off_value	= 500;
		const 		uint16_t	fan_on_value	= 1000;
		const 		uint8_t		sw_off_value	= 30;
		const 		uint8_t		sw_on_value		= 60;
		const 		uint8_t		sw_avg_len		= 13;
		const		uint8_t		temp_len		= 6;		// The current temperature exponential average length
		const 		uint8_t		hot_gun_len		= 10;		// The history data length of Hot Air Gun average values
        const		uint32_t	relay_activate	= 1;		// The relay activation delay (loops of TIM1, 1 time per second)
		const		uint32_t	cooling_to		= 60000;	// If min_cool_temp has not been changed during this timeout, assume the minimum temperature is reached
};

#endif
