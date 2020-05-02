# map function

## internal order of channels

 The internal order is defined by enumeration **rc_controls.h: rc_alias_e**.

 *Actually: ROLL (AILE), PITCH (ELEV), YAW (RUDD), THROTTLE, AUX1, ... . This corresponds with
 content of rcChannelLetters array.*

## Config and variables

 * rx.c: rcChannelLetters[], default AETR

 rxConfig()->rcmap - tam jsou cisla rxin kanalu

 policka vyplnuje funkce **rx.c: parseRcChannels()**

## Command

 * **map** - lists a current settings

 * **map XXXX** - sets a new mapping, the mapping must contain exact num of letters (**MAX_MAPPABLE_RX_INPUTS**)

 * **rxin** - reads a info from array **rx.c: rcChannels[].data**
