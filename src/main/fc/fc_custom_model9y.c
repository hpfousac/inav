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


# define OUT_THRO	0
# define OUT_LYTAIL	1 // left Y-tail
# define OUT_RYTAIL	2 // right Y-tail
# define OUT_RUDD	3

# define OUT_LAILE	4 // left aile
# define OUT_LFLAP  5 // left flap
# define OUT_RFLAP  6 // right flap
# define OUT_RAILE	7 // right aile

# define OUT_GEAR   8



void taskMainPidLoop(timeUs_t currentTimeUs)
{
int16_t 
    midrc = rxConfig()->midrc,
    iThro  = rcData[THROTTLE] - midrc,
    iYaw   = rcData[YAW]      - midrc,
    iRoll  = rcData[ROLL]     - midrc,
    iPitch = rcData[PITCH]    - midrc,
    iFlaps = rcData[AUX2]     - midrc,
    iGear  = rcData[AUX1]     - midrc;
    // input[INPUT_RC_AUX3]     = rcData[AUX3]     - rxConfig()->midrc;
    // input[INPUT_RC_AUX4]     = rcData[AUX4]     - rxConfig()->midrc;

    UNUSED(currentTimeUs);


	pwmWriteServo (OUT_THRO,   midrc + iThro);
	pwmWriteServo (OUT_RAILE,  midrc + iRoll);
	pwmWriteServo (OUT_LAILE,  midrc + iRoll);
	pwmWriteServo (OUT_RYTAIL, midrc + iPitch - (iYaw >> 2));
	pwmWriteServo (OUT_LYTAIL, midrc - iPitch - (iYaw >> 2));
	pwmWriteServo (OUT_RUDD,   midrc + iYaw);

	pwmWriteServo (OUT_RFLAP,  midrc + iFlaps);
	pwmWriteServo (OUT_LFLAP,  midrc - iFlaps);

	pwmWriteServo (OUT_GEAR,   midrc + iGear);

}
