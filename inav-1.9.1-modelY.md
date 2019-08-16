# Dead end branch for ModelY (9ch out) with SPM4649T #

 **Dead end** means the banc wil; not be mergeable to the master
 and other branches dur to significant deletions.

## Steps ##

### Add SERIALRX feature ###

 * disable SOFT_SERIAL

 * disable other serial rx protocols. Check in `target.h`

~~~
#undef USE_SERIALRX_SBUS
#undef USE_SERIALRX_SUMD
#undef USE_SERIALRX_SUMH
#undef USE_SERIALRX_XBUS
#undef USE_SERIALRX_IBUS
#undef USE_TELEMETRY_IBUS
#undef USE_SERIALRX_JETIEXBUS
#undef TELEMETRY_JETIEXBUS
#undef USE_SERIALRX_CRSF
#undef USE_TELEMETRY_CRSF
#undef USE_SERIALRX_FPORT
~~~ 

 * (re?)enable RX_SERIAL, FEATURE_RX_SERIAL

 * add `cliRxIn ()`, proved it somehow works

~~~
# rxin
# RXIN
 rxin 0, rcData=1958
 rxin 1, rcData=988
 rxin 2, rcData=988
 rxin 3, rcData=1994
 rxin 4, rcData=2011
 rxin 5, rcData=1838
 rxin 6, rcData=1983
 rxin 7, rcData=1500
 rxin 8, rcData=1500
 rxin 9, rcData=1500
 rxin 10, rcData=1500
 rxin 11, rcData=1500
 rxin 12, rcData=1500
 rxin 13, rcData=1500
 rxin 14, rcData=1500
 rxin 15, rcData=1500
 rxin 16, rcData=1500
 rxin 17, rcData=1500

# rxin
# RXIN                                                                   
 rxin 0, rcData=1160                                                     
 rxin 1, rcData=988                                                      
 rxin 2, rcData=988                                                      
 rxin 3, rcData=1371                                                     
 rxin 4, rcData=1372
 rxin 5, rcData=988
 rxin 6, rcData=1116
 rxin 7, rcData=1500
 rxin 8, rcData=1500
 rxin 9, rcData=1500
 rxin 10, rcData=1500
 rxin 11, rcData=1500
 rxin 12, rcData=1500
 rxin 13, rcData=1500
 rxin 14, rcData=1500
 rxin 15, rcData=1500
 rxin 16, rcData=1500
 rxin 17, rcData=1500
~~~ 

 * setup pins

  * disable **USE_RX_PWM** and **USE_RX_PPM**
  **TIM_USE_PWM** **TIM_USE_PPM**


 * TBD: replace standard output task with specific one


# HW description/Limitation # 

# Branch: feature/inav-1.9.1-modelY-customCode #

## gear delay ##

 Designed sequence;

 * Closed

 * Open main gear

 * Open front gear

 * Opened

 * Close front gear

 * Close main gear

 * Closed
