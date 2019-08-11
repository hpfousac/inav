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

# HW description/Limitation # 