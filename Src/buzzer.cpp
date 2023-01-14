/*
 * buzzer.cpp
 *
 * 2022 Dec 02
 *    The BUZZER::PlayTone() changed to use the timer instance
 *    Cannot fix the timer channel name, so it is important to edit BUZZER::playTone to setup correct PWM channel name
 */

#include "buzzer.h"
#include "main.h"

BUZZER::BUZZER(void) {
	BUZZER_TIM.Instance->CCR1 = 0;
}


void BUZZER::playTone(uint16_t period_mks, uint16_t duration_ms) {
	BUZZER_TIM.Instance->ARR 	= period_mks-1;
	BUZZER_TIM.Instance->CCR1 	= period_mks >> 1;
	HAL_Delay(duration_ms);
	BUZZER_TIM.Instance->CCR1 	= 0;
}

void BUZZER::shortBeep(void) {
	if (!enabled) return;
	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
	playTone(284, 160);
	HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
}

void BUZZER::doubleBeep(void) {
	if (!enabled) return;
	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
	playTone(284, 160);
	HAL_Delay(100);
	playTone(284, 160);
	HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
}

void BUZZER::lowBeep(void) {
	if (!enabled) return;
	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
	playTone(2840, 160);
	HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
}

void BUZZER::failedBeep(void) {
	if (!enabled) return;
	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
	playTone(284, 160);
	HAL_Delay(50);
	playTone(2840, 60);
	HAL_Delay(50);
	playTone(1420, 160);
	HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
}
