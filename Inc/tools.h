/*
 * tools.h
 *
 *  Created on: 13 Jul 2019
 *      Author: Alex
 *
 * Sep 09 2023, v 1.03
 *  	Added emap()
 */

#ifndef TOOLS_H_
#define TOOLS_H_

#include "main.h"

/*
 * Useful functions
 */

#ifdef __cplusplus
extern "C" {
#endif

int32_t		emap(int32_t value, int32_t v_min, int32_t v_max, int32_t r_min, int32_t r_max);
int32_t 	map(int32_t value, int32_t v_min, int32_t v_max, int32_t r_min, int32_t r_max);
int32_t		constrain(int32_t value, int32_t min, int32_t max);
uint8_t 	gauge(uint8_t percent, uint8_t p_middle, uint8_t g_max);

int16_t 	celsiusToFahrenheit(int16_t cels);
int16_t		fahrenheitToCelsius(int16_t fahr);

#ifdef __cplusplus
}
#endif

#endif
