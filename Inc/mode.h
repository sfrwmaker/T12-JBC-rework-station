/*
 * mode.h
 *
 * Dec 08 2022
 *    Added clean() method to the MODE class
 * Dec 20 2022
 *    Added restore_power_ms to the MCALIB_MANUAL class to stop powering the iron when you try to decrease the preset temperature
 * Sep 03 2023
 * 	  Modified the FDEBUG class
 * 	  Move load language data procedure to the menu.h
 */

#include <vector>
#include <string>
#include "hw.h"

#ifndef _MODE_H_
#define _MODE_H_

class MODE {
	public:
		MODE(HW *pCore)										{ this->pCore = pCore; 	}
		void			setup(MODE* return_mode, MODE* short_mode, MODE* long_mode);
		virtual void	init(void)							{ }
		virtual MODE*	loop(void)							{ return 0; }
		virtual void	clean(void)							{ }
		virtual			~MODE(void)							{ }
		void			useDevice(tDevice dev)				{ dev_type 	= dev; 	}
		MODE*			returnToMain(void);
		UNIT*			unit(void);
	protected:
		void 			resetTimeout(void);
		void 			setTimeout(uint16_t t);
		tDevice			dev_type		= d_t12;			// Some modes can work with iron(s) or gun (tune, calibrate, pid_tune)
		HW*				pCore			= 0;
		uint16_t		timeout_secs	= 0;				// Timeout to return to main mode, seconds
		uint32_t		time_to_return 	= 0;				// Time in ms when to return to the main mode
		uint32_t		update_screen	= 0;				// Time in ms when the screen should be updated
		MODE*			mode_return		= 0;				// Previous working mode
		MODE*			mode_spress		= 0;				// When encoder button short pressed
		MODE*			mode_lpress		= 0;				// When encoder button long  pressed

};

