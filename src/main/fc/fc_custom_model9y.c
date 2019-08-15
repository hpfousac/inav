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

void taskMainPidLoop(timeUs_t currentTimeUs)
{
int16_t 
    midrc = rxConfig()->midrc,
    servo[SERVO_OUT_CHANNELS];

    UNUSED(currentTimeUs);

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

    for (int16_t i = 0; i < SERVO_OUT_CHANNELS; ++i) {
    const servoParam_t *iServoParam = servoParams(i);
        /* scale */
        servo[i] = ((int32_t)iServoParam->rate * servo[i]) / 100L;

        /* normalize for output */
        servo[i] = iServoParam->middle + servo[i];

        /* limiting */
        servo[i] =  constrain(servo[i], iServoParam->min, iServoParam->max);
		pwmWriteServo (i, servo[i]);
    }
}
