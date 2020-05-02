# Servo initialisation


## from error "Not enough servo outputs/timers"

 Found in **src/main/drivers/pwm_mapping.c** there is a refference to **servos.c: getServoCount()**.

## Functions responsible for writting to the servo

 **pwm_output.c: static void pwmServoWriteStandard(uint8_t index, uint16_t value);**

 **pwm_output.c: static void pwmServoWriteExternalDriver(uint8_t index, uint16_t value)**

 First one uses on chip timers, second one is using external PWM board.
 The idea is to extend function cross both resources.

 Check following symbol **FEATURE_PWM_SERVO_DRIVER**

## pwmBuildTimerOutputList

### modify servos + motors initialization
 - **pwmBuildTimerOutputList** this method has to be modified

 - **pwmInitServos ()**