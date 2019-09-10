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

#include <stdint.h>

#include <platform.h>
#include "drivers/io.h"
#include "drivers/pwm_mapping.h"
#include "drivers/timer.h"

const timerHardware_t timerHardware[] = {
    // MOTOR outputs
    DEF_TIM(TIM8,  CH1, PC6,  0, 1),
    DEF_TIM(TIM8,  CH2, PC7,  0, 1),
    DEF_TIM(TIM8,  CH3, PC8,  0, 1),
    DEF_TIM(TIM8,  CH4, PC9,  0, 1),

    // Additional servo outputs
    DEF_TIM(TIM3,  CH2, PA4,  0, 0),
    DEF_TIM(TIM3,  CH4, PB1,  0, 0),

    // PPM PORT - Also USART2 RX (AF5)
    DEF_TIM(TIM2,  CH4, PA3,  TIM_USE_PPM, 0),

    // LED_STRIP
    DEF_TIM(TIM1,  CH1, PA8,  TIM_USE_ANY, 0),

    // PWM_BUZZER
    DEF_TIM(TIM16, CH1, PB4,  TIM_USE_BEEPER, 0),
};

const int timerHardwareCount = sizeof(timerHardware) / sizeof(timerHardware[0]);
