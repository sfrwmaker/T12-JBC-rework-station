/*
 * encoder.cpp
 *
 *  Created on: 03 june 2022.
 *      Author: Alex
 */

#include "encoder.h"

RENC::RENC(TIM_HandleTypeDef *htim) {
	pos 			= 0;
	min_pos 		= -32767; max_pos = 32766; increment = 1;
	is_looped		= false;
	increment		= 1;
	this->htim		= htim;
}

void RENC::addButton(GPIO_TypeDef* ButtonPORT, uint16_t ButtonPIN) {
	bpt 		= 0;
	b_port 		= ButtonPORT;
	b_pin  		= ButtonPIN;
	over_press	= def_over_press;
	avg.length(avg_length);
}

void RENC::reset(int16_t initPos, int16_t low, int16_t upp, uint8_t inc, uint8_t fast_inc, bool looped) {
	min_pos = low; max_pos = upp;
	if (!write(initPos)) initPos = min_pos;
	increment = fast_increment = inc;
	if (fast_inc > increment) fast_increment = fast_inc;
	is_looped = looped;
}

int16_t	RENC::read(void) {
	uint16_t c	= __HAL_TIM_GET_COUNTER(htim);
	uint16_t diff = 0;
	bool turn_clockwise = false;
	if (c > value) {										// Timer value has been increased;
		if (c - value < 0x7FFF) {							// Encoder turned clockwise
			turn_clockwise = true;
			diff = c - value;								// Encoder difference
		} else {											// The timer register overflow
			diff = value + 0xFFFF - c + 1;
		}
	} else {
		if (value - c < 0x7FFF) {							// Encoder turned counter clockwise
			diff = value - c;
		} else {
			diff = 0xFFFF - value + c + 1;
			turn_clockwise = true;
		}
	}
	diff >>= 1;												// The timer value changes by 2
	if (diff == 0)											// Encoder still did not turned
		return pos;
	uint8_t inc = increment;
	if (diff > 1)
		inc = (fast_increment > 0)?fast_increment:diff;
	change = diff;
	if (turn_clockwise == clockwise) {						// The pins of the rotary encoder were twisted on the schematics
		pos -= inc;
		change *= -1;
	} else {
		pos += inc;
	}
	if (pos > max_pos) {
		pos = (is_looped)?min_pos:max_pos;
	} else if (pos < min_pos) {
		pos = (is_looped)?max_pos:min_pos;
	}
	value = c;
	return pos;
}

int16_t RENC::changed(void) {
	int16_t res = change;
	change = 0;
	return res;
}

/*
 * The Encoder button current status
 * 0	- not pressed
 * 1	- short press
 * 2	- long press
 */
uint8_t	RENC::buttonStatus(void) {
	if (HAL_GetTick() >= b_check) {							// It is time to check the button status
		b_check = HAL_GetTick() + b_check_period;
		uint8_t s = 0;
		if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(b_port, b_pin))	// if port state is low, the button pressed
			s = trigger_on << 1;
		if (b_on) {
			if (avg.average(s) < trigger_off)
				b_on = false;
		} else {
			if (avg.average(s) > trigger_on)
				b_on = true;
		}

		if (b_on) {                                        	// Button status is 'pressed'
			uint32_t n = HAL_GetTick() - bpt;
			if ((bpt == 0) || (n > over_press)) {
				bpt = HAL_GetTick();
			} else if (n > long_press) {                    // Long press
				if (i_b_rel) {
					return 0;
				} else{
					i_b_rel = true;
					return 2;
				}
			}
		} else {                                            // Button status is 'not pressed'
			if (bpt == 0 || i_b_rel) {
				bpt = 0;
				i_b_rel = false;
				return 0;
			}
			uint32_t e = HAL_GetTick() - bpt;
			bpt = 0;										// Ready for next press
			if (e < over_press) {							// Long press already managed
				return 1;
			}
		}
	}
    return 0;
}

bool RENC::write(int16_t initPos)	{
	if ((initPos >= min_pos) && (initPos <= max_pos)) {
		pos		= initPos;
		return true;
	}
	return false;
}
