#MIXER_CUSTOM_SAILPLANE

The name should evoque no motor(s)/engine(s) at all, just servos.

## Basic motivation/rationale

Existing code doesn't supports servos on all outputs. Pins 1 & 2 only supports motor.

Second point is: There can be a plane with more than 6 control surfaces. In my case they are:

Left Aile
Left Flap
Right Flap
Right Aile
Y-Tail 3 surfaces (like MQ-9 Reaper or splitted elevator)

## test setup

# mixer
mixer CUSTOMSAILPLANE

# feature
feature PWM_OUTPUT_ENABLE
feature RX_PPM
feature -VBAT

# servo
servo 0 1000 2000 1500 100 -1 
servo 1 1000 2000 1500 100 -1 
servo 2 1000 2000 1500 100 -1 
servo 3 1000 2000 1500 100 -1 
servo 4 1000 2000 1500 100 -1 
servo 5 1000 2000 1500 100 -1 
servo 6 1000 2000 1500 100 -1 
servo 7 1000 2000 1500 100 -1 

servo
 
# servo mix

# Map: AETR5678
# smix n SERVO_ID SIGNAL_SOURCE RATE SPEED
smix reset

smix 0 0 4 100 100 # L-AILE - unstabilised
smix 1 1 4 100 100 # R-AILE - unstabilised

smix 2 2 5 50 100 # L-Y
smix 3 2 6 50 100 # L-Y

smix 4 3 5 50 100 # R-Y
smix 5 3 6 50 100 # R-Y

smix 6 4 6 100 100 # RUDD
smix 7 5 7 100 100 # THRO

smix 8 6 8 100 100 # GEAR
smix 9 7 9 100 100 # FLAPS


smix reverse 3 9 r # ? (25.2. not working as expected)
smix reverse 5 5 r # tady reverzovani na vyskovku funguje

# smix 1:1
mmix reset
smix reset
smix 0 0  4 100 0
smix 1 1  5 100 0
smix 2 2  6 100 0
smix 3 3  7 100 0
smix 4 4  8 100 0
smix 5 5  9 100 0
smix 6 6 10 100 0
smix 7 7 11 100 0



save

# mmix + smix
mmix reset
mmix 0  1.000  0.000  0.000  0.000
mmix 1  1.000  0.000  0.000  0.000

smix reset
smix 0 2  6 100 0
smix 1 3  7 100 0
smix 2 4  8 100 0
smix 3 5  9 100 0
smix 4 6 10 100 0
smix 5 7 11 100 0

# actual behaviour
# TxCh3 -> FcCh0
# ...
# TxCh8 -> FcCh5
# FcCh6-7: no signal
#

save
