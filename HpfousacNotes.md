inav-1.9.1-servosOut

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
