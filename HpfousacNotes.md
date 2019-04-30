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

  * make separate settings for neutral(s) and reverses


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

Best practice/way (recomandations)

 Step by step disable one *USE_* then another and see what happens, and or if code is compilable
