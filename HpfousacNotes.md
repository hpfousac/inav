# inav-1.9.1-servosOut #

 New branch, mostly for study on STM32 behaviour

* make RX_PPM enabled by default
 featureNames[0] = "RX_PPM"

 * related compile time symbol: USE_RX_PPM

 <featureMask()>, 

 from cli: <featureClear(mask);>, <featureSet(mask);> -- tady jsem zatim skoncil --

 * featureConfig() ?

 * featureConfigMutable() ? (also found in 2.0.1)

 On general what is difference between * & *Mutable.

 * pwmWriteServo(): src/main/drivers/pwm_output.c

 typo found there: USE_PMW_SERVO_DRIVER

 found *servos[index]->ccr as pointer to HW registers

## Designed features ##

  * extend list of pins to max possible number (Model-Y needs 9 out channels)

  * controlled by "old manner/MSP_" GUI from MultiWii (get/set value)

  * controlled by command line

  * make separate settings for neutral(s) and reverses. there is only one neutral.

~~~~
  rxConfig()->midrc
~~~~

  found message *MSP_SERVO_CONFIGURATIONS* and *MSP_SET_SERVO_CONFIGURATION*
  
 ~~~~
 case MSP_SERVO_CONFIGURATIONS:
        for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
            sbufWriteU16(dst, servoParams(i)->min);
            sbufWriteU16(dst, servoParams(i)->max);
            sbufWriteU16(dst, servoParams(i)->middle);
            sbufWriteU8(dst, servoParams(i)->rate);
            sbufWriteU8(dst, 0);
            sbufWriteU8(dst, 0);
            sbufWriteU8(dst, servoParams(i)->forwardFromChannel);
            sbufWriteU32(dst, servoParams(i)->reversedSources);
        }
 ~~~~

## Proceeded steps ##

 * removed symbols **TIM_USE_xxx** except **TIM_USE_ANY** and **TIM_USE_FW_SERVO**

 * removed mixers

 * excluded many HW components from *Makefile*


### System State ###

bit array containig rew stages.

### IO Port map ###

 Defined type: **ioRec_t** array with size **DEFIO_IO_USED_COUNT** defined in 
 *src/main/drivers/io.c*. Size of array is derived form ports specified as **TARGET_IO_PORTx** is
 particular *target.c* file.
 The array is named **ioRecs**. It is not configured it is initialized in *void IOInitGlobal(void);*.

 *How program work with this array should be investigated.*

### Configuration and Parameters ###

 The code is comming from Cleanflight project and definitions are in *parameter_group.h*.

 Discover how configuration is stored in persistent store.

### Tasks ###

 In **fc_tasks** there is table containing tasks including its enable, disable during boot.

 Check for **cfTask_t**

### Features ###

 See **feature()** function. It looks like bitmask.

## planned steps ##

 * modify GUI interface (set get rc out)

 * add commands (set get) to CLI (possible to change it)

 * ported to SP3RACING, BLUEPILL

# Elimination unnecessary code as discovered in On branch extractPpmIn #

 The one codefile with defined symbols USE_

 * src/main/target/common.h

 * src/main/target/<USED BOARD>/target.h

   - here was found 2 interesting symbols:

   * DEFAULT_FEATURES *investigate*
 
   * DEFAULT_RX_TYPE *investigate*

 some code was commented out in Makefile, but this is not a good way how to do.

 <Side effect> somewhere is defined symbol <FLASH_SIZE>, *investigate*

 The other thoughts: the resulting code size can be lowered by excluding unused features and/or modules

# Other investigation should go there: #

 Check if some commands/features in CLI is disabled when related USE_ is commented out

  Probably found typo: <USE_PMW_SERVO_DRIVER>

# Best practice/way (recomendations) #

 Step by step disable one *USE_* then another and see what happens, and or if code is compilable

# Other #

## Compile-time Symbols ##

Some of them are defined somewhere else from *TARGET*, **Makefile** and **target.mk**.

