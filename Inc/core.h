/*
 * core.h
 *
 *  Created on: 16 sep 2019
 *      Author: Alex
 */

#ifndef CORE_H_
#define CORE_H_

#include <stdbool.h>
#include "main.h"

// Forward function declaration
bool 		isACsine(void);
uint16_t	gtimPeriod(void);

#ifdef __cplusplus
extern "C" {
#endif

void setup(void);
void loop(void);

#ifdef __cplusplus
}
#endif

#endif
