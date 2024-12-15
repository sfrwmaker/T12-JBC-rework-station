/*
 * dash.h
 *  Author: Alex
 *
 * 2023 MAR 01, v.1.01
 *  Removed showOffTimeout()
 * 2024 OCT 14, v.1.07
 * 		Added DASH::gunStandby()
 */

#ifndef _DASH_H_
#define _DASH_H_
#include "mode.h"

typedef enum { DM_T12_GUN = 0, DM_JBC_T12, DM_JBC_GUN} tDashMode;

typedef enum { IRPH_OFF = 0, IRPH_HEATING, IRPH_READY, IRPH_NORMAL, IRPH_BOOST, IRPH_LOWPWR, IRPH_GOINGOFF,
				IRPH_COOLING, IRPH_COLD } tIronPhase;

// Main working mode dashboard class
class DASH : public MODE {
	public:
		DASH(HW *pCore)	: MODE(pCore)						{ }
		void			init();
		bool			setMode(tDashMode dm);
		void			drawStatus(tIronPhase t12_phase, tIronPhase jbc_phase, int16_t ambient);
		void 			animateFan(void);
		void			ironT12Used(bool active);
		bool			enableJBC(void);
		bool			enableGUN(void);
		bool			enableT12(void);
		bool			disableJBC(void);
		bool			disableGUN(void);
		bool			disableT12(void);
		tUnitPos		devPos(tDevice dev);
		void			ironPhase(tDevice dev, tIronPhase phase);
		void			presetTemp(tDevice dev, uint16_t temp);
		void			fanSpeed(bool modify);
		void			gunStandby(void);
	protected:
		void 			initEncoders(tDevice u_dev, tDevice l_dev, int16_t u_value, uint16_t l_value);
		void 			changeIronShort(void);
		void  			changeIronLong(void);
		bool 			t12Encoder(uint16_t new_value);
		bool			initDevices(bool init_upper, bool init_lower);
		tDevice			u_dev			= d_t12;			// Default is DM_T12_GUN mode
		tDevice			l_dev			= d_gun;
		tDevice			h_dev			= d_jbc;
		bool			is_extra_tip	= false;			// Is the T12 tip index is the extra tip: Hakko 936 or Tweezers
		bool			no_ambient		= false;			// Does the ambient sensor in the HAkko T12 iron handle is connected
		bool			not_jbc			= false;			// The JBC iron is not connected flag: no current through the iron
		bool			not_t12			= false;			// The T12 iron handle is not connected flag: not ambient sensor, no current through the iron and no extra tip
		uint32_t		fan_animate		= 0;				// Time when draw new fan animation
		bool			fan_blowing		= false;			// Used to draw grey or animated and colored fan
		tIronPhase		t12_phase		= IRPH_OFF;			// Current T12 IRON phase
		tIronPhase		jbc_phase		= IRPH_OFF;			// Current JBC IRON phase
};



#endif
