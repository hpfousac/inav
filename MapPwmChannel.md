# branch: feature/MapPwmChannel #

Idea: The PWM pins (in/out) will be allocated by settings.

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

Following list is based on **newly created** enumeration **timerUsageFlag_e**

 * cppm/**ppm**(-in)

 * **pwm**(-in)

 * **motor** (mc & fw)

 * **servo** (mc & fw)

 * **led**(_strip)

 * **beeper**

 * **reset**


#### Automatic/Default assigment

**Not implemented yet**

~~~
pwmmap auto
~~~

The **PPM** or **PWM** will be used if appropriate receiver is configured. 
The SERVO will have the priority over MOTOR.

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

 Ongoing test: will show if settings is persistent over reboots. **OK**

### Add CLI commands

 the commands are added in line with structure creation

### Modify (c)PPM pins initialization

 First of all enable LOG and its reading, then the records can be stored (and debugged/evaluated), log is directed to specific port.

 TODO: Check pwmmap PPM-IN and **rxConfig()->receiverType** **USE_RX_PPM** vs. **RX_TYPE_PWM**

 ? how to set/check/list receiver type ?

 ? check if it is in conflicts ? ppm pins against pwm and receiver type

 First test will be focused to moving PPM input over input channels.

 **timer_init ()** **pwmMotorAndServoInit ()**

 **rxPpmInit(rxRuntimeConfig_t)** selects by **timerGetByUsageFlag(TIM_USE_PPM)** approprate port/pin

 **ppmInConfig ()** - there is starting point of investigation what has to be modified

 **timer.c:timerGetByUsageFlag ()** - function was modified to select timer-channel based on pwmmap configuration

#### Modify settings with feature 

~~~
set receiver_type = PPM
pwmmap 10 ppm

save

pwmmap list
~~~

### Modify PWM pins initialization (individual rx servo channels)

### modify servos + motors initialization
 - **pwmBuildTimerOutputList** this method has to be modified

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

# cli commands and configuration consistency

 During work on **pwmmap** feature I realised that commands allows inconsistent configuration.
 There is probably no checks if one set parameter is consistent with another one.

## not to assign same channel twice

 It is checked during CLI setting.

 The PPM LED and BEEPER channel can be assingned only once.

  The PWM, SERVO and MOTOR are checked for assignment and index.

 The 

## not to assign servo & motor channels without "holes"

 The hole in used indexes (field: **devndx**) can cause HW/SW crash.

# default behaviour of FEATURE_PWM_SERVO_DRIVER

 See **void pwmServoPreconfigure(void)**, the internal timers is used of external driver.
 The original code does not allows extend internal timers with external driver it is one or second.

 * **pwmServoWriteStandard ();**

 * **pwmServoWriteExternalDriver ();**

# Tests

 It is intended to proof of configuration option is working as expected.

## configuration persistence

 * set some confguration and save it

 * show configuration after device reboot

## pwmmap servo motor assignment

~~~
# tedsted for SPK6Ch PPM DSM2/X
map AETR

feature PWM_OUTPUT_ENABLE
feature -VBAT

set receiver_type = PPM
pwmmap 10 ppm

pwmmap 1 servo 2

pwmmap 1 servo 1
pwmmap 2 servo 1
pwmmap 2 servo 2
pwmmap 3 servo 3
pwmmap 4 servo 4

pwmmap 5 servo 5
pwmmap 6 servo 6
pwmmap 7 servo 7
pwmmap 8 servo 8

pwmmap 9 servo 0
pwmmap 1 servo 0

# put thro to 1 pin
pwmmap reset

pwmmap 2 servo 1
pwmmap 3 servo 2
pwmmap 4 servo 3
pwmmap 1 servo 4

pwmmap list

smix reset
smix 0 1 4 100 100 # 4 - Raw ROLL RC channel (https://github.com/iNavFlight/inav/blob/master/docs/Mixer.md)
smix 1 2 5 100 100 # 5 - Raw PITCH RC channel
smix 2 3 6 100 100 # 6 - Raw YAW RC channel
smix 3 4 7 100 100 # 7 - Raw THRO RC channel
smix 4 5 8 100 100 # 8 - Raw RC channel 5
smix 5 6 9 100 100 # 9 - Raw RC channel 6

smix 1 0 4 100 100 # L-AILE - unstabilised
smix 2 3 4 100 100 # R-AILE - unstabilised

smix 3 1 9 100 50 # elev
smix 4 2 9 100 50 # R-FLAP
smix reverse 4 9 r # ? (25.2. not working as expected)

smix 5 4 5 50 100 # L-Y
smix 6 4 6 50 100 # L-Y

smix 7 5 5 50 100 # R-Y
smix reverse 7 5 r # tady reverzovani na vyskovku funguje

smix 8 6 6 100 100 # RUDD


pwmmap list
smix
save

#
resource
pwmmap list

~~~

# Work status

 - rozpracovane nastaveni PPM in kanalu, otestovat na povolenych pinech, **otestovano na pin 1 a 10**, podle kodu soudim ze to bude fungovat i jinde

 - podivat se na vystupni piny "pwmmap pinNo servo Ch"

 pwmMotorAndServoInit () -> pwmBuildTimerOutputList()

  pro explicitni mapping asi bude nutne prepsat tuto funkci:
  (pwm_mapping.c) pwmBuildTimerOutputList()
 
- stav: Chova se to divne
- kdyz je pwmmap jen jedno servo, resources neukazuje nic a na pinu neni signal
- kdyz je pwmmap servo 1-8 tak resources ukazuji motor 1-7 (proverit co to znamena) a na pinech je signal tajy jen 1-7.
- pwmmap 1-6 no res
- pwmmap 7 servo - motor 1-6, signal 1-7

MAP TAER

 S1 - ELEV Ch3
 S2 - THRO Ch1
 S3 - RUDD Ch4
 S4 - AILE Ch2
 S5 - GEAR Ch5
 S6 - FLAP Ch6

map etar
map rtae

map taer


# status
System Uptime: 57 seconds
Current Time: 2041-06-28T01:04:00.000+00:00
Voltage: 0.00V (1S battery - NOT PRESENT)
CPU Clock=72MHz, GYRO=MPU6050, ACC=MPU6050
STM32 system clocks:
  SYSCLK = 72 MHz
  HCLK   = 72 MHz
  PCLK1  = 36 MHz
  PCLK2  = 72 MHz
Sensor status: GYRO=OK, ACC=OK, MAG=NONE, BARO=NONE, RANGEFINDER=NONE, OPFLOW=NONE, GPS=NONE
Stack size: 6144, Stack address: 0x10002000, Heap available: 904
I2C Errors: 0, config size: 4657, max available config: 6144
ADC channel usage:
   BATTERY : configured = ADC 1, used = ADC 1
      RSSI : configured = ADC 3, used = none
   CURRENT : configured = ADC 2, used = none
  AIRSPEED : configured = none, used = none
System load: 14, cycle time: 1014, PID rate: 986, RX rate: 495, System rate: 9
Arming disabled flags: ACC CLI PWMOUT
PWM output init error: Not enough servo outputs/timers

