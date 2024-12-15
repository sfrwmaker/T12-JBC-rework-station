/*
 * config.h
 *
 *  Created on: 12 july 2021.
 *      Author: Alex
 *
 *  2022 DEC 23
 *  	Removed CFG::lowTempInternal() because sometimes the lowTemp can be greater than preset Temp
 *  2023 MAR 01, v.1.01
 *   	Added no_lower_limit parameter to the CFG::humanToTemp() to correctly display the temperature in the low power mode
 *  2024 OCT 01, v.1.07
 *  	Added CFG::CORE::isFastGunCooling()
 *  	Added new parameter fast_cooling to CFG_CORE::setup()
 *  2024 OCT 10
 *		Added CFG::CORE::isIPS()
 *		Added new parameter ips_display to CFG_CORE::setup()
 *  2024 OCT 13
 *  	Moved CFG_CORE::getOffTimeout() and CFG_CORE::getOffTimeout() bodies into config.cpp, to implement behavior for Hot Air Gun
 *  	Added CFG_CORE::setupGUN() method
 *  2024 NOV 05, v.1.08
 *  	Moved TIP_CFG::tempMaxC() and tempMinC() to CFG_CORE class to implement safe iron mode
 *  	Added CFG_CORE::isSafeIronMode()
 *  	Added safe_iron_mode parameter onto CFG_CORE::setup()
 *  2024 NOV 07
 *  	Renamed CFG_CORE::tempMinC() -> CFG_CORE::tempMin() and added extra argument
 *  	Renamed CFG_CORE::tempMaxC() -> CFG_CORE::tempMax() and added extra argument
 *  2024 NOV 08
 *  	Added CFG_CORE::tempMax(tDevice dev, bool celsius, bool safe_iron_mode) to use in the parameters menu
 *  2024 NOV 09
 *  	Added TIP_CFG::defaultCalibration(TIP *tip) to use in the CFG::toggleTipActivation()
 *  	Added constant TIP_CFG::min_temp_diff
 *  	Turn the CFG::saveTipCalibtarion() from void to bool
 *  	Added TIP_CFG::calib_default[4] array to use in TIP_CFG::defaultCalibration()
 *  2024 NOV 16
 *  	Added extra parameter to TIP_CFG::applyTipCalibtarion() to correctly setup tip mask
 *  2024 NOV 28
 *  	made public TIP_CFG::isValidTipConfig();
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <string.h>
#include <string>
#include "main.h"
#include "pid.h"
#include "flash.h"
#include "cfgtypes.h"
#include "iron_tips.h"
#include "buzzer.h"

typedef enum {CFG_OK = 0, CFG_NO_TIP, CFG_READ_ERROR, CFG_NO_FILESYSTEM} CFG_STATUS;

/*
 * The actual configuration record is loaded from the EEPROM chunk into a_cfg variable
 * The spare copy of the  configuration record is preserved into s_cfg variable
 * When update request arrives, configuration record writes into EEPROM if spare copy is different from actual copy
 */
class CFG_CORE: public TIPS {
	public:
		CFG_CORE(void)									{ }
		bool		isCelsius(void) 					{ return a_cfg.bit_mask & CFG_CELSIUS;		}
		bool		isBuzzerEnabled(void)				{ return a_cfg.bit_mask & CFG_BUZZER; 		}
		bool		isReedType(void)					{ return a_cfg.bit_mask & CFG_SWITCH;		}
		bool		isBigTempStep(void)					{ return a_cfg.bit_mask & CFG_BIG_STEP;		}
		bool		isAutoStart(void)					{ return a_cfg.bit_mask & CFG_AU_START;		}
		bool		isUpperEncClockWise(void)			{ return a_cfg.bit_mask & CFG_U_CLOCKWISE;	}
		bool		isLowerEncClockWise(void)			{ return a_cfg.bit_mask & CFG_L_CLOCKWISE;	}
		bool		isFastGunCooling(void)				{ return a_cfg.bit_mask & CFG_FAST_COOLING;	}
		bool		isIPS(void)							{ return a_cfg.bit_mask & CFG_DSPL_TYPE;	}
		bool		isSafeIronMode(void)				{ return a_cfg.bit_mask & CFG_SAFE_MODE;	}
		uint16_t	gunFanPreset(void)					{ return a_cfg.gun_fan_speed;				}
		uint8_t		getLowTO(void)						{ return a_cfg.t12_low_to; 					}	// 5-seconds intervals
		uint8_t		getDsplBrightness(void)				{ return a_cfg.dspl_bright;					}	// 1-100%
		uint8_t		getDsplRotation(void)				{ return a_cfg.dspl_rotation;				}
		void		setDsplRotation(uint8_t rotation)	{ a_cfg.dspl_rotation = rotation;			}
		void		setLanguage(const char *lang)		{ strncpy(a_cfg.language, lang, LANG_LENGTH);}
		bool		isExtraTip(void)					{ return TIPS::isExtraTip(a_cfg.t12_tip);	}
		uint8_t		getOffTimeout(tDevice dev);
		uint16_t	getLowTemp(tDevice dev);
		uint16_t	tempPresetHuman(tDevice dev);		// Human readable units
		const char	*getLanguage(void);					// Returns current language name
		void		setup(bool buzzer, bool celsius, bool big_temp_step, bool i_enc, bool g_enc, bool ips_display, bool safe_iron_mode, uint8_t bright);
		void		setupT12(bool reed, bool auto_start, uint8_t off_timeout, uint16_t low_temp, uint8_t low_to, uint8_t delta_temp, uint16_t duration);
		void		setupJBC(uint8_t off_timeout, uint16_t stby_temp);
		void		setupGUN(bool fast_gun_chill, uint8_t stby_timeout, uint16_t stby_temp);
		void 		savePresetTempHuman(uint16_t temp_set, tDevice dev_type);
		void		saveGunPreset(uint16_t temp, uint16_t fan = 0);
		uint8_t		boostTemp(void);
		uint16_t	boostDuration(void);
		void		saveBoost(uint8_t temp, uint16_t duration);
		void		restoreConfig(void);
		PIDparam	pidParams(tDevice dev);
		PIDparam 	pidParamsSmooth(tDevice dev);
		uint16_t	tempMin(tDevice dev, bool force_celsius = false);
		uint16_t	tempMax(tDevice dev, bool force_celsius = false);
		uint16_t 	tempMax(tDevice dev, bool celsius, bool safe_iron_mode);
	protected:
		void		setDefaults(void);
		void		setPIDdefaults(void);
		void		syncConfig(void);
		bool		areConfigsIdentical(void);
		PID_PARAMS	pid;								// PID parameters of all devices
		RECORD		a_cfg;								// active configuration
	private:
		RECORD		s_cfg;								// spare configuration, used when save the configuration to the EEPROM
};

