/*
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "platform.h"

#include "blackbox/blackbox.h"

#include "build/debug.h"

#include "common/maths.h"
#include "common/axis.h"
#include "common/color.h"
#include "common/utils.h"
#include "common/filter.h"

#include "drivers/light_led.h"
#include "drivers/serial.h"
#include "drivers/time.h"
#include "drivers/pwm_output.h"

#include "sensors/sensors.h"
#include "sensors/diagnostics.h"
#include "sensors/boardalignment.h"
#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/pitotmeter.h"
#include "sensors/gyro.h"
#include "sensors/battery.h"
#include "sensors/rangefinder.h"
#include "sensors/opflow.h"

#include "fc/fc_core.h"
#include "fc/cli.h"
#include "fc/config.h"
#include "fc/controlrate_profile.h"
#include "fc/rc_adjustments.h"
#include "fc/rc_controls.h"
#include "fc/rc_curves.h"
#include "fc/rc_modes.h"
#include "fc/runtime_config.h"

#include "io/beeper.h"
#include "io/dashboard.h"
#include "io/gimbal.h"
#include "io/gps.h"
#include "io/serial.h"
#include "io/statusindicator.h"
#include "io/asyncfatfs/asyncfatfs.h"

#include "msp/msp_serial.h"

#include "navigation/navigation.h"

#include "rx/rx.h"
#include "rx/msp.h"

#include "scheduler/scheduler.h"

#include "telemetry/telemetry.h"

#include "flight/mixer.h"
#include "flight/servos.h"
#include "flight/pid.h"
#include "flight/imu.h"

#include "flight/failsafe.h"

#include "config/feature.h"


#define OUT_THRO	0
#define OUT_LYTAIL	1 // left Y-tail
#define OUT_RYTAIL	2 // right Y-tail
#define OUT_RUDD	3

#define OUT_LAILE	4 // left aile
#define OUT_LFLAP  5 // left flap
#define OUT_RFLAP  6 // right flap
#define OUT_RAILE	7 // right aile

#define OUT_MAIN_GEAR   8
#define OUT_FRONT_GEAR  9

#define SERVO_OUT_CHANNELS  10


#define SERVO_HALF_RANGE ((DEFAULT_SERVO_MAX - DEFAULT_SERVO_MIN)/2)


typedef enum {
    GEAR_UNKNOWN      = 0, // stable state
    GEAR_CLOSED       = 1,
    GEAR_OPENINGMAIN  = 2,
    GEAR_OPENINGFRONT = 3,
    GEAR_OPEN         = 4, // stable state
    GEAR_CLOSINGFRONT = 5,
    GEAR_CLOSINGMAIN  = 6
} gearState_e;

#define GEAR_MOVE_DURATION_US 4000000 // it can become set parameter


static int16_t prevServoPosition[SERVO_OUT_CHANNELS];
static gearState_e frontGearState = GEAR_UNKNOWN;
static timeUs_t lastGearTimestamp; // usually beginning of some delay

void taskMainPidLoop(timeUs_t currentTimeUs)
{
int16_t 
    midrc = rxConfig()->midrc,
    servo[SERVO_OUT_CHANNELS];

    servo[OUT_THRO]      = rcData[THROTTLE] - midrc;
    servo[OUT_RUDD]      = rcData[YAW]      - midrc;

    servo[OUT_LAILE]     = servo[OUT_RAILE] = rcData[ROLL] - midrc;

    servo[OUT_RYTAIL]    = rcData[PITCH]    - midrc;
    servo[OUT_LYTAIL]    = - servo[OUT_RYTAIL];

    servo[OUT_LFLAP]     = rcData[AUX2]     - midrc;
    servo[OUT_RFLAP]     = - servo[OUT_LFLAP];

    servo[OUT_MAIN_GEAR] = servo[OUT_FRONT_GEAR] = rcData[AUX1] - midrc;

    // input[INPUT_RC_AUX3]     = rcData[AUX3]     - rxConfig()->midrc;
    // input[INPUT_RC_AUX4]     = rcData[AUX4]     - rxConfig()->midrc;

    // V-mix
    servo[OUT_RYTAIL]  -= (servo[OUT_RUDD] >> 2);
    servo[OUT_LYTAIL]  -= (servo[OUT_RUDD] >> 2);

	if (servo[OUT_THRO] >= 0) {
    const servoParam_t *throServoParam = servoParams(OUT_THRO);
        servo[OUT_THRO] = (servo[OUT_THRO] - (((throServoParam->max - throServoParam->min) >> 2) << 1));
//		iOutAirbrake = 0; => OUT_RAILE & OUT_LAILE remains unaffected (by thro)
	} else {
// replace variable usage, iThro is not needed after calling RC_Out_setOut (OUT_THRO, ...);
#define iOutAirbrake	(servo[OUT_THRO] >> 1)
		servo[OUT_RAILE] += iOutAirbrake;
		servo[OUT_LAILE] -= iOutAirbrake;
#undef iOutAirbrake
        servo[OUT_THRO] = DEFAULT_SERVO_MIN - DEFAULT_SERVO_MIDDLE; // should result in -500
	}

    if (servo[OUT_FRONT_GEAR] < 0) { // gear switch down
        switch (frontGearState) {
        case GEAR_UNKNOWN:
            servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->min - midrc;
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->min - midrc;
            frontGearState = GEAR_CLOSED;
            break;
        case GEAR_CLOSED:
            servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->min - midrc;
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->min - midrc;
            break;
        case GEAR_OPENINGMAIN: // wait for fully opened then close
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->min - midrc; // keep closed
            if ((currentTimeUs - lastGearTimestamp) < GEAR_MOVE_DURATION_US) {
                servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->max - midrc; // keep opening
            } else { // main gear considered fully opened
                servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->min - midrc;  // start closing
                frontGearState = GEAR_CLOSINGMAIN;
                lastGearTimestamp = currentTimeUs;
            }
            break;
        case GEAR_OPENINGFRONT: // wait for fully opened then start closing sequention
            servo[OUT_MAIN_GEAR]   = servoParams(OUT_MAIN_GEAR)->max - midrc;  // keep open
            if ((currentTimeUs - lastGearTimestamp) < GEAR_MOVE_DURATION_US) {
                servo[OUT_FRONT_GEAR]  = servoParams(OUT_FRONT_GEAR)->max - midrc; // keep opening
            } else { // main gear considered fully opened
                servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->min - midrc; // start closing
                frontGearState = GEAR_CLOSINGFRONT;
                lastGearTimestamp = currentTimeUs;
            }
            break;
        case GEAR_OPEN: // start closing sequention
            servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->max - midrc; // keep open
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->min - midrc; // start closing
            frontGearState = GEAR_CLOSINGFRONT;
            lastGearTimestamp = currentTimeUs;
            break;
        case GEAR_CLOSINGFRONT:
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->min - midrc; // keep closing/closed
            if ((currentTimeUs - lastGearTimestamp) < GEAR_MOVE_DURATION_US) {
                servo[OUT_MAIN_GEAR]   = servoParams(OUT_MAIN_GEAR)->max - midrc;  // keep open
            } else { // main gear considered fully opened
                servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->min - midrc;  // start closing
                frontGearState = GEAR_CLOSINGMAIN;
                lastGearTimestamp = currentTimeUs;
            }
            break;
        case GEAR_CLOSINGMAIN:
            servo[OUT_FRONT_GEAR]  = servoParams(OUT_FRONT_GEAR)->min - midrc; // keep closed
            servo[OUT_MAIN_GEAR]   = servoParams(OUT_MAIN_GEAR)->min - midrc;  // keep closing/closed
            if ((currentTimeUs - lastGearTimestamp) >= GEAR_MOVE_DURATION_US) {
                frontGearState = GEAR_CLOSED;
            }
            break;
        }
    } else { // gear switch up
        switch (frontGearState) {
        case GEAR_UNKNOWN:
            servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->max - midrc;
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->max - midrc;
            frontGearState = GEAR_OPEN;
            break;
        case GEAR_CLOSED:
            servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->max - midrc; // start opening
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->min - midrc; // keep closed
            frontGearState = GEAR_OPENINGMAIN;
            lastGearTimestamp = currentTimeUs;
            break;
            break;
        case GEAR_OPENINGMAIN:
            servo[OUT_MAIN_GEAR]   = servoParams(OUT_MAIN_GEAR)->max - midrc;  // keep opening/open
            if ((currentTimeUs - lastGearTimestamp) < GEAR_MOVE_DURATION_US) {
                servo[OUT_FRONT_GEAR]  = servoParams(OUT_FRONT_GEAR)->min - midrc; // keep closed
            } else { // main gear considered fully opened
                servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->max - midrc; // start opening
                frontGearState = GEAR_OPENINGFRONT;
                lastGearTimestamp = currentTimeUs;
            }
            break;
        case GEAR_OPENINGFRONT:
            servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->max - midrc;  // keep open
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->max - midrc; // keep opening/open
            if ((currentTimeUs - lastGearTimestamp) >= GEAR_MOVE_DURATION_US) {
                frontGearState = GEAR_OPEN;
            }
            break;
        case GEAR_OPEN:
            servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->max - midrc;
            servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->max - midrc;
            break;
        case GEAR_CLOSINGMAIN:
            servo[OUT_FRONT_GEAR]  = servoParams(OUT_FRONT_GEAR)->min - midrc; // keep closed
            if ((currentTimeUs - lastGearTimestamp) < GEAR_MOVE_DURATION_US) {
                servo[OUT_MAIN_GEAR]   = servoParams(OUT_MAIN_GEAR)->min - midrc;  // keep closing
            } else { // main gear considered fully closed
                servo[OUT_MAIN_GEAR]  = servoParams(OUT_MAIN_GEAR)->max - midrc;  // start opening
                frontGearState = GEAR_OPENINGMAIN;
                lastGearTimestamp = currentTimeUs;
            }
            break;
        case GEAR_CLOSINGFRONT:
            servo[OUT_MAIN_GEAR]   = servoParams(OUT_MAIN_GEAR)->max - midrc;  // keep open
            if ((currentTimeUs - lastGearTimestamp) < GEAR_MOVE_DURATION_US) {
                servo[OUT_FRONT_GEAR]  = servoParams(OUT_FRONT_GEAR)->min - midrc; // keep closing
            } else { // front gear considered fully closed
                servo[OUT_FRONT_GEAR] = servoParams(OUT_FRONT_GEAR)->max - midrc; // start opening
                frontGearState = GEAR_OPENINGFRONT;
                lastGearTimestamp = currentTimeUs;
            }
            break;
        }
    }

    for (int16_t i = 0; i < SERVO_OUT_CHANNELS; ++i) {
    const servoParam_t *iServoParam = servoParams(i);
        /* scale */
        servo[i] = ((int32_t)iServoParam->rate * servo[i]) / 100L;

        /* normalize for output */
        servo[i] = iServoParam->middle + servo[i];

        /* limiting */
        servo[i] =  constrain(servo[i], iServoParam->min, iServoParam->max);

        // move if needed
        if (prevServoPosition[i] != servo[i]) {
		    pwmWriteServo (i, prevServoPosition[i] = servo[i]);
        }
    }
}
