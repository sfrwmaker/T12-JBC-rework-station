/*
 * stat.cpp
 *
 * 2024 NOV 16, v1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 * 2025 NOV 01, v1.04
 * 		Renamed the EMP_AVERAGE class to EXPA
 * 		Created class for OLS approximation
 */

#include <math.h>
#include "stat.h"
#include "tools.h"

int32_t EXPA::average(int32_t value) {
	uint8_t round_v = emp_k >> 1;
	update(value);
	return (emp_data + round_v) / emp_k;
}

void EXPA::update(int32_t value) {
	uint8_t round_v = emp_k >> 1;
	emp_data += value - (emp_data + round_v) / emp_k;
}

int32_t EXPA::read(void) {
	uint8_t round_v = emp_k >> 1;
	return (emp_data + round_v) / emp_k;
}

int32_t	HIST::read(void) {
	int32_t sum = 0;
	if (len == 0) return 0;
	if (len == 1) return queue[0];
	for (uint8_t i = 0; i < len; ++i) sum += queue[i];
	sum += len >> 1;								// round the average
	sum /= len;
	return sum;
}

int32_t	HIST::average(int32_t value) {
	update(value);
	return read();
}

void HIST::update(int32_t value) {
	if (len < max_len) {
		queue[len++] = value;
	} else {
		queue[index] = value;
		if (++index >= max_len) index = 0;			// Use ring buffer
	}
}

uint32_t HIST::dispersion(void) {
	if (len < 3) return 1000;
	uint32_t sum = 0;
	uint32_t avg = read();
	for (uint8_t i = 0; i < len; ++i) {
		int32_t q = queue[i];
		q -= avg;
		q *= q;
		sum += q;
	}
	sum += len >> 1;
	sum /= len;
	return sum;
}

void SWITCH::init(uint8_t h_len, uint16_t off, uint16_t on) {
	EXPA::length(h_len);
    if (on < off) on = off;
    on_val    	= on;
    off_val   	= off;
    mode		= false;
}


bool SWITCH::changed(void) {
	if (sw_changed) {
		sw_changed = false;
		return true;
	}
	return false;
}

void SWITCH::update(uint16_t value) {
	uint16_t max_val = on_val  + (on_val  >> 1);
	uint16_t min_val = off_val - (off_val >> 1);
	value = constrain(value, min_val, max_val);
	uint16_t avg = EXPA::average(value);
	if (mode) {
		if (avg < off_val) {
			sw_changed	= true;
			mode		= false;
		}
	} else {
		if (avg > on_val) {
			sw_changed = true;
			mode 	= true;
		}
	}
}

/*
 * Calculate tip calibration parameter using linear approximation by Ordinary Least Squares method
 * Y = a * X + b, where
 * Y - internal temperature, X - real temperature. a and b are double coefficients
 * a = (N * sum(Xi*Yi) - sum(Xi) * sum(Yi)) / ( N * sum(Xi^2) - (sum(Xi))^2)
 * b = 1/N * (sum(Yi) - a * sum(Xi))
 */
bool OLS::loadOLS(uint16_t X[], uint16_t Y[], bool filter[], uint16_t size) {
	int64_t sum_XY	= 0;										// sum(Xi * Yi)
	int64_t sum_X	= 0;										// sum(Xi)
	int64_t sum_Y	= 0;										// sum(Yi)
	int64_t sum_X2	= 0;										// sum(Xi^2)
	int32_t N		= 0;

	for (uint16_t i = 0; i < size; ++i) {
		if (filter[i]) {
			sum_XY 	+= X[i] * Y[i];
			sum_X	+= X[i];
			sum_Y   += Y[i];
			sum_X2  += X[i] * X[i];
			++N;
		}
	}

	if (N < 2)													// Not enough real data have been entered
		return false;

	a  = (double)N * (double)sum_XY - (double)sum_X * (double)sum_Y;
	a /= (double)N * (double)sum_X2 - (double)sum_X * (double)sum_X;
	b  = (double)sum_Y - a * (double)sum_X;
	b /= (double)N;
	return true;
}

// Calculate Y by X
void OLS::approximate(uint16_t X[], uint16_t Y[], uint16_t size) {
	for (uint16_t i = 0; i < size; ++i) {
		double y = a * (double)X[i] + b;
		Y[i] = round(y);
	}
}
