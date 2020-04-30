# inav-1.9.1-servosOut #

 New branch, mostly for study on STM32 behaviour

* make RX_PPM enabled by default
 `featureNames[0] = "RX_PPM"`

 * related compile time symbol: **USE_RX_PPM**

 `featureMask()`,

 from cli: `<`featureClear(mask);`, `featureSet(mask);` -- tady jsem zatim skoncil --

 * `featureConfig()` ?

 * `featureConfigMutable()` ? (also found in 2.0.1)

 On general what is difference between * & *Mutable.

 * `pwmWriteServo()`: src/main/drivers/pwm_output.c

 typo found there: **USE_PMW_SERVO_DRIVER**

 found `*servos[index]->ccr` as pointer to HW registers

## Designed features ##

  * extend list of pins to max possible number (Model-Y needs 9 out channels)

  * controlled by "old manner/MSP_" GUI from MultiWii (get/set value)

  * controlled by command line

  * make separate settings for neutral(s) and reverses. there is only one neutral.

~~~~
  rxConfig()->midrc
~~~~

  found message **MSP_SERVO_CONFIGURATIONS** and **MSP_SET_SERVO_CONFIGURATION**

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

 * excluded many HW components from `Makefile`


### System State ###

bit array containig rew stages.

### IO Port map ###

 Defined type: `ioRec_t` array with size **DEFIO_IO_USED_COUNT** defined in
 `src/main/drivers/io.c`. Size of array is derived form ports specified as **TARGET_IO_PORTx** is
 particular `target.c` file.
 The array is named **ioRecs**. It is not configured it is initialized in `void IOInitGlobal(void);`.

 *How program work with this array should be investigated.*

~~~
 // claim IO pin, set owner and resources
void IOInit(IO_t io, resourceOwner_e owner, resourceType_e resource, uint8_t index);
~~~

 function **IOGetOwner();** has to be used for usage determination

### Configuration and Parameters ###

 The code is comming from Cleanflight project and definitions are in `parameter_group.h`.

 Discover how configuration is stored in persistent store.

### Tasks ###

 In **fc_tasks** there is table containing tasks including its enable, disable during boot.

 Check for **cfTask_t**

### Features ###

 See `feature();` function. It looks like bitmask.

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

 Check if some commands/features in CLI is disabled when related **USE_** is commented out


# Best practice/way (recomendations) #

 Step by step disable one **USE_** then another and see what happens, and or if code is compilable

# Other #

## Compile-time Symbols ##

Some of them are defined somewhere else from *TARGET*, `Makefile` and `target.mk`.

 * **AVOID_UART2_FOR_PWM_PPM**

 * **AVOID_UART3_FOR_PWM_PPM**

 * **BRUSHED_ESC_AUTODETECT**

 * **CLI_MINIMAL_VERBOSITY**

 * **MAX_PWM_OUTPUT_PORTS**

 * **MAX_PWM_MOTORS**

 * **MAX_PWM_SERVOS**

 * **ONBOARDFLASH**

 * **RX_CHANNELS_TAER**

 * **SOFTSERIAL_LOOPBACK**

 * **SKIP_TASK_STATISTICS**

 * **STACK_CHECK** - checks stack usage, probably for development purposes.

 * **USE_ADC**

 * **USE_ASYNC_GYRO_PROCESSING**

 * **USE_BLACKBOX**

 * **USE_CLI**

 * **USE_CMS** - neni u F1

 * **USE_DEBUG_TRACE**

 * **USE_EXTI** - enables externa INT for MAG, MPU, or some others, and it can be used or not used on specific HW

 * **USE_FLASHFS**

 * **USE_GPS**

 * **USE_HARDWARE_REVISION_DETECTION**

 * **USE_NAV**

 * **NAV_NON_VOLATILE_WAYPOINT_STORAGE**

 * **USE_PITOT**

 * **USE_PMW_SERVO_DRIVER**

 * **USE_QUAD_MIXER_ONLY**

 * **USE_RANGEFINDER_HCSR04**

 * **USE_RCDEVICE**

 * **USE_RX_CX10**

 * **USE_RX_ELERES**

 * **USE_RX_H8_3D**

 * **USE_RX_INAV**

 * **USE_RX_SOFTSPI**

 * **USE_SERIALRX_SPEKTRUM**

 * **USE_SERVOS**

 * **USE_SOFTSPI**

 * **USE_SPEKTRUM_BIND**

 * **USE_TELEMETRY**

 * **USE_UART1**

 * **USE_UART2**

 * **USE_UART2_RX_DMA**

 * **USE_UART2_TX_DMA**

 * **USE_UART3**

 * **USE_UART4**

 * **USE_UART5**

 * **USE_UART6**

 * **USE_UART7**

 * **USE_UART8**

 * **USE_SOFTSERIAL1** - ?

 * **USE_SOFTSERIAL2** - ?

 * **USE_VCP**

## Config arrays/blocks ##

 Here should be detected **_System** and **_Mutable** and ...

 * **gyroConfig**

 * **navConfig**

 * **rcControlsConfig**

 * **telemetryConfig**

 `PG_FOREACH` - vypada jako ze ma v pozadi nejaky seznam memory bloku

 `PG_REGISTER_WITH_RESET_FN` - dalsi zajimave makro

# Recomended changes #

`typedef struct timerHardware_s`

 move conditionally added fields to the end of list. Obey this can result in strange behaviour after
 sucessfull compilation and linking based on wrong combination of parameters/attributes.

# Subsystems #

 `io.c`**` - handles work with particular pins. Ouptut statistics is used by CLI resource.

 IO Pins are in array: `ioDefUsedMask` this is the mask, which pins can be used.


