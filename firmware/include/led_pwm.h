// MIT License
//
// Copyright (c) 2026 Kevin Thomas
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Author:  Kevin Thomas
// Email:   kevin@mytechnotalent.com
// GitHub:  https://github.com/mytechnotalent/picokit-03-led-pwm-breathing
// File:    led_pwm.h
// Desc:    Declares the hardware PWM breathing LED helper.
// Created: 2026

#ifndef LED_PWM_H
#define LED_PWM_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Maximum duty value accepted by the breathing LED helper.
 */
#define LED_PWM_DUTY_MAX 1000u

/**
 * @brief Initialize the breathing LED PWM slice.
 *
 * Configures the breathing LED GPIO for PWM at the fixed carrier and
 * starts the slice with a dark output.
 *
 * @param void No parameters.
 * @return bool true when initialization completed.
 */
bool led_pwm_init(void);

/**
 * @brief Set the breathing LED duty cycle.
 *
 * Values above LED_PWM_DUTY_MAX clamp to the maximum duty.
 *
 * @param duty Requested duty in the zero to LED_PWM_DUTY_MAX range.
 * @return void
 */
void led_pwm_set_duty(uint16_t duty);

#endif // LED_PWM_H
