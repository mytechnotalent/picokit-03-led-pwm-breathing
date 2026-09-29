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
// File:    monitor.c
// Desc:    Implements the PWM breathing state machine that pairs each breath
//          step with an authenticated LoRa heartbeat.
// Created: 2026

#include "picokit_03_led_pwm_breathing.h"
#include "monitor.h"
#include "led_pwm.h"
#include "radio.h"
#include "status_led.h"
#include "ccm.h"
#include "envelope.h"
#include "field_secrets.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Module-ready flag.
 *
 * Set to true by monitor_init() once the peripherals are configured.
 * monitor_step() returns false while this flag is clear.
 */
static bool g_ready;

/**
 * @brief Current breathing duty value.
 */
static uint16_t g_duty;

/**
 * @brief Current breathing direction, one to rise and zero to fall.
 */
static uint8_t g_rising;

/**
 * @brief Monotonic transmit sequence number.
 */
static uint16_t g_seq;

/**
 * @brief Absolute time in microseconds of the next breath step.
 */
static uint64_t g_next_step_us;

/**
 * @brief Absolute time in microseconds of the next authenticated transmit.
 */
static uint64_t g_next_tx_us;

/**
 * @brief Inbound radio line accumulator.
 */
static char g_rx_line[RADIO_LINE_BUF_LEN];

/**
 * @brief Number of bytes currently held in the inbound line accumulator.
 */
static size_t g_rx_len;

/**
 * @brief AES-128 session key for telemetry.
 */
static uint8_t g_key[CCM_KEY_LEN];

/**
 * @brief True once the telemetry session key has been loaded.
 */
static bool g_key_ready;

/**
 * @brief Configure the onboard heartbeat LED as a dark output.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_state_init_io(void) {
    gpio_init(PICOKIT_03_LED_PWM_BREATHING_LED_PIN);
    gpio_set_dir(PICOKIT_03_LED_PWM_BREATHING_LED_PIN, GPIO_OUT);
    gpio_put(PICOKIT_03_LED_PWM_BREATHING_LED_PIN, 0);
}

/**
 * @brief Reset the duty, sequence, and transmit timing.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_state_init(void) {
    uint64_t now_us = time_us_64();
    g_duty = 0u;
    g_rising = 1u;
    g_seq = 0u;
    g_next_step_us = now_us;
    g_next_tx_us = now_us + (uint64_t)PICOKIT_03_LED_PWM_BREATHING_TX_INTERVAL_MS * 1000u;
    g_ready = true;
}

/**
 * @brief Load the telemetry session key from the field secret.
 *
 * LAB-ONLY: production must provision the session key through OTP rather
 * than embedding a committed key.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_load_key(void) {
    static const uint8_t key[CCM_KEY_LEN] = FIELD_SECRET_KEY;
    memcpy(g_key, key, CCM_KEY_LEN);
    g_key_ready = true;
}

/**
 * @brief Print the boot banner for the breathing lesson.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_banner(void) {
    printf("=== PICOKIT-03 LED PWM BREATHING // DUTY + AUTHENTICATED HEARTBEAT ===\n");
}

/**
 * @brief Derive the field key and announce a ready monitor.
 *
 * @param void No parameters.
 * @return bool true when the field key was derived and installed.
 */
static bool monitor_finish(void) {
    monitor_load_key();
    monitor_banner();
    return true;
}

/**
 * @brief Blink the onboard heartbeat LED exactly once.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_heartbeat(void) {
    gpio_put(PICOKIT_03_LED_PWM_BREATHING_LED_PIN, 1);
    sleep_us(MONITOR_HEARTBEAT_BLINK_US);
    gpio_put(PICOKIT_03_LED_PWM_BREATHING_LED_PIN, 0);
    sleep_us(MONITOR_HEARTBEAT_BLINK_US);
}

/**
 * @brief Print one console line for the current breathing duty.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_log_step(void) {
    printf("BREATH duty=%u seq=%u\n", (unsigned)g_duty, (unsigned)g_seq);
}

/**
 * @brief Advance the breathing duty and reverse at the extremes.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_advance_duty(void) {
    if (g_duty >= LED_PWM_DUTY_MAX) {
        g_rising = 0u;
    } else if (g_duty == 0u) {
        g_rising = 1u;
    }
    g_duty = (uint16_t)(g_rising ? g_duty + MONITOR_BREATH_STEP
                                 : g_duty - MONITOR_BREATH_STEP);
}

/**
 * @brief Drive the breathing LED and schedule the next breath step.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_step_tick(uint64_t now_us) {
    led_pwm_set_duty(g_duty);
    monitor_heartbeat();
    monitor_log_step();
    monitor_advance_duty();
    g_next_step_us = now_us + (uint64_t)MONITOR_STEP_INTERVAL_MS * 1000u;
}

/**
 * @brief Format the heartbeat JSON body for the current duty.
 *
 * @param frame Pointer to the mutable frame output buffer.
 * @param frame_len Capacity of the frame output buffer in bytes.
 * @return size_t Number of JSON bytes written, or zero on overflow.
 */