//---------------------- The tip selection mode ----------------------------------
#define MSLCT_LEN		(10)
class MSLCT : public MODE {
	public:
		MSLCT(HW *pCore) : MODE(pCore)						{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		void			changeTip(uint8_t index);
		TIP_ITEM		tip_list[MSLCT_LEN];
		uint32_t 		tip_begin_select	= 0;			// The time in ms when we started to select new tip
		uint32_t		tip_disconnected	= 0;			// When the tip has been disconnected
		bool			manual_change		= false;
};

//---------------------- The Activate tip mode: select tips to use ---------------
class MTACT : public MODE {
	public:
		MTACT(HW *pCore) : MODE(pCore)						{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
};

//---------------------- The calibrate tip mode: automatic calibration -----------
#define MCALIB_POINTS	8
class MCALIB : public MODE {
	public:
		MCALIB(HW *pCore) : MODE(pCore)						{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		bool 		calibrationOLS(uint16_t* tip, uint16_t min_temp, uint16_t max_temp);
		uint8_t		closestIndex(uint16_t temp);
		void 		updateReference(uint8_t indx);
		void 		buildFinishCalibration(void);
		uint8_t		ref_temp_index	= 0;					// Which temperature reference to change: [0-MCALIB_POINTS]
		uint16_t	calib_temp[2][MCALIB_POINTS];			// The calibration data: real temp. [0] and temp. in internal units [1]
		uint16_t	tip_temp_max	= 0;					// the maximum possible tip temperature in the internal units
		bool		tuning			= false;
		uint32_t	ready_to		= 0;					// The time when the Iron should be ready to enter real temperature (ms)
		uint32_t	phase_change	= 0;					// The heating phase change time (ms)
		uint32_t	check_device_tm	= 0;					// Time in ms when to check the device connectivity
		enum {MC_OFF = 0, MC_GET_READY, MC_HEATING, MC_COOLING, MC_HEATING_AGAIN, MC_READY}
					phase = MC_OFF;							// The Iron getting the Reference temperature phase
		const uint16_t start_int_temp 	 = 600;				// Minimal temperature in internal units, about 100 degrees Celsius
		const uint32_t phase_change_time = 3000;
		const uint32_t check_device_to	 = 5000;
};

//---------------------- The calibrate tip mode: manual calibration --------------
class MCALIB_MANUAL : public MODE {
	public:
		MCALIB_MANUAL(HW *pCore) : MODE(pCore)				{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		void 		buildCalibration(int8_t ambient, uint16_t tip[], uint8_t ref_point);
		void		restorePIDconfig(CFG *pCFG, UNIT* pUnit);
		uint8_t		ref_temp_index	= 1;					// Which temperature reference to change: [0-3]
		uint16_t	calib_temp[4];							// The calibration temp. in internal units in reference points
		bool		calib_flag[4];							// Flag indicating the reference temperature has been calibrated
		bool		ready			= 0;					// Whether the temperature has been established
		bool		tuning			= 0;					// Whether the reference temperature is modifying (else we select new reference point)
		uint32_t	temp_setready_ms= 0;					// The time in ms when we should check the temperature is ready
		uint32_t	restore_power_ms= 0;
		uint16_t	fan_speed		= 1500;					// The Hot Air Gun fan speed during calibration
};

//---------------------- The PID coefficients tune mode --------------------------
class MTPID : public MODE {
	public:
		MTPID(HW *pCore) : MODE(pCore)						{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
		virtual void	clean(void);
	private:
		bool		confirm(void);							// Confirmation dialog
		uint32_t	data_update	= 0;						// When read the data from the sensors (ms)
		uint8_t		data_index	= 0;						// Active coefficient
		bool        modify		= 0;						// Whether is modifying value of coefficient
		bool		on			= 0;						// Whether the IRON or Hot Air Gun is turned on
		bool		reset_dspl	= false;					// The display should be reset flag
		bool		allocated	= false;					// Flag indicating the data allocated successfully
		uint16_t 	old_index 	= 3;
};

//---------------------- The PID coefficients automatic tune mode ----------------
class MAUTOPID : public MODE {
	public:
	typedef enum { TUNE_OFF, TUNE_HEATING, TUNE_BASE, TUNE_PLUS_POWER, TUNE_MINUS_POWER, TUNE_RELAY } TuneMode;
	typedef enum { FIX_PWR_NONE = 0, FIX_PWR_DECREASED, FIX_PWR_INCREASED, FIX_PWR_DONE } FixPWR;
		MAUTOPID(HW *pCore) : MODE(pCore)					{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
		virtual void	clean(void);
		bool			updatePID(UNIT *pUnit);
	private:
		uint16_t	td_limit	= 6;						// Temperature dispersion limits
		uint32_t	pwr_ch_to	= 5000;						// Power change timeout
		FixPWR		pwr_change	= FIX_PWR_NONE;				// How the fixed power was adjusted
		uint32_t	data_update	= 0;						// When read the data from the sensors (ms)
		uint32_t	next_mode	= 0;						// When next mode can be activated (ms)
		uint32_t	phase_to	= 0;						// Phase timeout
		uint16_t	base_pwr	= 0;						// The applied power when preset temperature reached
		uint16_t	base_temp	= 0;						// The temperature when base power applied
		uint16_t	old_temp	= 0;						// The previous temperature allowing adjust the supplied power
		uint16_t	delta_temp  = 0;						// The temperature limit (base_temp - delta_temp <= t <= base_temp + delta_temp)
		uint16_t	delta_power = 0;						// Extra power
		uint16_t	data_period = 250;						// Graph data update period (ms)
		TuneMode	mode		= TUNE_OFF;					// The preset temperature reached
		uint16_t	tune_loops	= 0;						// The number of oscillation loops elapsed in relay mode
		const uint16_t	max_delta_temp 		= 6;			// Maximum possible temperature difference between base_temp and upper temp.
		const uint32_t	msg_to	= 2000;						// Show message timeout (ms)
		const uint16_t  max_pwr	= 400;						// Maximum power in the heating phase
};

//---------------------- The Fail mode: display error message --------------------
class MFAIL : public MODE {
	public:
		MFAIL(HW *pCore) : MODE(pCore)						{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
		void			setMessage(const t_msg_id msg, const char *parameter = 0);
	private:
		char			parameter[20] = {0};
		t_msg_id		message	= MSG_LAST;
};

//---------------------- The About dialog mode. Show about message ---------------
class MABOUT : public MODE {
	public:
		MABOUT(HW *pCore, MODE* flash_debug) : MODE(pCore)						{ this->flash_debug = flash_debug; }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		MODE*			flash_debug;						// Flash debug mode pointer
};


//---------------------- The Debug mode: display internal parameters ------------
class MDEBUG : public MODE {
	public:
		MDEBUG(HW *pCore) : MODE(pCore)						{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		uint16_t		old_ip 			= 0;				// Old IRON encoder value
		uint16_t		old_fp			= 0;				// Old GUN encoder value
		bool			gun_is_on 		= false;			// Flag indicating the gun is powered on
		bool			jbc_selected	= false;			// Flag indicating the JBC iron is selected
		bool			iron_on			= true;				// Flag indicating the iron (JBC or T12) nis powered on
		const uint16_t	max_iron_power 	= 800;
		const uint16_t	min_fan_speed	= 800;
		const uint16_t	max_fan_speed 	= 1999;
		const uint8_t	gun_power		= 3;
};

//---------------------- The Flash debug mode: display flash status & content ---
class FDEBUG : public MODE {
	public:
		FDEBUG(HW *pCore, MODE* m_flash) : MODE(pCore)		{ this->manage_flash = m_flash; }
		virtual void	init(void);
		virtual MODE*	loop(void);
		void			readDirectory();
	private:
		MODE*			manage_flash;
		FLASH_STATUS	status			=	FLASH_OK;
		uint16_t		old_ge 			= 0;				// Old Gun encoder value
		std::string		c_dir			= "/";				// Current directory path
		std::vector<std::string>	dir_list;				// Directory list
		int8_t			delete_index	= -1;				// File to be deleted index
		bool			confirm_format	= false;			// Confirmation dialog activates
		t_msg_id		msg				= MSG_LAST;			// Error message index
		const uint32_t	update_timeout	= 60000;			// Default update display timeout, ms

};

//---------------------- The Flash format mode: Confirm and format the flash ----
class FFORMAT : public MODE {
	public:
		FFORMAT(HW *pCore) : MODE(pCore)					{ }
		virtual void	init(void);
		virtual MODE*	loop(void);
	private:
		uint8_t p = 2;										// Make sure the message sill be displayed for the first time in the loop
};

#endif
