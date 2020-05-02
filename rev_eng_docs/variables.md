# Variables

This text describes major variables which was investigated during rev engineering work.

## General pin allocation/usage

 * io.c: ioRecs[DEFIO_IO_USED_COUNT]; field: **owner**.

## Servos & Mixer

 * servos.c: int16_t servo[MAX_SUPPORTED_SERVOS]; - preset or calculated value for servo PWM output (range: DEFAULT_SERVO_MIN - DEFAULT_SERVO_MIDDLE - DEFAULT_SERVO_MAX)

## Receivers

 Who is filling **rcChannels**?