static size_t monitor_build_frame(char *frame, size_t frame_len) {
    int written = snprintf(frame, frame_len, "{\"n\":%u,\"s\":%u,\"d\":%u}", (unsigned)PACKET_NODE_ID, (unsigned)g_seq, (unsigned)g_duty);
    return (written > 0 && (size_t)written < frame_len) ? (size_t)written : 0u;
}

/**
 * @brief Seal the current heartbeat body into a hex envelope.
 *
 * @param hex Pointer to the NUL-terminated hex output buffer.
 * @param hex_len Capacity of the hex output buffer in bytes.
 * @return bool true when the heartbeat was sealed and encoded.
 */
static bool monitor_seal_frame(char *hex, size_t hex_len) {
    char frame[PICOKIT_03_LED_PWM_BREATHING_FRAME_SIZE];
    uint8_t nonce[ENVELOPE_NONCE_LEN];
    uint8_t ad = (uint8_t)PACKET_NODE_ID;
    size_t frame_len = monitor_build_frame(frame, sizeof(frame));
    envelope_fill_nonce(nonce);
    return envelope_seal_hex(g_key, nonce, &ad, 1u, (const uint8_t *)frame, frame_len, hex, hex_len);
}

/**
 * @brief Build and transmit the authenticated heartbeat frame.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_transmit(void) {
    char hex[ENVELOPE_MAX_HEX_LEN];
    if (!g_key_ready) {
        return;
    }
    if (monitor_seal_frame(hex, sizeof(hex))) {
        radio_send_frame(PICOKIT_03_LED_PWM_BREATHING_UART, (const uint8_t *)hex, strlen(hex));
        g_seq += 1u;
    }
}

/**
 * @brief Transmit one heartbeat and schedule the next transmit.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_tx_tick(uint64_t now_us) {
    monitor_transmit();
    g_next_tx_us = now_us + (uint64_t)PICOKIT_03_LED_PWM_BREATHING_TX_INTERVAL_MS * 1000u;
}

/**
 * @brief Drain inbound radio lines and log every valid +RCV report.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_rx_tick(void) {
    radio_rcv_t rcv;
    while (radio_line_pump(PICOKIT_03_LED_PWM_BREATHING_UART, g_rx_line, &g_rx_len)) {
        if (radio_parse_rcv(g_rx_line, &rcv) == RADIO_RESULT_OK) {
            printf("RX from 0x%04X, %u bytes\n", (unsigned)rcv.sender, (unsigned)rcv.len);
        }
    }
}

/**
 * @brief Service the breath step and heartbeat transmit timers.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_service_timers(uint64_t now_us) {
    if (now_us >= g_next_step_us) {
        monitor_step_tick(now_us);
    }
    if (now_us >= g_next_tx_us) {
        monitor_tx_tick(now_us);
    }
}

bool monitor_init(void) {
    bool ok;
    ok = status_led_init() && radio_init(PICOKIT_03_LED_PWM_BREATHING_UART);
    monitor_state_init_io();
    monitor_state_init();
    return ok && led_pwm_init() && monitor_finish();
}

void monitor_deinit(void) {
    g_ready = false;
}

bool monitor_step(void) {
    uint64_t now_us;
    if (!g_ready) {
        return false;
    }
    now_us = time_us_64();
    monitor_service_timers(now_us);
    monitor_rx_tick();
    return true;
}
