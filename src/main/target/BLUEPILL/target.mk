F1_TARGETS   += $(TARGET)
FEATURES     =  HIGHEND

TARGET_SRC = \
            drivers/flash_m25p16.c \
            drivers/light_ws2811strip.c \
            drivers/light_ws2811strip_stdperiph.c \
            hardware_revision.c
