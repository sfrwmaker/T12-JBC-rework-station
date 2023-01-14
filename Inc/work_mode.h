/*
 * work_mode.h
 *
 *      Author: Alex
 */

#ifndef _WORK_MODE_H_
#define _WORK_MODE_H_

#include "dash.h"

//-------------------- The iron main working mode, keep the temperature ----------
// mode_spress 	- tip selection mode
// mode_lpress	- main menu (when the IRON is OFF)

class MWORK : public DASH {
	public:
		MWORK(HW *pCore) : DASH(pCore), idle_pwr(5)		{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		void			selectUpperUnit(tDevice dev);
		void			manageHardwareSwitches(CFG* pCFG, IRON *pT12, IRON *pJBC, HOTGUN *pHG); // True if exit from the mode
		void 			adjustPresetTemp(void);
		bool			hwTimeout(bool tilt_active);
		void 			swTimeout(uint16_t temp, uint16_t temp_set, uint16_t temp_setH, uint32_t td, uint32_t pd, uint16_t ap);
		void			t12PhaseEnd(void);					// Proceed T12 IRON end of phase
		void			jbcPhaseEnd(void);					// Proceed JBC IRON end of phase
		bool			t12IdleMode(void);					// Check the T12 IRON is used. Return tilt is active
		void			jbcReadyMode(void);					// Check the JBC iron reached the preset temperature
		bool 			manageEncoders(void);
		void 			t12PressShort(void);
		void  			t12PressLong(void);
		bool 			t12Rotate(uint16_t new_value);
		void			jbcPressShort(void);
		bool			jbcRotate(uint16_t new_value);
		bool			isIronCold(tIronPhase phase);
		bool			isIronWorking(tIronPhase phase);
		EMP_AVERAGE  	idle_pwr;							// Exponential average value for idle power
		uint32_t		t12_phase_end	= 0;				// Time when to change phase of T12 IRON (ms)
		uint32_t		jbc_phase_end	= 0;				// Time when to change phase of JBC IRON (ms)
		uint32_t		lowpower_time	= 0;				// Time when switch the T12 IRON to standby power mode
		uint32_t		swoff_time		= 0;				// Time when to switch the IRON off by sotfware method (see swTimeout())
		uint32_t		tilt_time		= 0;				// Time when to change tilt status (ms)s
		uint32_t		check_jbc_tm	= 0;				// When to test the JBC IRON status
		int16_t  		ambient			= 0;				// The ambient temperature
		bool			edit_temp		= true;				// The HOT AIR GUN Encoder mode (Edit Temp/Edit fan)
		uint32_t		return_to_temp	= 0;				// Time when to return to temperature edit mode (ms)
		bool			start			= true;				// Flag indicating the controller just started (used to turn the IRON on)
		const uint16_t	period			= 500;				// Redraw display period (ms)
		const uint16_t	tilt_show_time	= 1500;				// Time the tilt icon to be shown
		const uint32_t	check_irons_to	= 5000;				// When to stop checking the current through the IRONs
		const uint16_t	edit_fan_timeout = 3000;			// The time to edit fan speed (ms)
};

#endif
