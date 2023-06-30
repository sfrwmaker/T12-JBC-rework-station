/*
 * encoder.h
 *
 *  Created on: 03 june 2022.
 *      Author: Alex
 *
 * 2023 FEB 20 v.1.01
 *  Introduced the minimum timeout before next call the read() method to ensure correct readings from the encoder
 *  Added read_ms variable that keeps the time when the read() was called last time
 *  and 'to' constant - minimum timeout before next encoder reading
 *
 */
 
#ifndef ENCODER_H_
#define ENCODER_H_
#include "main.h"
#include "stat.h"

class RENC {
	public:
		RENC(TIM_HandleTypeDef *htim);
		void		start(void)								{ HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL); }
		void		stop(void)								{ HAL_TIM_Encoder_Stop(htim, TIM_CHANNEL_ALL);	}
		void 		addButton(GPIO_TypeDef* ButtonPORT, uint16_t ButtonPIN);
		int16_t 	read(void);
		int16_t		changed(void);
		uint8_t		buttonStatus(void);
		void		setClockWise(bool clockwise)			{ this->clockwise = clockwise;		}
		bool		write(int16_t initPos);
		void    	reset(int16_t initPos, int16_t low, int16_t upp, uint8_t inc, uint8_t fast_inc, bool looped);
		void 		setTimeout(uint16_t timeout_ms)			{ over_press = timeout_ms; }
		void    	setIncrement(uint8_t inc)           	{ increment = fast_increment = inc; }
		uint8_t		getIncrement(void)                 		{ return increment; }
	private:
		EMP_AVERAGE			avg;							// Do average the button readings to maintain the button status
		int16_t				min_pos	= 0;					// Minimum value of rotary encoder
		int16_t				max_pos	= 0;					// Maximum value of roraty encoder
		uint16_t			over_press = 0;					// Maximum time in ms the button can be pressed
		bool              	is_looped = false;          	// Whether the encoder is looped
		uint8_t            	increment = 0;              	// The value to add or substract for each encoder tick
		uint8_t             fast_increment = 0;         	// The value to change encoder when it runs quickly
		uint16_t			value	= 0;					// Timer counter value
		int16_t  			pos		= 0;                	// Encoder current position
		bool				i_b_rel	= false;				// Ignore button release event
		bool				b_on	= false;				// The button current position: true - pressed
		uint32_t 			bpt		= 0;                	// Time in ms when the button was pressed (press time)
		uint32_t			b_check	= 0;					// Time in ms when the button should be checked
		TIM_HandleTypeDef* 	htim	= 0;					// The encoder timer
		GPIO_TypeDef* 		b_port	= 0;					// The PORT of the press button
		uint16_t			b_pin	= 0;					// The PIN number of the button
		int16_t				change	= 0;					// The encoder difference (set in the read() method
		uint32_t			read_ms	= 0;					// Last time the read() function called
		bool				clockwise		= true;			// How exactly the encoder soldered
        const uint8_t     	trigger_on		= 100;			// avg limit to change button status to on
        const uint8_t     	trigger_off 	= 50;			// avg limit to change button status to off
        const uint8_t     	avg_length   	= 4;			// avg length
        const uint8_t		b_check_period	= 20;			// The button check period, ms
		const uint16_t 		long_press		= 1500;			// If the button was pressed more that this timeout, we assume the long button press
		const uint16_t		fast_timeout	= 300;			// Time in ms to change encoder quickly
		const uint16_t		def_over_press	= 2500;			// Default value for button over press timeout (ms)
		const uint8_t		to				= 50;			// Minimum timeout to call read()
};

#endif
