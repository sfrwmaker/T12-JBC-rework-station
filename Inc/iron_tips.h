/*
 * iron_tips.h
 *
 *  Created on: 15 aug. 2019.
 *      Author: Alex
 */

#ifndef IRON_TIPS_H_
#define IRON_TIPS_H_
#include "main.h"

// The length of the tip name
#define		tip_name_sz		(5)

class TIPS {
	public:
		TIPS()													{ }
		uint8_t			total(void);							// Whole TIPS number: T12 + JBC + Hot Air Gun pseudo tip
		uint8_t			t12Tips(void);
		uint8_t			jbcTips(void);
		uint8_t			jbcFirstIndex(void);
		bool			isExtraTip(uint8_t index);				// Tweezers or Hakko 936
		const char* 	name(uint8_t index);
		int 			index(const char *name);
};


#endif