AVOID_UART2_FOR_PWM_PPM
AVOID_UART3_FOR_PWM_PPM

BRUSHED_ESC_AUTODETECT

CLI_MINIMAL_VERBOSITY

MAX_PWM_OUTPUT_PORTS
MAX_PWM_MOTORS
MAX_PWM_SERVOS

ONBOARDFLASH

RX_CHANNELS_TAER

SOFTSERIAL_LOOPBACK

SKIP_TASK_STATISTICS

 * **STACK_CHECK** - checks stack usage, probably for development purposes.

USE_ADC
USE_ASYNC_GYRO_PROCESSING
USE_BLACKBOX
USE_CLI
USE_CMS - neni u F1
USE_DEBUG_TRACE

 * **USE_EXTI** - enables externa INT for MAG, MPU, or some others, and it can be used or not used on specific HW

USE_FLASHFS
USE_GPS
USE_HARDWARE_REVISION_DETECTION
USE_NAV
NAV_NON_VOLATILE_WAYPOINT_STORAGE
USE_PITOT
USE_PMW_SERVO_DRIVER
USE_QUAD_MIXER_ONLY
USE_RANGEFINDER_HCSR04
USE_RCDEVICE
USE_RX_CX10
USE_RX_ELERES
USE_RX_H8_3D
USE_RX_INAV
USE_RX_SOFTSPI
USE_SERIALRX_SPEKTRUM
USE_SERVOS
USE_SOFTSPI
USE_SPEKTRUM_BIND
USE_TELEMETRY

USE_UART1
USE_UART2
USE_UART2_RX_DMA
USE_UART2_TX_DMA
USE_UART3
USE_UART4
USE_UART5
USE_UART6
USE_UART7
USE_UART8

 * **USE_SOFTSERIAL1** - ?

 * **USE_SOFTSERIAL2** - ?

USE_VCP

## Config arrays/blocks ##

 Here should be detected **_System** and **_Mutable** and ...

gyroConfig
navConfig
rcControlsConfig
telemetryConfig

PG_FOREACH - vypada jako ze ma v pozadi nejaky seznam memory bloku

PG_REGISTER_WITH_RESET_FN - dalsi zajimave makro

# Recomended changes #
typedef struct timerHardware_s

 move conditionally added fields to the end of list. Obey this can result in strange behaviour after 
 sucessfull compilation and linking based on wrong combination of parameters/attributes.

# Subsystems #

## Serial Port(s) ##

 branch: **inav-1.9.1-serialOnly**

### Initialization ###

 * **uartPort_t *serialUARTx(uint32_t baudRate, portMode_t mode, portOptions_t options);** Low level init

 * **serialPort_t *uartOpen(USART_TypeDef *USARTx,  ...);** higher level the **USART_TypeDef** is coming from **CMISIS**

 * **serialPort_t *openSerialPort();**

### operations ###

 * **serialBeginWrite(serialPort_t *instance)**

  **find which task is responsible for reading CLI/MSP serial** check for **TASK_SERIAL**
  **taskHandleSerial ()**, **cliProcess();** there is wariable **cliPort** check **cliEnter()**
  see: **mspSerialAllocatePorts ();** which is calling **openSerialPort();** using appropriate parameters.


 **serialInit(bool softserialEnabled, serialPortIdentifier_e serialPortToDisable);**


## RX ##

 Initialisation is done thru **rxInit ()** in **rx.c**.

 rxConfig, rxRuntimeConfig

 **failsafeInit()** needs to be discovered

 The all checks for serial usage has to be wiped out:

findSerialPortConfig(FUNCTION_RX_SERIAL)
findSerialPortUsageByIdentifier(identifier)

it looks that **serialInit ();** has to be called.

raw RX data are read and evaluated by: **calculateRxChannelsAndUpdateFailsafe()** when
**rxRuntimeConfig.rcReadRawFn** is initialised to appropriate function pointer.

