/*
 * unit.cpp
 *
 *  Created on: 13 June 2022
 *      Author: Alex
 *
 *  2025 SEP 19, v1.10
 *  	changed UNIT::init() method to initialize the current instance to 'iron is connected' value
 *  2025 SEP 23, v.1.11
 *  	changed UNIT::init() method to initialize the current instance to 'iron is NOT connected' value
 */

#include "unit.h"

void UNIT::init(uint8_t c_len, uint16_t c_min, uint16_t c_max, uint8_t s_len, uint16_t s_min, uint16_t s_max) {
	current.init(c_len,	c_min, c_max);						// By default the IRON is NOT connected at startup
	sw.init(s_len,		s_min, s_max);
	change.init(s_len,	s_min, s_max);
}

bool UNIT::isReedSwitch(bool reed) {
	if (reed)
		return sw.status();									// TRUE if switch is open (IRON in use)
	return sw.changed();									// TRUE if tilt status has been changed
}
