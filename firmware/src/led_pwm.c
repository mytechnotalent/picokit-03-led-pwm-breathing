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
// File:    led_pwm.c
// Desc:    Implements the hardware PWM breathing LED helper.
// Created: 2026

#include "pico/stdlib.h"
#include "picokit_03_led_pwm_breathing.h"
#include "led_pwm.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef GPIO_FUNC_PWM
/**
 * @brief GPIO function selector value for the PWM peripheral.
 */
#define GPIO_FUNC_PWM 4u
#endif

/**
 * @brief Clock divider applied to the breathing LED PWM slice.
 */
#define LED_PWM_CLKDIV 1.0f

/**
 * @brief Wrap value that sets the breathing LED PWM period.
 */
#define LED_PWM_WRAP 1000u

bool led_pwm_init(void) {
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, LED_PWM_CLKDIV);
    pwm_config_set_wrap(&cfg, (uint16_t)LED_PWM_WRAP);
    pwm_init(pwm_gpio_to_slice_num(PICOKIT_03_LED_PWM_BREATHING_PWM_LED_PIN), &cfg, true);
    gpio_set_function(PICOKIT_03_LED_PWM_BREATHING_PWM_LED_PIN, GPIO_FUNC_PWM);
    led_pwm_set_duty(0u);
    return true;
}

void led_pwm_set_duty(uint16_t duty) {
    if (duty > LED_PWM_DUTY_MAX) {
        duty = LED_PWM_DUTY_MAX;
    }
    pwm_set_gpio_level(PICOKIT_03_LED_PWM_BREATHING_PWM_LED_PIN, duty);
}