[TASK_RX] -> taskUpdateRxMain() -> processRx() -> calculateRxChannelsAndUpdateFailsafe()

## PWM Out ##

 pwmServoConfig () in file: pwm_output.c called from pwmInit() pwm_mapping.c
 pwmWriteServo()

## board modification of default settings ##

~~~
// alternative defaults settings for -censored- targets
void targetConfiguration(void) ...
~~~

 check code snippet from *src/main/fc/config.c*

~~~
#if defined(TARGET_CONFIG)
    targetConfiguration();
#endif
~~~

# inav-1.9.1-serialSpkTest #

focused on testung with serial spektrum receiver like SPM4649T. 

## receiver part ##

 It is in `spektrum.c` file and it is initialised in function `spektrumInit()`, where is set callback to
 function `spektrumDataReceive()`, where part of frame is processed.

 next fragment describes *(I hope)* relevant settings for **RX_SERIAL**.

~~~ 
#define USE_SERIALRX_SPEKTRUM
#define DEFAULT_FEATURES        (FEATURE_TX_PROF_SEL | FEATURE_BLACKBOX | FEATURE_VBAT | FEATURE_PWM_OUTPUT_ENABLE)
#define DEFAULT_RX_TYPE         RX_TYPE_SERIAL
#define SERIALRX_UART           SERIAL_PORT_USART3
#define SERIALRX_PROVIDER       SERIALRX_SPEKTRUM1024
~~~

# feature/inav-1.9.1-modelY #

## regular init and port allocation sequence ##

 * boot log

 * system init (i.e. clock)

 * detect hardware

 * EEPROM (i.e. read config)

 * i2c set speed

 * led init

 * timerInit();

 * serial init (incl MSP serial)

 * servos init

 * allocate pwm & ppm inputs, if enabled *it may explain why some pins are not activated properly*

 ~~~
 #if defined(USE_RX_PWM) || defined(USE_RX_PPM)
    pwmRxInit(systemConfig()->pwmRxInputFilteringMode);
#endif
 ~~~

  * `pwmInit();` loop over each timer pin (in order to priority, first user of pin wins)

   * uart2 on STM32F10X

   * soft serials

   * WS2811 timer

   * ADC pins if is used

   * rangefinder

   * // Handle timer mapping to PWM/PPM inputs?

   * check if multirotor or helicopter/airplane then assign mapping to (in mentioned order) servo, motor

   * if pin is marked as **MAP_TO_PPM_INPUT** or **MAP_TO_PWM_INPUT** then it is not included in further evaluation even
    the feature(s) are disabled **USE_RX_PPM** and **USE_RX_PWM**.

### TIMER_CHANNEL_MAPPED/BOOT_EVENT_TIMER_CH_MAPPED ###

 * **0** - MAP_TO_PPM_INPUT

 * **1** - MAP_TO_PWM_INPUT

 * **2** - MAP_TO_MOTOR_OUTPUT

 * **3** - MAP_TO_SERVO_OUTPUT

###  TIMER_CHANNEL_SKIPPED/BOOT_EVENT_TIMER_CH_SKIPPED ###

 Modified by me

 * **0** - OLIMEXINO_UNCUT_LED2_E_JUMPER/PWM2

 * **1** - useUART2

 * **2** - useUART3

 * **3** - useUART6

 * **4** - SOFTSERIAL_1_RX_PI

 * **5** - SOFTSERIAL_2_RX_PIN

 * **6** - WS2811_TIMER

 * **7** - WS2811_PIN

 * **8** - VBAT_ADC_PIN

 * **9** - RSSI_ADC_PIN

 * **10** - CURRENT_METER_ADC_PIN

 * **11** - useTriggerRangefinder

 * **12** - pwmIOConfiguration.motorCount >= MAX_MOTORS

 * **13** - `pwmMotorConfig()` - failed

 * **14** - pwmIOConfiguration.servoCount >=  MAX_SERVOS

 * **15** - `pwmServoConfig()` failed

