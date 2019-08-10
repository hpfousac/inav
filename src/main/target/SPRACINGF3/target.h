/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#define TARGET_BOARD_IDENTIFIER "SRF3"

//#define LED0                    PB3

//#define BEEPER                  PC15
//#define BEEPER_INVERTED

#define USE_EXTI
#define GYRO_INT_EXTI            PC13
#define EXTI_CALLBACK_HANDLER_COUNT 2 // MPU data ready and MAG data ready

#define USE_FLASHFS
#define USE_FLASH_M25P16

#define USE_UART1
#define USE_UART2
#define USE_UART3
#define USE_SOFTSERIAL1
#define USE_SOFTSERIAL2
#define SERIAL_PORT_COUNT       5

#define UART1_TX_PIN            PA9
#define UART1_RX_PIN            PA10

#define UART2_TX_PIN            PA14 // PA14 / SWCLK
#define UART2_RX_PIN            PA15

#define UART3_TX_PIN            PB10 // PB10 (AF7)
#define UART3_RX_PIN            PB11 // PB11 (AF7)

#define SOFTSERIAL_1_RX_PIN     PB4
#define SOFTSERIAL_1_TX_PIN     PB5
#define SOFTSERIAL_2_RX_PIN     PB0
#define SOFTSERIAL_2_TX_PIN     PB1

#define USE_I2C
#define USE_I2C_DEVICE_1

#define USE_SPI
#define USE_SPI_DEVICE_2 // PB12,13,14,15 on AF5

#define ENABLE_BLACKBOX_LOGGING_ON_SPIFLASH_BY_DEFAULT

#define DEFAULT_FEATURES        (FEATURE_TX_PROF_SEL | FEATURE_BLACKBOX | FEATURE_VBAT)
#define DEFAULT_RX_TYPE         RX_TYPE_PPM

#define USE_SERIAL_4WAY_BLHELI_INTERFACE

// Number of available PWM outputs
#define MAX_PWM_OUTPUT_PORTS    12

// IO - stm32f303cc in 48pin package
#define TARGET_IO_PORTA         0xffff
#define TARGET_IO_PORTB         0xffff
#define TARGET_IO_PORTC         (BIT(13)|BIT(14)|BIT(15))
#define TARGET_IO_PORTF         (BIT(0)|BIT(1)|BIT(3)|BIT(4))

#define USABLE_TIMER_CHANNEL_COUNT 17
#define USED_TIMERS             ( TIM_N(1) | TIM_N(2) | TIM_N(3) | TIM_N(4) | TIM_N(15) | TIM_N(16) | TIM_N(17) )

// from rx.h (see BLUEPILL target)
#define PWM_RANGE_MIN 1000
#define PWM_RANGE_MAX 2000
#define PWM_PULSE_MIN   750       // minimum PWM pulse width which is considered valid
#define PWM_PULSE_MAX   2250      // maximum PWM pulse width which is considered valid
