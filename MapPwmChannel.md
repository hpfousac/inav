# branch: feature/MapPwmChannel #

Idea: The PWM pins (in/out) will bve allocated by settings.

The reasoning is based on not enought output channels, 
Some of the pins are not used if (C-)PPM or Serial RX is attached.

## High level design

The PWM pins defined in **timerHardware_t timerHardware[]** should be attached to
specific purpose by configuration. Not the hardcoded feature set (which is not so obvious for me)

## Usage (CLI)


### list of possibilities

~~~
pwmmap list

 <ndx> <pin> <usage> # direction
 0 PA6 SERVO 1 # out
 1 PA7 MOTOR 1 # out
 2 PA8 CPPM  0 # in
~~~

*NOTE:* Direction is assumed based on assigned purpose

### assignemnt

#### CLI Template

~~~
pwmmap <ch/pin> <function> <index>
~~~

#### CLI real example

 *note* first channel has index **1** on cli, evei internal index is **0**

~~~
pwmmap 1 cppm 0 # in case of CPPM only 0 is allowed

pwmmap 2 servo 1


pwmmap 1 PPM 0
pwmmap 2 SERVO 1
pwmmap 3 SERVO 2
pwmmap 4 SERVO 3
pwmmap 5 SERVO 4
pwmmap 6 SERVO 5
pwmmap 7 SERVO 6
pwmmap 8 SERVO 7
pwmmap 9 SERVO 8
pwmmap 10 SERVO 9
pwmmap 11 SERVO 10
pwmmap 12 SERVO 11
pwmmap 13 SERVO 12
pwmmap 14 SERVO 13
pwmmap 15 SERVO 14
pwmmap 16 SERVO 15
pwmmap 17 SERVO 16
pwmmap 18 motor 1
pwmmap 19 motor 2
pwmmap 20 motor 3
pwmmap 21 motor 4
pwmmap 22 motor 5
pwmmap 23 motor 6
pwmmap 24 motor 7
pwmmap 25 motor 8
pwmmap 26 ANY 0
pwmmap 27 beeper
pwmmap 28 led

pwmmap list
~~~

Following list is based on enumeration **timerUsageFlag_e**

 * cppm/**ppm**(-in)

 * **pwm**(-in)

 * **motor** (mc & fw)

 * **servo** (mc & fw)

 * **led**(_strip)

 * **beeper**


## Code modification

### Add flash stored map

*(Including appropriate structure)*, File: ***main/drivers/pwm_mapping.h***

Method **pwmBuildTimerOutputList()** passes list of PWM timers and assigns outputs use based of config.

 * PG_REGISTER_WITH_RESET_FN

 * PG_REGISTER_WITH_RESET_TEMPLATE

 * PG_REGISTER_ARRAY_I

 * PG_REGISTER_ARRAY

 * PG_REGISTER_ARRAY_WITH_RESET_FN

 * PG_FOREACH

 * PG_DECLARE

- save config - **config_eeprom.c: writeSettingsToEEPROM()**

 Check usage of **FEATURE_PWM_SERVO_DRIVER** - external driver (one if enought)
 **USE_PWM_DRIVER_PCA9685**. *Note:* Some functionalities (ppm/pwm in) are available only
 for chip's timers (not for external PWM driver)

 The symbol **PG_INAV_END** has to be redefined/adjusted due to new ID **PG_PWMMAP_SETTINGS**

#### final structure

 The **timer.h** file contains definition of structure **timerUsageMap_t** and array **timerUsageMap**.

 Ongoing test: will show if settings is persistent over reboots.


### Add CLI commands

 the commands are added in line with structure creation

### Modify PWM pins initialization


 First of all enable LOG and its reading, then the records can be stored (and debugged/evaluated), log is directed to specific port.

 TODO: Check pwmmap PPM-IN and **rxConfig()->receiverType** **USE_RX_PPM** vs. **RX_TYPE_PWM**

 ? how to set/check/list receiver type ?

 ? check if it is in conflicts ? ppm pins against pwm and receiver type

 First test will be focused to moving PPM input over input channels.

 **timer_init ()** **pwmMotorAndServoInit ()**


 **ppmInConfig ()** - tady jsem skoncil a tedy je potreba sahnout do kodu.


# Other

---
# probably bug found:
## lowlevel function fastA2I(const char *s) has no indication when non-number is passed in

## Current Behavior
fastA2I("2"); returns 2; it's OK
fastA2I("20"); returns 21; it's OK
fastA2I("2O"); returns 2; it's probably not OK especially for CLI

## Steps to Reproduce
Try in CLI
1.  serial 
2.
3.
4.
