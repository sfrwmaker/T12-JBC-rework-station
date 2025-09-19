/*
 * buzzer.h
 *
 * 2022 DEC 02
 * 		Creation date
 * 2025 SEP 17, v.1.10
 * 		Modified to non-blocking mode. The TIM7 timer is used to play tones for the specified period of time
 *
 */

#ifndef BUZZER_H_
#define BUZZER_H_

#ifndef __BUZZ_H
#define __BUZZ_H
#include "main.h"

#define BUZZER_TIM			htim10
extern TIM_HandleTypeDef 	BUZZER_TIM;
#define PERIOD_TIM			htim7
extern  TIM_HandleTypeDef 	PERIOD_TIM;

class BUZZER {
	public:
		BUZZER(void)					{ }
		void		activate(bool e)	{ enabled = e; BUZZER_TIM.Instance->CCR1 = 0; }
		void		lowBeep(void);
		void		shortBeep(void);
		void		doubleBeep(void);
		void		failedBeep(void);
		void		playSongCB(void);
	private:
		void		playTone(uint16_t period_mks, uint16_t duration_ms);
		void		playSong(const uint16_t *song);
		bool		enabled = true;
		const uint16_t *next_note		= 0;
		const uint16_t short_beep[4] 	= { 284,  1600, 0, 0 };
		const uint16_t double_beep[8] 	= { 284,  1600, 0, 1000, 284,  1600, 0, 0 };
		const uint16_t low_beep[4]		= { 2840, 1600, 0, 0 };
		const uint16_t failed_beep[12]	= { 284,  1600, 0, 500 , 2840, 600,  0, 500, 1420, 1600, 0, 0 };
};

#endif

#endif
