/*
 * buzzer.h
 *
 * Created 2022 Dec 2
 *
 */

#ifndef BUZZER_H_
#define BUZZER_H_

#ifndef __BUZZ_H
#define __BUZZ_H
#include "main.h"

#define BUZZER_TIM			htim10
extern TIM_HandleTypeDef 	BUZZER_TIM;

class BUZZER {
	public:
		BUZZER(void);
		void		activate(bool e)	{ enabled = e; }
		void		lowBeep(void);
		void		shortBeep(void);
		void		doubleBeep(void);
		void		failedBeep(void);
	private:
		void		playTone(uint16_t period_mks, uint16_t duration_ms);
		void		startPlaying(uint16_t music[][2], uint8_t size);
		bool		enabled = true;
};

#endif

#endif		/* BUZZER_H_ */