## Serial Port(s) ##

 branch: **inav-1.9.1-serialOnly**

### Initialization ###

 * `uartPort_t *serialUARTx(uint32_t baudRate, portMode_t mode, portOptions_t options);` Low level init

 * `serialPort_t *uartOpen(USART_TypeDef *USARTx,  ...);** higher level the **USART_TypeDef` is coming from **CMISIS**

 * `serialPort_t *openSerialPort();`

### operations ###

 * `serialBeginWrite(serialPort_t *instance)`

  **find which task is responsible for reading CLI/MSP serial** check for **TASK_SERIAL**
  `taskHandleSerial ()`, `cliProcess();` there is wariable `cliPort` check `cliEnter()`
  see: `mspSerialAllocatePorts ();` which is calling `openSerialPort();` using appropriate parameters.


 `serialInit(bool softserialEnabled, serialPortIdentifier_e serialPortToDisable);`


## RX ##

 Initialisation is done thru `rxInit ()` in `rx.c`.

 `rxConfig`, `rxRuntimeConfig`

 `failsafeInit()` needs to be discovered

 The all checks for serial usage has to be wiped out:

`findSerialPortConfig(FUNCTION_RX_SERIAL)`

`findSerialPortUsageByIdentifier(identifier)`

it looks that `serialInit ();`` has to be called.

raw RX data are read and evaluated by: `calculateRxChannelsAndUpdateFailsafe()` when
`rxRuntimeConfig.rcReadRawFn` is initialised to appropriate function pointer.

`[TASK_RX] -> taskUpdateRxMain() -> processRx() -> calculateRxChannelsAndUpdateFailsafe()`

## PWM Out ##

 `pwmServoConfig ()` in file: `pwm_output.c` called from `pwmInit()` `pwm_mapping.c`

 symbols from HW definition **MAX_PWM_OUTPUT_PORTS** **USABLE_TIMER_CHANNEL_COUNT**

 `pwmInit()` prepares config for particular pins


 `pwmServoConfig`

 `pwmWriteServo()` writes value to the servo/pin and uses:

 there is `servos[]` and `motors[]`

 `pwm_params.enablePWMOutput = /* feature(FEATURE_PWM_OUTPUT_ENABLE) */ true;` // hardcoded for now

 `servoConfig()->servoCenterPulse` here is place to change for personified center for each servo

  Number of servos is limited by compile-time symbol **MAX_PWM_SERVOS** - in actual development version of upstream project
  is module `pwm_output.*` different.

## board modification of default settings ##

~~~
// alternative defaults settings for -censored- targets
void targetConfiguration(void) ...
~~~

 check code snippet from `src/main/fc/config.c`

~~~
#if defined(TARGET_CONFIG)
    targetConfiguration();
#endif
~~~

## LED Strip ##

Driven by DMA see **timerPWMConfigChannelDMA()** it shiuld be used as inspiration for (C)PPM outpupt to the TX


# Branch: feature/planeOnly #

 Only plane features will be available.
 No specific mixer at all.

## Starting size ##

~~~
Linking SPRACINGF3
Memory region         Used Size  Region Size  %age Used
           FLASH:      217980 B       250 KB     85.15%
    FLASH_CONFIG:          0 GB         6 KB      0.00%
             RAM:       33964 B        40 KB     82.92%
             CCM:        7996 B         8 KB     97.61%
       MEMORY_B1:          0 GB         0 GB      -nan%
arm-none-eabi-size ./obj/main/inav_SPRACINGF3.elf
   text	   data	    bss	    dec	    hex	filename
 215536	   2444	  39512	 257492	  3edd4	./obj/main/inav_SPRACINGF3.elf
arm-none-eabi-objcopy -O ihex --set-start 0x8000000 obj/main/inav_SPRACINGF3.elf obj/inav_2.3.0_SPRACINGF3.hex
~~~

### Modification steps ###

 * Removal of **flyingPlatformType_e** *(defined in mixer.h)*- all references and if sections will be removed.
 The remaining code parts are as was used only **PLATFORM_AIRPLANE** symbol.

 List of all symbols: PLATFORM_MULTIROTOR, PLATFORM_AIRPLANE, PLATFORM_HELICOPTER,
 PLATFORM_TRICOPTER, PLATFORM_ROVER, PLATFORM_BOAT, PLATFORM_OTHER.

 Remove element/field **mixerConfig_t.platformType**

 Remove element/field **servoConfig_t.tri_unarmed_servo**

 * Remove all occurences of **TIM_USE_MC_SERVO** *(Multicopter servo output (i.e. TRI))* + TIM_USE_MC_CHNFW + TIM_USE_MC_MOTOR

 * Remove all ocurences of **USE_AUTOTUNE_MULTIROTOR**

 * Check symbol **FIXED_WING** an remove all other irrelevant STATEs/Options

 * Remove symbol **MR_RTH_CLIMB_MARGIN_PERCENT** - MR means multirotor (it is going together with FIXED_WING) (commit fcc1265c9)

 * Remove symbol **MC_NAV** (commit 58d0284a0)

 * Remove symbol **&pidProfile()->bank_mc**, Remove all multicopter settings (for instance: **nav_mc_pos_z_p**) (commit 46b437e8e

 * **MR_RTH_CLIMB_OVERSHOOT_CM** (not found)
 
 * Remove parameter **rth_tail_first** which is related to the multirotor. (commit 595b8d535)

 * Remove **navConfig()->mc.posDecelerationTime** (commit d6bdd1231)

 * Remove **nav_mc_vel_xy_dterm_lpf_hz** (commit bcab059cc)

 * Remove macro/function **isMixerUsingServos();**, **FLIGHT_LOG_FIELD_CONDITION_SERVOS** (replaced by TRUE, where appropriate) (commit 161dffe9c)

#### TODO ####


#### Notes ####

 Inside function **void servoMixer(float dT);** was a something for head down position for multirotor in 3D.

# SPRACINGF3 #

## CLI SETUP ##

~~~
feature PWM_OUTPUT_ENABLE
~~~


## Code analysis

### Initialisation phase and variables

#### Flow

 * Chip/Controller initialisation (clock and so)

 * Check and Read EEPROM config

~~~
#ifdef USE_EXTI
    EXTIInit();
#endif

#ifdef USE_SPEKTRUM_BIND
    if (rxConfig()->receiverType == RX_TYPE_SERIAL) {
...
                spektrumBind(rxConfigMutable());
...                
    }
#endif
~~~

 * USB Init

 * Timer Init

 * MSP & LOG Serial init

 * Servos init