typedef struct s_TIP_RECORD	TIP_RECORD;
struct s_TIP_RECORD {
	uint16_t	calibration[4];
	uint8_t		mask;
	int8_t		ambient;
};

class TIP_CFG {
	public:
		TIP_CFG(void)									{ }
		bool 		isTipCalibrated(tDevice dev);
		void		load(const TIP& tip, tDevice dev = d_t12);
		void		dump(TIP* tip, tDevice dev = d_t12);
		int8_t		ambientTemp(tDevice dev);
		uint16_t	calibration(uint8_t index, tDevice dev);
		uint16_t	referenceTemp(uint8_t index, tDevice dev);
		uint16_t	tempCelsius(uint16_t temp, int16_t ambient, tDevice dev);
		void		getTipCalibtarion(uint16_t temp[4], tDevice dev);
		void		applyTipCalibtarion(uint16_t temp[4], int8_t ambient, tDevice dev, bool calibrated);
		void		resetTipCalibration(tDevice dev);
		bool		isValidTipConfig(TIP *tip);
	protected:
		void 		defaultCalibration(tDevice dev = d_t12);
		void		defaultCalibration(TIP *tip);
	private:
		TIP_RECORD	tip[3];								// Active T12 IRON tip (0), JBC IRON (1) and Hot Air Gun virtual tip (2)
		const uint16_t	temp_ref_iron[4]	= { 200, 260, 330, 400};
		const uint16_t	temp_ref_gun[4]		= { 200, 300, 400, 500};
		const uint16_t	calib_default[4]	= {	1200, 1900, 2500, 2900};
		const uint16_t	min_temp_diff		= 100;		// Minimal temperature difference between nearest reference points
};

class CFG : public W25Q, public CFG_CORE, public TIP_CFG, public BUZZER {
	public:
		CFG(void)				{ }
		CFG_STATUS	init(void);
		bool		reloadTips(void);
		uint16_t	tempToHuman(uint16_t temp, int16_t ambient, tDevice dev);
		uint16_t	humanToTemp(uint16_t temp, int16_t ambient, tDevice dev, bool no_lower_limit = false);
		std::string tipName(tDevice dev);
		void     	changeTip(uint8_t index);
		uint8_t		currentTipIndex(tDevice dev);
		bool		saveTipCalibtarion(uint8_t index, uint16_t temp[4], uint8_t mask, int8_t ambient);
		bool		toggleTipActivation(uint8_t index);
		int			tipList(uint8_t second, TIP_ITEM list[], uint8_t list_len, bool active_only, tDevice dev_type);
		uint8_t		nearActiveTip(uint8_t current_tip);
		void		saveConfig(void);
		void		savePID(PIDparam &pp, tDevice dev = d_t12);
		void 		initConfig(void);
		bool		clearAllTipsCalibration(void);		// Remove tip calibration data
	private:
		void		correctConfig(RECORD *cfg);
		bool 		selectTip(tDevice dev_type, uint8_t index);
		uint8_t		buildTipTable(TIP_TABLE tt[]);
		std::string buildFullTipName(const uint8_t index);
		TIP_TABLE	*tip_table = 0;						// Tip table - chunk number of the tip or 0xFF if does not exist in the EEPROM
};

#endif
