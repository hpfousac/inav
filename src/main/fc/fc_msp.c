/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "platform.h"

#include "blackbox/blackbox.h"

#include "build/debug.h"
#include "build/version.h"

#include "common/axis.h"
#include "common/color.h"
#include "common/maths.h"
#include "common/streambuf.h"
#include "common/bitarray.h"
#include "common/time.h"
#include "common/utils.h"

#include "drivers/accgyro/accgyro.h"
#include "drivers/bus_i2c.h"
#include "drivers/compass/compass.h"
#include "drivers/max7456.h"
#include "drivers/pwm_mapping.h"
#include "drivers/sdcard.h"
#include "drivers/serial.h"
#include "drivers/system.h"
#include "drivers/time.h"
#include "drivers/vtx_common.h"

#include "fc/config.h"
#include "fc/controlrate_profile.h"
#include "fc/fc_msp.h"
#include "fc/fc_msp_box.h"
#include "fc/rc_adjustments.h"
#include "fc/rc_controls.h"
#include "fc/rc_modes.h"
#include "fc/runtime_config.h"
#include "fc/settings.h"

#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/hil.h"
#include "flight/mixer.h"
#include "flight/pid.h"
#include "flight/servos.h"

#include "config/config_eeprom.h"
#include "config/feature.h"

#include "io/asyncfatfs/asyncfatfs.h"
#include "io/flashfs.h"
#include "io/gps.h"
#include "io/gimbal.h"
#include "io/ledstrip.h"
#include "io/osd.h"
#include "io/serial.h"
#include "io/serial_4way.h"

#include "msp/msp.h"
#include "msp/msp_protocol.h"
#include "msp/msp_serial.h"

#include "navigation/navigation.h"

#include "rx/rx.h"
#include "rx/msp.h"

#include "scheduler/scheduler.h"

#include "sensors/diagnostics.h"


#ifdef USE_HARDWARE_REVISION_DETECTION
#include "hardware_revision.h"
#endif

#undef USE_NAV
#undef VTX_COMMON
#undef USE_FLASHFS
#undef USE_GPS
#undef NAV_NON_VOLATILE_WAYPOINT_STORAGE
#undef USE_BLACKBOX

extern timeDelta_t cycleTime; // FIXME dependency on mw.c

static const char * const flightControllerIdentifier = INAV_IDENTIFIER; // 4 UPPER CASE alpha numeric characters that identify the flight controller.
static const char * const boardIdentifier = TARGET_BOARD_IDENTIFIER;

// placed here because here will be set by MSP call
// and then probably by cli call
int16_t rcData[MAX_SUPPORTED_RC_CHANNEL_COUNT];     // interval [1000;2000]


// from mixer.c
//extern int16_t motor_disarmed[MAX_SUPPORTED_MOTORS];

static const char pidnames[] =
    "ROLL;"
    "PITCH;"
    "YAW;"
    "ALT;"
    "Pos;"
    "PosR;"
    "NavR;"
    "LEVEL;"
    "MAG;"
    "VEL;";

typedef enum {
    MSP_SDCARD_STATE_NOT_PRESENT = 0,
    MSP_SDCARD_STATE_FATAL       = 1,
    MSP_SDCARD_STATE_CARD_INIT   = 2,
    MSP_SDCARD_STATE_FS_INIT     = 3,
    MSP_SDCARD_STATE_READY       = 4
} mspSDCardState_e;

typedef enum {
    MSP_SDCARD_FLAG_SUPPORTTED   = 1
} mspSDCardFlags_e;

typedef enum {
    MSP_FLASHFS_BIT_READY        = 1,
    MSP_FLASHFS_BIT_SUPPORTED    = 2
} mspFlashfsFlags_e;

#ifdef USE_SERIAL_4WAY_BLHELI_INTERFACE
#define ESC_4WAY 0xff

static uint8_t escMode;
static uint8_t escPortIndex;

static void mspFc4waySerialCommand(sbuf_t *dst, sbuf_t *src, mspPostProcessFnPtr *mspPostProcessFn)
{
    const unsigned int dataSize = sbufBytesRemaining(src);

    if (dataSize == 0) {
        // Legacy format
        escMode = ESC_4WAY;
    } else {
        escMode = sbufReadU8(src);
        escPortIndex = sbufReadU8(src);
    }

    switch (escMode) {
    case ESC_4WAY:
        // get channel number
        // switch all motor lines HI
        // reply with the count of ESC found
        sbufWriteU8(dst, esc4wayInit());

        if (mspPostProcessFn) {
            *mspPostProcessFn = esc4wayProcess;
        }
        break;

    default:
        sbufWriteU8(dst, 0);
    }
}

#endif

static void mspRebootFn(serialPort_t *serialPort)
{
    UNUSED(serialPort);


    // extra delay before reboot to give ESCs chance to reset
    delay(1000);
    systemReset();

    // control should never return here.
    while (true) ;
}

static void serializeSDCardSummaryReply(sbuf_t *dst)
{
#ifdef USE_SDCARD
    uint8_t flags = MSP_SDCARD_FLAG_SUPPORTTED;
    uint8_t state;

    sbufWriteU8(dst, flags);

    // Merge the card and filesystem states together
    if (!sdcard_isInserted()) {
        state = MSP_SDCARD_STATE_NOT_PRESENT;
    } else if (!sdcard_isFunctional()) {
        state = MSP_SDCARD_STATE_FATAL;
    } else {
        switch (afatfs_getFilesystemState()) {
            case AFATFS_FILESYSTEM_STATE_READY:
                state = MSP_SDCARD_STATE_READY;
                break;
            case AFATFS_FILESYSTEM_STATE_INITIALIZATION:
                if (sdcard_isInitialized()) {
                    state = MSP_SDCARD_STATE_FS_INIT;
                } else {
                    state = MSP_SDCARD_STATE_CARD_INIT;
                }
                break;
            case AFATFS_FILESYSTEM_STATE_FATAL:
            case AFATFS_FILESYSTEM_STATE_UNKNOWN:
            default:
                state = MSP_SDCARD_STATE_FATAL;
                break;
        }
    }

    sbufWriteU8(dst, state);
    sbufWriteU8(dst, afatfs_getLastError());
    // Write free space and total space in kilobytes
    sbufWriteU32(dst, afatfs_getContiguousFreeSpace() / 1024);
    sbufWriteU32(dst, sdcard_getMetadata()->numBlocks / 2); // Block size is half a kilobyte
#else
    sbufWriteU8(dst, 0);
    sbufWriteU8(dst, 0);
    sbufWriteU8(dst, 0);
    sbufWriteU32(dst, 0);
    sbufWriteU32(dst, 0);
#endif
}

static void serializeDataflashSummaryReply(sbuf_t *dst)
{
#ifdef USE_FLASHFS
    const flashGeometry_t *geometry = flashfsGetGeometry();
    sbufWriteU8(dst, flashfsIsReady() ? 1 : 0);
    sbufWriteU32(dst, geometry->sectors);
    sbufWriteU32(dst, geometry->totalSize);
    sbufWriteU32(dst, flashfsGetOffset()); // Effectively the current number of bytes stored on the volume
#else
    sbufWriteU8(dst, 0);
    sbufWriteU32(dst, 0);
    sbufWriteU32(dst, 0);
    sbufWriteU32(dst, 0);
#endif
}

#ifdef USE_FLASHFS
static void serializeDataflashReadReply(sbuf_t *dst, uint32_t address, uint16_t size)
{
    // Check how much bytes we can read
    const int bytesRemainingInBuf = sbufBytesRemaining(dst);
    uint16_t readLen = (size > bytesRemainingInBuf) ? bytesRemainingInBuf : size;

    // size will be lower than that requested if we reach end of volume
    const uint32_t flashfsSize = flashfsGetSize();
    if (readLen > flashfsSize - address) {
        // truncate the request
        readLen = flashfsSize - address;
    }

    // Write address
    sbufWriteU32(dst, address);

    // Read into streambuf directly
    const int bytesRead = flashfsReadAbs(address, sbufPtr(dst), readLen);
    sbufAdvance(dst, bytesRead);
}
#endif

/*
 * Returns true if the command was processd, false otherwise.
 * May set mspPostProcessFunc to a function to be called once the command has been processed
 */
static bool mspFcProcessOutCommand(uint16_t cmdMSP, sbuf_t *dst, mspPostProcessFnPtr *mspPostProcessFn)
{
    switch (cmdMSP) {
    case MSP_API_VERSION:
        sbufWriteU8(dst, MSP_PROTOCOL_VERSION);
        sbufWriteU8(dst, API_VERSION_MAJOR);
        sbufWriteU8(dst, API_VERSION_MINOR);
        break;

    case MSP_FC_VARIANT:
        sbufWriteData(dst, flightControllerIdentifier, FLIGHT_CONTROLLER_IDENTIFIER_LENGTH);
        break;

    case MSP_FC_VERSION:
        sbufWriteU8(dst, FC_VERSION_MAJOR);
        sbufWriteU8(dst, FC_VERSION_MINOR);
        sbufWriteU8(dst, FC_VERSION_PATCH_LEVEL);
        break;

    case MSP_BOARD_INFO:
    {
        sbufWriteData(dst, boardIdentifier, BOARD_IDENTIFIER_LENGTH);
#ifdef USE_HARDWARE_REVISION_DETECTION
        sbufWriteU16(dst, hardwareRevision);
#else
        sbufWriteU16(dst, 0); // No other build targets currently have hardware revision detection.
#endif
        // OSD support (for BF compatibility):
        // 0 = no OSD
        // 1 = OSD slave (not supported in INAV)
        // 2 = OSD chip on board
#if defined(USE_OSD) && defined(USE_MAX7456)
        sbufWriteU8(dst, 2);
#else
        sbufWriteU8(dst, 0);
#endif
        // Board communication capabilities (uint8)
        // Bit 0: 1 iff the board has VCP
        // Bit 1: 1 iff the board supports software serial
        uint8_t commCapabilities = 0;
#ifdef USE_VCP
        commCapabilities |= 1 << 0;
#endif
#if defined(USE_SOFTSERIAL1) || defined(USE_SOFTSERIAL2)
        commCapabilities |= 1 << 1;
#endif
        sbufWriteU8(dst, commCapabilities);

        sbufWriteU8(dst, strlen(targetName));
        sbufWriteData(dst, targetName, strlen(targetName));
        break;
    }

    case MSP_BUILD_INFO:
        sbufWriteData(dst, buildDate, BUILD_DATE_LENGTH);
        sbufWriteData(dst, buildTime, BUILD_TIME_LENGTH);
        sbufWriteData(dst, shortGitRevision, GIT_SHORT_REVISION_LENGTH);
        break;

    // DEPRECATED - Use MSP_API_VERSION
    case MSP_IDENT:
        sbufWriteU8(dst, MW_VERSION);
        sbufWriteU8(dst, 0);
        sbufWriteU8(dst, MSP_PROTOCOL_VERSION);
        sbufWriteU32(dst, CAP_PLATFORM_32BIT | CAP_DYNBALANCE | CAP_FLAPS | CAP_NAVCAP | CAP_EXTAUX); // "capability"
        break;

#ifdef HIL
    case MSP_HIL_STATE:
        sbufWriteU16(dst, hilToSIM.pidCommand[ROLL]);
        sbufWriteU16(dst, hilToSIM.pidCommand[PITCH]);
        sbufWriteU16(dst, hilToSIM.pidCommand[YAW]);
        sbufWriteU16(dst, hilToSIM.pidCommand[THROTTLE]);
        break;
#endif

    case MSP_SENSOR_STATUS:
        return MSP_RESULT_ERROR;

    case MSP_ACTIVEBOXES:
        {
            boxBitmask_t mspBoxModeFlags;
            packBoxModeFlags(&mspBoxModeFlags);
            sbufWriteData(dst, &mspBoxModeFlags, sizeof(mspBoxModeFlags));
        }
        break;

    case MSP_STATUS_EX:
    case MSP_STATUS:
        {
            boxBitmask_t mspBoxModeFlags;
            packBoxModeFlags(&mspBoxModeFlags);

            sbufWriteU16(dst, (uint16_t)cycleTime);
#ifdef USE_I2C
            sbufWriteU16(dst, i2cGetErrorCounter());
#else
            sbufWriteU16(dst, 0);
#endif
            sbufWriteU16(dst, packSensorStatus());
            sbufWriteData(dst, &mspBoxModeFlags, 4);
            sbufWriteU8(dst, getConfigProfile());

            if (cmdMSP == MSP_STATUS_EX) {
                sbufWriteU16(dst, averageSystemLoadPercent);
                sbufWriteU16(dst, armingFlags);
                sbufWriteU8(dst, 0);
            }
        }
        break;

        case MSP2_INAV_STATUS:
        {
            // Preserves full arming flags and box modes
            boxBitmask_t mspBoxModeFlags;
            packBoxModeFlags(&mspBoxModeFlags);

            sbufWriteU16(dst, (uint16_t)cycleTime);
#ifdef USE_I2C
            sbufWriteU16(dst, i2cGetErrorCounter());
#else
            sbufWriteU16(dst, 0);
#endif
            sbufWriteU16(dst, packSensorStatus());
            sbufWriteU16(dst, averageSystemLoadPercent);
            sbufWriteU8(dst, 0);
            sbufWriteU32(dst, armingFlags);
            sbufWriteData(dst, &mspBoxModeFlags, sizeof(mspBoxModeFlags));
        }
        break;

    case MSP_RAW_IMU:
        return MSP_RESULT_ERROR;

#ifdef USE_SERVOS
    case MSP_SERVO:
        sbufWriteData(dst, &servo, MAX_SUPPORTED_SERVOS * 2);
        break;

    case MSP_RCOUT_NEUTRAL:
        return MSP_RESULT_ERROR;
//        break;

    case MSP_RCOUT_REVERSES:
        return MSP_RESULT_ERROR;
//        break;

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
        break;
    case MSP_SERVO_MIX_RULES:
        return MSP_RESULT_ERROR;
#endif

    case MSP2_COMMON_MOTOR_MIXER:
        return MSP_RESULT_ERROR;


    case MSP_MOTOR:
        return MSP_RESULT_ERROR;

    case MSP_RC:
        for (int i = 0; i < rxRuntimeConfig.channelCount; i++) {
            sbufWriteU16(dst, rcData[i]);
        }
        break;

    case MSP_ATTITUDE:
        return MSP_RESULT_ERROR;

    case MSP_ALTITUDE:
#if defined(USE_NAV)
        sbufWriteU32(dst, lrintf(getEstimatedActualPosition(Z)));
        sbufWriteU16(dst, lrintf(getEstimatedActualVelocity(Z)));
#else
        sbufWriteU32(dst, 0);
        sbufWriteU16(dst, 0);
#endif
#if defined(USE_BARO)
        sbufWriteU32(dst, baroGetLatestAltitude());
#else
        sbufWriteU32(dst, 0);
#endif
        break;

    case MSP_SONAR_ALTITUDE:
#ifdef USE_RANGEFINDER
        sbufWriteU32(dst, rangefinderGetLatestAltitude());
#else
        sbufWriteU32(dst, 0);
#endif
        break;

    case MSP2_INAV_OPTICAL_FLOW:
#ifdef USE_OPTICAL_FLOW
        sbufWriteU8(dst, opflow.rawQuality);
        sbufWriteU16(dst, RADIANS_TO_DEGREES(opflow.flowRate[X]));
        sbufWriteU16(dst, RADIANS_TO_DEGREES(opflow.flowRate[Y]));
        sbufWriteU16(dst, RADIANS_TO_DEGREES(opflow.bodyRate[X]));
        sbufWriteU16(dst, RADIANS_TO_DEGREES(opflow.bodyRate[Y]));
#else
        sbufWriteU8(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
#endif
        break;

    case MSP_ANALOG:
        return MSP_RESULT_ERROR;

    case MSP2_INAV_ANALOG:
        return MSP_RESULT_ERROR;

    case MSP_ARMING_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_LOOP_TIME:
        return MSP_RESULT_ERROR;

    case MSP_RC_TUNING:
        sbufWriteU8(dst, 100); //rcRate8 kept for compatibity reasons, this setting is no longer used
        sbufWriteU8(dst, currentControlRateProfile->stabilized.rcExpo8);
        for (int i = 0 ; i < 3; i++) {
            sbufWriteU8(dst, currentControlRateProfile->stabilized.rates[i]); // R,P,Y see flight_dynamics_index_t
        }
        sbufWriteU8(dst, currentControlRateProfile->throttle.dynPID);
        sbufWriteU8(dst, currentControlRateProfile->throttle.rcMid8);
        sbufWriteU8(dst, currentControlRateProfile->throttle.rcExpo8);
        sbufWriteU16(dst, currentControlRateProfile->throttle.pa_breakpoint);
        sbufWriteU8(dst, currentControlRateProfile->stabilized.rcYawExpo8);
        break;

    case MSP2_INAV_RATE_PROFILE:
        // throttle
        sbufWriteU8(dst, currentControlRateProfile->throttle.rcMid8);
        sbufWriteU8(dst, currentControlRateProfile->throttle.rcExpo8);
        sbufWriteU8(dst, currentControlRateProfile->throttle.dynPID);
        sbufWriteU16(dst, currentControlRateProfile->throttle.pa_breakpoint);

        // stabilized
        sbufWriteU8(dst, currentControlRateProfile->stabilized.rcExpo8);
        sbufWriteU8(dst, currentControlRateProfile->stabilized.rcYawExpo8);
        for (uint8_t i = 0 ; i < 3; ++i) {
            sbufWriteU8(dst, currentControlRateProfile->stabilized.rates[i]); // R,P,Y see flight_dynamics_index_t
        }

        // manual
        sbufWriteU8(dst, currentControlRateProfile->manual.rcExpo8);
        sbufWriteU8(dst, currentControlRateProfile->manual.rcYawExpo8);
        for (uint8_t i = 0 ; i < 3; ++i) {
            sbufWriteU8(dst, currentControlRateProfile->manual.rates[i]); // R,P,Y see flight_dynamics_index_t
        }
        break;

    case MSP_PID:
        return MSP_RESULT_ERROR;

    case MSP_PIDNAMES:
        for (const char *c = pidnames; *c; c++) {
            sbufWriteU8(dst, *c);
        }
        break;

    case MSP_PID_CONTROLLER:
        sbufWriteU8(dst, 2);      // FIXME: Report as LuxFloat
        break;

    case MSP_MODE_RANGES:
        for (int i = 0; i < MAX_MODE_ACTIVATION_CONDITION_COUNT; i++) {
            const modeActivationCondition_t *mac = modeActivationConditions(i);
            const box_t *box = findBoxByActiveBoxId(mac->modeId);
            sbufWriteU8(dst, box ? box->permanentId : 0);
            sbufWriteU8(dst, mac->auxChannelIndex);
            sbufWriteU8(dst, mac->range.startStep);
            sbufWriteU8(dst, mac->range.endStep);
        }
        break;

    case MSP_ADJUSTMENT_RANGES:
        for (int i = 0; i < MAX_ADJUSTMENT_RANGE_COUNT; i++) {
            const adjustmentRange_t *adjRange = adjustmentRanges(i);
            sbufWriteU8(dst, adjRange->adjustmentIndex);
            sbufWriteU8(dst, adjRange->auxChannelIndex);
            sbufWriteU8(dst, adjRange->range.startStep);
            sbufWriteU8(dst, adjRange->range.endStep);
            sbufWriteU8(dst, adjRange->adjustmentFunction);
            sbufWriteU8(dst, adjRange->auxSwitchChannelIndex);
        }
        break;

    case MSP_BOXNAMES:
        if (!serializeBoxNamesReply(dst)) {
            return false;
        }
        break;

    case MSP_BOXIDS:
        serializeBoxReply(dst);
        break;

    case MSP_MISC:
        return MSP_RESULT_ERROR;

    case MSP2_INAV_MISC:
        return MSP_RESULT_ERROR;

    case MSP2_INAV_BATTERY_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_MOTOR_PINS:
        // FIXME This is hardcoded and should not be.
        for (int i = 0; i < 8; i++) {
            sbufWriteU8(dst, i + 1);
        }
        break;

#ifdef USE_GPS
    case MSP_RAW_GPS:
        sbufWriteU8(dst, gpsSol.fixType);
        sbufWriteU8(dst, gpsSol.numSat);
        sbufWriteU32(dst, gpsSol.llh.lat);
        sbufWriteU32(dst, gpsSol.llh.lon);
        sbufWriteU16(dst, gpsSol.llh.alt/100); // meters
        sbufWriteU16(dst, gpsSol.groundSpeed);
        sbufWriteU16(dst, gpsSol.groundCourse);
        sbufWriteU16(dst, gpsSol.hdop);
        break;

    case MSP_COMP_GPS:
        sbufWriteU16(dst, GPS_distanceToHome);
        sbufWriteU16(dst, GPS_directionToHome);
        sbufWriteU8(dst, gpsSol.flags.gpsHeartbeat ? 1 : 0);
        break;
#ifdef USE_NAV
    case MSP_NAV_STATUS:
        sbufWriteU8(dst, NAV_Status.mode);
        sbufWriteU8(dst, NAV_Status.state);
        sbufWriteU8(dst, NAV_Status.activeWpAction);
        sbufWriteU8(dst, NAV_Status.activeWpNumber);
        sbufWriteU8(dst, NAV_Status.error);
        //sbufWriteU16(dst,  (int16_t)(target_bearing/100));
        sbufWriteU16(dst, getHeadingHoldTarget());
        break;
#endif

    case MSP_GPSSVINFO:
        /* Compatibility stub - return zero SVs */
        sbufWriteU8(dst, 1);

        // HDOP
        sbufWriteU8(dst, 0);
        sbufWriteU8(dst, 0);
        sbufWriteU8(dst, gpsSol.hdop / 100);
        sbufWriteU8(dst, gpsSol.hdop / 100);
        break;

    case MSP_GPSSTATISTICS:
        sbufWriteU16(dst, gpsStats.lastMessageDt);
        sbufWriteU32(dst, gpsStats.errors);
        sbufWriteU32(dst, gpsStats.timeouts);
        sbufWriteU32(dst, gpsStats.packetCount);
        sbufWriteU16(dst, gpsSol.hdop);
        sbufWriteU16(dst, gpsSol.eph);
        sbufWriteU16(dst, gpsSol.epv);
        break;
#endif
    case MSP_DEBUG:
        // output some useful QA statistics
        // debug[x] = ((hse_value / 1000000) * 1000) + (SystemCoreClock / 1000000);         // XX0YY [crystal clock : core clock]

        for (int i = 0; i < DEBUG16_VALUE_COUNT; i++) {
            sbufWriteU16(dst, debug[i]);      // 4 variables are here for general monitoring purpose
        }
        break;

    case MSP_UID:
        sbufWriteU32(dst, U_ID_0);
        sbufWriteU32(dst, U_ID_1);
        sbufWriteU32(dst, U_ID_2);
        break;

    case MSP_FEATURE:
        sbufWriteU32(dst, featureMask());
        break;

    case MSP_BOARD_ALIGNMENT:
        return MSP_RESULT_ERROR;

    case MSP_VOLTAGE_METER_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_CURRENT_METER_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_MIXER:
        return MSP_RESULT_ERROR;

    case MSP_RX_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_FAILSAFE_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_RSSI_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_RX_MAP:
        return MSP_RESULT_ERROR;

    case MSP_BF_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_CF_SERIAL_CONFIG:
        for (int i = 0; i < SERIAL_PORT_COUNT; i++) {
            if (!serialIsPortAvailable(serialConfig()->portConfigs[i].identifier)) {
                continue;
            };
            sbufWriteU8(dst, serialConfig()->portConfigs[i].identifier);
            sbufWriteU16(dst, serialConfig()->portConfigs[i].functionMask);
            sbufWriteU8(dst, serialConfig()->portConfigs[i].msp_baudrateIndex);
            sbufWriteU8(dst, serialConfig()->portConfigs[i].gps_baudrateIndex);
            sbufWriteU8(dst, serialConfig()->portConfigs[i].telemetry_baudrateIndex);
            sbufWriteU8(dst, serialConfig()->portConfigs[i].peripheral_baudrateIndex);
        }
        break;

#ifdef USE_LED_STRIP
    case MSP_LED_COLORS:
        for (int i = 0; i < LED_CONFIGURABLE_COLOR_COUNT; i++) {
            const hsvColor_t *color = &ledStripConfig()->colors[i];
            sbufWriteU16(dst, color->h);
            sbufWriteU8(dst, color->s);
            sbufWriteU8(dst, color->v);
        }
        break;

    case MSP_LED_STRIP_CONFIG:
        for (int i = 0; i < LED_MAX_STRIP_LENGTH; i++) {
            const ledConfig_t *ledConfig = &ledStripConfig()->ledConfigs[i];
            sbufWriteU32(dst, *ledConfig);
        }
        break;

    case MSP_LED_STRIP_MODECOLOR:
        for (int i = 0; i < LED_MODE_COUNT; i++) {
            for (int j = 0; j < LED_DIRECTION_COUNT; j++) {
                sbufWriteU8(dst, i);
                sbufWriteU8(dst, j);
                sbufWriteU8(dst, ledStripConfig()->modeColors[i].color[j]);
            }
        }

        for (int j = 0; j < LED_SPECIAL_COLOR_COUNT; j++) {
            sbufWriteU8(dst, LED_MODE_COUNT);
            sbufWriteU8(dst, j);
            sbufWriteU8(dst, ledStripConfig()->specialColors.color[j]);
        }
        break;
#endif

    case MSP_DATAFLASH_SUMMARY:
        serializeDataflashSummaryReply(dst);
        break;

    case MSP_BLACKBOX_CONFIG:
#ifdef USE_BLACKBOX
        sbufWriteU8(dst, 1); //Blackbox supported
        sbufWriteU8(dst, blackboxConfig()->device);
        sbufWriteU8(dst, blackboxConfig()->rate_num);
        sbufWriteU8(dst, blackboxConfig()->rate_denom);
#else
        sbufWriteU8(dst, 0); // Blackbox not supported
        sbufWriteU8(dst, 0);
        sbufWriteU8(dst, 0);
        sbufWriteU8(dst, 0);
#endif
        break;

    case MSP_SDCARD_SUMMARY:
        serializeSDCardSummaryReply(dst);
        break;

    case MSP_OSD_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_BF_BUILD_INFO:
        sbufWriteData(dst, buildDate, 11); // MMM DD YYYY as ascii, MMM = Jan/Feb... etc
        sbufWriteU32(dst, 0); // future exp
        sbufWriteU32(dst, 0); // future exp
        break;

    case MSP_3D:
        return MSP_RESULT_ERROR;

    case MSP_RC_DEADBAND:
        return MSP_RESULT_ERROR;

    case MSP_SENSOR_ALIGNMENT:
        return MSP_RESULT_ERROR;

    case MSP_ADVANCED_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_FILTER_CONFIG :
        return MSP_RESULT_ERROR;

    case MSP_PID_ADVANCED:
        return MSP_RESULT_ERROR;

    case MSP_INAV_PID:
        return MSP_RESULT_ERROR;

    case MSP_SENSOR_CONFIG:
        return MSP_RESULT_ERROR;

#ifdef USE_NAV
    case MSP_NAV_POSHOLD:
        sbufWriteU8(dst, navConfig()->general.flags.user_control_mode);
        sbufWriteU16(dst, navConfig()->general.max_auto_speed);
        sbufWriteU16(dst, navConfig()->general.max_auto_climb_rate);
        sbufWriteU16(dst, navConfig()->general.max_manual_speed);
        sbufWriteU16(dst, navConfig()->general.max_manual_climb_rate);
        sbufWriteU8(dst, navConfig()->mc.max_bank_angle);
        sbufWriteU8(dst, navConfig()->general.flags.use_thr_mid_for_althold);
        sbufWriteU16(dst, navConfig()->mc.hover_throttle);
        break;

    case MSP_RTH_AND_LAND_CONFIG:
        sbufWriteU16(dst, navConfig()->general.min_rth_distance);
        sbufWriteU8(dst, navConfig()->general.flags.rth_climb_first);
        sbufWriteU8(dst, navConfig()->general.flags.rth_climb_ignore_emerg);
        sbufWriteU8(dst, navConfig()->general.flags.rth_tail_first);
        sbufWriteU8(dst, navConfig()->general.flags.rth_allow_landing);
        sbufWriteU8(dst, navConfig()->general.flags.rth_alt_control_mode);
        sbufWriteU16(dst, navConfig()->general.rth_abort_threshold);
        sbufWriteU16(dst, navConfig()->general.rth_altitude);
        sbufWriteU16(dst, navConfig()->general.land_descent_rate);
        sbufWriteU16(dst, navConfig()->general.land_slowdown_minalt);
        sbufWriteU16(dst, navConfig()->general.land_slowdown_maxalt);
        sbufWriteU16(dst, navConfig()->general.emerg_descent_rate);
        break;

    case MSP_FW_CONFIG:
        sbufWriteU16(dst, navConfig()->fw.cruise_throttle);
        sbufWriteU16(dst, navConfig()->fw.min_throttle);
        sbufWriteU16(dst, navConfig()->fw.max_throttle);
        sbufWriteU8(dst, navConfig()->fw.max_bank_angle);
        sbufWriteU8(dst, navConfig()->fw.max_climb_angle);
        sbufWriteU8(dst, navConfig()->fw.max_dive_angle);
        sbufWriteU8(dst, navConfig()->fw.pitch_to_throttle);
        sbufWriteU16(dst, navConfig()->fw.loiter_radius);
        break;
#endif

    case MSP_CALIBRATION_DATA:
        sbufWriteU8(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);

        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        break;

    case MSP_POSITION_ESTIMATION_CONFIG:
    #ifdef USE_NAV

        sbufWriteU16(dst, positionEstimationConfig()->w_z_baro_p * 100); //     inav_w_z_baro_p float as value * 100
        sbufWriteU16(dst, positionEstimationConfig()->w_z_gps_p * 100);  // 2   inav_w_z_gps_p  float as value * 100
        sbufWriteU16(dst, positionEstimationConfig()->w_z_gps_v * 100);  // 2   inav_w_z_gps_v  float as value * 100
        sbufWriteU16(dst, positionEstimationConfig()->w_xy_gps_p * 100); // 2   inav_w_xy_gps_p float as value * 100
        sbufWriteU16(dst, positionEstimationConfig()->w_xy_gps_v * 100); // 2   inav_w_xy_gps_v float as value * 100
        sbufWriteU8(dst, gpsConfigMutable()->gpsMinSats);                // 1
        sbufWriteU8(dst, positionEstimationConfig()->use_gps_velned);    // 1   inav_use_gps_velned ON/OFF

    #else
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU16(dst, 0);
        sbufWriteU8(dst, 0);
        sbufWriteU8(dst, 0);
    #endif
        break;

    case MSP_REBOOT:
        if (!ARMING_FLAG(ARMED)) {
            if (mspPostProcessFn) {
                *mspPostProcessFn = mspRebootFn;
            }
        }
        break;

    case MSP_WP_GETINFO:
        return MSP_RESULT_ERROR;

    case MSP_TX_INFO:
        return MSP_RESULT_ERROR;

    case MSP_RTC:
        {
            int32_t seconds = 0;
            uint16_t millis = 0;
            rtcTime_t t;
            if (rtcGet(&t)) {
                seconds = rtcTimeGetSeconds(&t);
                millis = rtcTimeGetMillis(&t);
            }
            sbufWriteU32(dst, (uint32_t)seconds);
            sbufWriteU16(dst, millis);
        }
        break;

#if defined(VTX_COMMON)
    case MSP_VTX_CONFIG:
        {
            uint8_t deviceType = vtxCommonGetDeviceType();
            if (deviceType != VTXDEV_UNKNOWN) {

                uint8_t band=0, channel=0;
                vtxCommonGetBandAndChannel(&band,&channel);

                uint8_t powerIdx=0; // debug
                vtxCommonGetPowerIndex(&powerIdx);

                uint8_t pitmode=0;
                vtxCommonGetPitMode(&pitmode);

                sbufWriteU8(dst, deviceType);
                sbufWriteU8(dst, band);
                sbufWriteU8(dst, channel);
                sbufWriteU8(dst, powerIdx);
                sbufWriteU8(dst, pitmode);
                // future extensions here...
            }
            else {
                sbufWriteU8(dst, VTXDEV_UNKNOWN); // no VTX detected
            }
        }
        break;
#endif

    case MSP_NAME:
        {
            const char *name = systemConfig()->name;
            while (*name) {
                sbufWriteU8(dst, *name++);
            }
        }
        break;

    case MSP2_COMMON_TZ:
        sbufWriteU16(dst, (uint16_t)timeConfig()->tz_offset);
        break;

    case MSP2_INAV_AIR_SPEED:
        return MSP_RESULT_ERROR;

    default:
        return false;
    }
    return true;
}

#ifdef USE_NAV
static void mspFcWaypointOutCommand(sbuf_t *dst, sbuf_t *src)
{
    const uint8_t msp_wp_no = sbufReadU8(src);    // get the wp number
    navWaypoint_t msp_wp;
    getWaypoint(msp_wp_no, &msp_wp);
    sbufWriteU8(dst, msp_wp_no);   // wp_no
    sbufWriteU8(dst, msp_wp.action);  // action (WAYPOINT)
    sbufWriteU32(dst, msp_wp.lat);    // lat
    sbufWriteU32(dst, msp_wp.lon);    // lon
    sbufWriteU32(dst, msp_wp.alt);    // altitude (cm)
    sbufWriteU16(dst, msp_wp.p1);     // P1
    sbufWriteU16(dst, msp_wp.p2);     // P2
    sbufWriteU16(dst, msp_wp.p3);     // P3
    sbufWriteU8(dst, msp_wp.flag);    // flags
}
#endif

#ifdef USE_FLASHFS
static void mspFcDataFlashReadCommand(sbuf_t *dst, sbuf_t *src)
{
    const unsigned int dataSize = sbufBytesRemaining(src);
    uint16_t readLength;

    const uint32_t readAddress = sbufReadU32(src);

    // Request payload:
    //  uint32_t    - address to read from
    //  uint16_t    - size of block to read (optional)
    if (dataSize >= sizeof(uint32_t) + sizeof(uint16_t)) {
        readLength = sbufReadU16(src);
    }
    else {
        readLength = 128;
    }

    serializeDataflashReadReply(dst, readAddress, readLength);
}
#endif

static mspResult_e mspFcProcessInCommand(uint16_t cmdMSP, sbuf_t *src)
{
    uint8_t tmp_u8;
    uint16_t tmp_u16;

    const unsigned int dataSize = sbufBytesRemaining(src);

    switch (cmdMSP) {
    case MSP_SELECT_SETTING:
        if (sbufReadU8Safe(&tmp_u8, src) && (!ARMING_FLAG(ARMED)))
            setConfigProfileAndWriteEEPROM(tmp_u8);
        else
            return MSP_RESULT_ERROR;
        break;

    case MSP_SET_HEAD:
        return MSP_RESULT_ERROR;

#ifdef USE_RX_MSP
    case MSP_SET_RAW_RC:
        {
            uint8_t channelCount = dataSize / sizeof(uint16_t);
            if ((channelCount > MAX_SUPPORTED_RC_CHANNEL_COUNT) || (dataSize > channelCount * sizeof(uint16_t))) {
                return MSP_RESULT_ERROR;
            } else {
                uint16_t frame[MAX_SUPPORTED_RC_CHANNEL_COUNT];
                for (int i = 0; i < channelCount; i++) {
                    frame[i] = sbufReadU16(src);
                }
// NOTE: It can be usefull for settings a data                
                // rxMspFrameReceive(frame, channelCount);
            }
        }
        break;
#endif

    case MSP_SET_ARMING_CONFIG:
        if (dataSize >= 2) {
            armingConfigMutable()->auto_disarm_delay = constrain(sbufReadU8(src), AUTO_DISARM_DELAY_MIN, AUTO_DISARM_DELAY_MAX);
            armingConfigMutable()->disarm_kill_switch = !!sbufReadU8(src);
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP_SET_LOOP_TIME:
        return MSP_RESULT_ERROR;

    case MSP_SET_PID_CONTROLLER:
        // FIXME: Do nothing
        break;

    case MSP_SET_PID:
        return MSP_RESULT_ERROR;

    case MSP_SET_MODE_RANGE:
        sbufReadU8Safe(&tmp_u8, src);
        if ((dataSize >= 5) && (tmp_u8 < MAX_MODE_ACTIVATION_CONDITION_COUNT)) {
            modeActivationCondition_t *mac = modeActivationConditionsMutable(tmp_u8);
            tmp_u8 = sbufReadU8(src);
            const box_t *box = findBoxByPermanentId(tmp_u8);
            if (box) {
                mac->modeId = box->boxId;
                mac->auxChannelIndex = sbufReadU8(src);
                mac->range.startStep = sbufReadU8(src);
                mac->range.endStep = sbufReadU8(src);

                updateUsedModeActivationConditionFlags();
            } else {
                return MSP_RESULT_ERROR;
            }
        } else {
            return MSP_RESULT_ERROR;
        }
        break;

    case MSP_SET_ADJUSTMENT_RANGE:
        sbufReadU8Safe(&tmp_u8, src);
        if ((dataSize >= 7) && (tmp_u8 < MAX_ADJUSTMENT_RANGE_COUNT)) {
            adjustmentRange_t *adjRange = adjustmentRangesMutable(tmp_u8);
            tmp_u8 = sbufReadU8(src);
            if (tmp_u8 < MAX_SIMULTANEOUS_ADJUSTMENT_COUNT) {
                adjRange->adjustmentIndex = tmp_u8;
                adjRange->auxChannelIndex = sbufReadU8(src);
                adjRange->range.startStep = sbufReadU8(src);
                adjRange->range.endStep = sbufReadU8(src);
                adjRange->adjustmentFunction = sbufReadU8(src);
                adjRange->auxSwitchChannelIndex = sbufReadU8(src);
            } else {
                return MSP_RESULT_ERROR;
            }
        } else {
            return MSP_RESULT_ERROR;
        }
        break;

    case MSP_SET_RC_TUNING:
        return MSP_RESULT_ERROR;

    case MSP2_INAV_SET_RATE_PROFILE:
        return MSP_RESULT_ERROR;

    case MSP_SET_MISC:
        return MSP_RESULT_ERROR;

    case MSP2_INAV_SET_MISC:
        return MSP_RESULT_ERROR;

    case MSP2_INAV_SET_BATTERY_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_SET_MOTOR:
        return MSP_RESULT_ERROR;

#ifdef USE_SERVOS
    case MSP_SET_SERVO_CONFIGURATION:
        if (dataSize != (1 + 14)) {
            return MSP_RESULT_ERROR;
        }
        tmp_u8 = sbufReadU8(src);
        if (tmp_u8 >= MAX_SUPPORTED_SERVOS) {
            return MSP_RESULT_ERROR;
        } else {
            servoParamsMutable(tmp_u8)->min = sbufReadU16(src);
            servoParamsMutable(tmp_u8)->max = sbufReadU16(src);
            servoParamsMutable(tmp_u8)->middle = sbufReadU16(src);
            servoParamsMutable(tmp_u8)->rate = sbufReadU8(src);
            sbufReadU8(src);
            sbufReadU8(src);
            servoParamsMutable(tmp_u8)->forwardFromChannel = sbufReadU8(src);
            servoParamsMutable(tmp_u8)->reversedSources = sbufReadU32(src);
            servoComputeScalingFactors(tmp_u8);
        }
        break;
#endif

#ifdef USE_SERVOS
    case MSP_SET_SERVO_MIX_RULE:
        return MSP_RESULT_ERROR;
#endif

    case MSP2_COMMON_SET_MOTOR_MIXER:
        return MSP_RESULT_ERROR;

    case MSP_SET_3D:
        return MSP_RESULT_ERROR;

    case MSP_SET_RC_DEADBAND:
        if (dataSize >= 5) {
            rcControlsConfigMutable()->deadband = sbufReadU8(src);
            rcControlsConfigMutable()->yaw_deadband = sbufReadU8(src);
            rcControlsConfigMutable()->alt_hold_deadband = sbufReadU8(src);
            rcControlsConfigMutable()->deadband3d_throttle = sbufReadU16(src);
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP_SET_RESET_CURR_PID:
        return MSP_RESULT_ERROR;
        
    case MSP_SET_SENSOR_ALIGNMENT:
        return MSP_RESULT_ERROR;

    case MSP_SET_ADVANCED_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_SET_FILTER_CONFIG :
         return MSP_RESULT_ERROR;
    case MSP_SET_PID_ADVANCED:
         return MSP_RESULT_ERROR;

    case MSP_SET_INAV_PID:
        return MSP_RESULT_ERROR;

    case MSP_SET_SENSOR_CONFIG:
        return MSP_RESULT_ERROR;

#ifdef USE_NAV
    case MSP_SET_NAV_POSHOLD:
        if (dataSize >= 13) {
            navConfigMutable()->general.flags.user_control_mode = sbufReadU8(src);
            navConfigMutable()->general.max_auto_speed = sbufReadU16(src);
            navConfigMutable()->general.max_auto_climb_rate = sbufReadU16(src);
            navConfigMutable()->general.max_manual_speed = sbufReadU16(src);
            navConfigMutable()->general.max_manual_climb_rate = sbufReadU16(src);
            navConfigMutable()->mc.max_bank_angle = sbufReadU8(src);
            navConfigMutable()->general.flags.use_thr_mid_for_althold = sbufReadU8(src);
            navConfigMutable()->mc.hover_throttle = sbufReadU16(src);
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP_SET_RTH_AND_LAND_CONFIG:
        if (dataSize >= 19) {
            navConfigMutable()->general.min_rth_distance = sbufReadU16(src);
            navConfigMutable()->general.flags.rth_climb_first = sbufReadU8(src);
            navConfigMutable()->general.flags.rth_climb_ignore_emerg = sbufReadU8(src);
            navConfigMutable()->general.flags.rth_tail_first = sbufReadU8(src);
            navConfigMutable()->general.flags.rth_allow_landing = sbufReadU8(src);
            navConfigMutable()->general.flags.rth_alt_control_mode = sbufReadU8(src);
            navConfigMutable()->general.rth_abort_threshold = sbufReadU16(src);
            navConfigMutable()->general.rth_altitude = sbufReadU16(src);
            navConfigMutable()->general.land_descent_rate = sbufReadU16(src);
            navConfigMutable()->general.land_slowdown_minalt = sbufReadU16(src);
            navConfigMutable()->general.land_slowdown_maxalt = sbufReadU16(src);
            navConfigMutable()->general.emerg_descent_rate = sbufReadU16(src);
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP_SET_FW_CONFIG:
        if (dataSize >= 12) {
            navConfigMutable()->fw.cruise_throttle = sbufReadU16(src);
            navConfigMutable()->fw.min_throttle = sbufReadU16(src);
            navConfigMutable()->fw.max_throttle = sbufReadU16(src);
            navConfigMutable()->fw.max_bank_angle = sbufReadU8(src);
            navConfigMutable()->fw.max_climb_angle = sbufReadU8(src);
            navConfigMutable()->fw.max_dive_angle = sbufReadU8(src);
            navConfigMutable()->fw.pitch_to_throttle = sbufReadU8(src);
            navConfigMutable()->fw.loiter_radius = sbufReadU16(src);
        } else
            return MSP_RESULT_ERROR;
        break;
#endif

    case MSP_SET_CALIBRATION_DATA:
        return MSP_RESULT_ERROR;

#ifdef USE_NAV
    case MSP_SET_POSITION_ESTIMATION_CONFIG:
        if (dataSize >= 12) {
            positionEstimationConfigMutable()->w_z_baro_p = constrainf(sbufReadU16(src) / 100.0f, 0.0f, 10.0f);
            positionEstimationConfigMutable()->w_z_gps_p = constrainf(sbufReadU16(src) / 100.0f, 0.0f, 10.0f);
            positionEstimationConfigMutable()->w_z_gps_v = constrainf(sbufReadU16(src) / 100.0f, 0.0f, 10.0f);
            positionEstimationConfigMutable()->w_xy_gps_p = constrainf(sbufReadU16(src) / 100.0f, 0.0f, 10.0f);
            positionEstimationConfigMutable()->w_xy_gps_v = constrainf(sbufReadU16(src) / 100.0f, 0.0f, 10.0f);
            gpsConfigMutable()->gpsMinSats = constrain(sbufReadU8(src), 5, 10);
            positionEstimationConfigMutable()->use_gps_velned = constrain(sbufReadU8(src), 0, 1);
        } else
            return MSP_RESULT_ERROR;
        break;
#endif

    case MSP_RESET_CONF:
        if (!ARMING_FLAG(ARMED)) {
            resetEEPROM();
            readEEPROM();
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP_ACC_CALIBRATION:
        return MSP_RESULT_ERROR;

    case MSP_MAG_CALIBRATION:
        return MSP_RESULT_ERROR;

    case MSP_EEPROM_WRITE:
        if (!ARMING_FLAG(ARMED)) {
            writeEEPROM();
            readEEPROM();
        } else
            return MSP_RESULT_ERROR;
        break;

#ifdef USE_BLACKBOX
    case MSP_SET_BLACKBOX_CONFIG:
        // Don't allow config to be updated while Blackbox is logging
        if ((dataSize >= 3) && blackboxMayEditConfig()) {
            blackboxConfigMutable()->device = sbufReadU8(src);
            blackboxConfigMutable()->rate_num = sbufReadU8(src);
            blackboxConfigMutable()->rate_denom = sbufReadU8(src);
        } else
            return MSP_RESULT_ERROR;
        break;
#endif


#if defined(VTX_COMMON)
    case MSP_SET_VTX_CONFIG:
        if (dataSize >= 4) {
            tmp_u16 = sbufReadU16(src);
            const uint8_t band    = (tmp_u16 / 8) + 1;
            const uint8_t channel = (tmp_u16 % 8) + 1;

            if (vtxCommonGetDeviceType() != VTXDEV_UNKNOWN) {
                uint8_t current_band=0, current_channel=0;
                vtxCommonGetBandAndChannel(&current_band,&current_channel);
                if ((current_band != band) || (current_channel != channel))
                    vtxCommonSetBandAndChannel(band,channel);

                if (sbufBytesRemaining(src) < 2)
                    break;

                uint8_t power = sbufReadU8(src);
                uint8_t current_power = 0;
                vtxCommonGetPowerIndex(&current_power);
                if (current_power != power)
                    vtxCommonSetPowerByIndex(power);

                uint8_t pitmode = sbufReadU8(src);
                uint8_t current_pitmode = 0;
                vtxCommonGetPitMode(&current_pitmode);
                if (current_pitmode != pitmode)
                    vtxCommonSetPitMode(pitmode);
            }
        } else
            return MSP_RESULT_ERROR;
        break;
#endif

#ifdef USE_FLASHFS
    case MSP_DATAFLASH_ERASE:
        flashfsEraseCompletely();
        break;
#endif

#ifdef USE_GPS
    case MSP_SET_RAW_GPS:
        return MSP_RESULT_ERROR;
#endif

#ifdef USE_NAV
    case MSP_SET_WP:
        if (dataSize >= 21) {
            const uint8_t msp_wp_no = sbufReadU8(src);     // get the waypoint number
            navWaypoint_t msp_wp;
            msp_wp.action = sbufReadU8(src);    // action
            msp_wp.lat = sbufReadU32(src);      // lat
            msp_wp.lon = sbufReadU32(src);      // lon
            msp_wp.alt = sbufReadU32(src);      // to set altitude (cm)
            msp_wp.p1 = sbufReadU16(src);       // P1
            msp_wp.p2 = sbufReadU16(src);       // P2
            msp_wp.p3 = sbufReadU16(src);       // P3
            msp_wp.flag = sbufReadU8(src);      // future: to set nav flag
//            setWaypoint(msp_wp_no, &msp_wp);
        } else
            return MSP_RESULT_ERROR;
        break;
#endif

    case MSP_SET_FEATURE:
        return MSP_RESULT_ERROR;

    case MSP_SET_BOARD_ALIGNMENT:
        return MSP_RESULT_ERROR;

    case MSP_SET_VOLTAGE_METER_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_SET_CURRENT_METER_CONFIG:
        return MSP_RESULT_ERROR;

#ifndef USE_QUAD_MIXER_ONLY
    case MSP_SET_MIXER:
        return MSP_RESULT_ERROR;
#endif

    case MSP_SET_RX_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_SET_FAILSAFE_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_SET_RSSI_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_SET_RX_MAP:
        return MSP_RESULT_ERROR;

    case MSP_SET_BF_CONFIG:
        return MSP_RESULT_ERROR;

    case MSP_SET_CF_SERIAL_CONFIG:
        {
            uint8_t portConfigSize = sizeof(uint8_t) + sizeof(uint16_t) + (sizeof(uint8_t) * 4);

            if (dataSize % portConfigSize != 0) {
                return MSP_RESULT_ERROR;
            }

            uint8_t remainingPortsInPacket = dataSize / portConfigSize;

            while (remainingPortsInPacket--) {
                uint8_t identifier = sbufReadU8(src);

                serialPortConfig_t *portConfig = serialFindPortConfiguration(identifier);
                if (!portConfig) {
                    return MSP_RESULT_ERROR;
                }

                portConfig->identifier = identifier;
                portConfig->functionMask = sbufReadU16(src);
                portConfig->msp_baudrateIndex = sbufReadU8(src);
                portConfig->gps_baudrateIndex = sbufReadU8(src);
                portConfig->telemetry_baudrateIndex = sbufReadU8(src);
                portConfig->peripheral_baudrateIndex = sbufReadU8(src);
            }
        }
        break;

#ifdef USE_LED_STRIP
    case MSP_SET_LED_COLORS:
        if (dataSize >= LED_CONFIGURABLE_COLOR_COUNT * 4) {
            for (int i = 0; i < LED_CONFIGURABLE_COLOR_COUNT; i++) {
                hsvColor_t *color = &ledStripConfigMutable()->colors[i];
                color->h = sbufReadU16(src);
                color->s = sbufReadU8(src);
                color->v = sbufReadU8(src);
            }
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP_SET_LED_STRIP_CONFIG:
        if (dataSize >= 5) {
            tmp_u8 = sbufReadU8(src);
            if (tmp_u8 >= LED_MAX_STRIP_LENGTH || dataSize != (1 + 4)) {
                return MSP_RESULT_ERROR;
            }
            ledConfig_t *ledConfig = &ledStripConfigMutable()->ledConfigs[tmp_u8];
            *ledConfig = sbufReadU32(src);
            reevaluateLedConfig();
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP_SET_LED_STRIP_MODECOLOR:
        if (dataSize >= 3) {
            ledModeIndex_e modeIdx = sbufReadU8(src);
            int funIdx = sbufReadU8(src);
            int color = sbufReadU8(src);

            if (!setModeColor(modeIdx, funIdx, color))
                return MSP_RESULT_ERROR;
        } else
            return MSP_RESULT_ERROR;
        break;
#endif

#ifdef NAV_NON_VOLATILE_WAYPOINT_STORAGE
    case MSP_WP_MISSION_LOAD:
        sbufReadU8Safe(NULL, src);    // Mission ID (reserved)
        if ((dataSize != 1) || (!loadNonVolatileWaypointList()))
            return MSP_RESULT_ERROR;
        break;

    case MSP_WP_MISSION_SAVE:
        sbufReadU8Safe(NULL, src);    // Mission ID (reserved)
        if ((dataSize != 1) || (!saveNonVolatileWaypointList()))
            return MSP_RESULT_ERROR;
        break;
#endif

    case MSP_SET_RTC:
        if (dataSize >= 6) {
            // Use seconds and milliseconds to make senders
            // easier to implement. Generating a 64 bit value
            // might not be trivial in some platforms.
            int32_t secs = (int32_t)sbufReadU32(src);
            uint16_t millis = sbufReadU16(src);
            rtcTime_t t = rtcTimeMake(secs, millis);
            rtcSet(&t);
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP_SET_TX_INFO:
        return MSP_RESULT_ERROR;

    case MSP_SET_NAME:
        if (dataSize <= MAX_NAME_LENGTH) {
            char *name = systemConfigMutable()->name;
            int len = MIN(MAX_NAME_LENGTH, (int)dataSize);
            sbufReadData(src, name, len);
            memset(&name[len], '\0', (MAX_NAME_LENGTH + 1) - len);
        } else
            return MSP_RESULT_ERROR;
        break;

    case MSP2_COMMON_SET_TZ:
        if (dataSize == 2)
            timeConfigMutable()->tz_offset = (int16_t)sbufReadU16(src);
        else
            return MSP_RESULT_ERROR;
        break;

    default:
        return MSP_RESULT_ERROR;
    }
    return MSP_RESULT_ACK;
}

static const setting_t *mspReadSettingName(sbuf_t *src)
{
    char name[SETTING_MAX_NAME_LENGTH];
    uint8_t c;
    size_t s = 0;
    while (1) {
        if (!sbufReadU8Safe(&c, src)) {
            return NULL;
        }
        name[s++] = c;
        if (c == '\0') {
            break;
        }
        if (s == SETTING_MAX_NAME_LENGTH) {
            // Name is too long
            return NULL;
        }
    }
    return setting_find(name);
}

static bool mspSettingCommand(sbuf_t *dst, sbuf_t *src)
{
    const setting_t *setting = mspReadSettingName(src);
    if (!setting) {
        return false;
    }

    const void *ptr = setting_get_value_pointer(setting);
    size_t size = setting_get_value_size(setting);
    sbufWriteDataSafe(dst, ptr, size);
    return true;
}

static bool mspSetSettingCommand(sbuf_t *dst, sbuf_t *src)
{
    UNUSED(dst);

    const setting_t *setting = mspReadSettingName(src);
    if (!setting) {
        return false;
    }

    setting_min_t min = setting_get_min(setting);
    setting_max_t max = setting_get_max(setting);

    void *ptr = setting_get_value_pointer(setting);
    switch (SETTING_TYPE(setting)) {
        case VAR_UINT8:
            {
                uint8_t val;
                if (!sbufReadU8Safe(&val, src)) {
                    return false;
                }
                if (val > max) {
                    return false;
                }
                *((uint8_t*)ptr) = val;
            }
            break;
        case VAR_INT8:
            {
                int8_t val;
                if (!sbufReadI8Safe(&val, src)) {
                    return false;
                }
                if (val < min || val > (int8_t)max) {
                    return false;
                }
                *((int8_t*)ptr) = val;
            }
            break;
        case VAR_UINT16:
            {
                uint16_t val;
                if (!sbufReadU16Safe(&val, src)) {
                    return false;
                }
                if (val > max) {
                    return false;
                }
                *((uint16_t*)ptr) = val;
            }
            break;
        case VAR_INT16:
            {
                int16_t val;
                if (!sbufReadI16Safe(&val, src)) {
                    return false;
                }
                if (val < min || val > (int16_t)max) {
                    return false;
                }
                *((int16_t*)ptr) = val;
            }
            break;
        case VAR_UINT32:
            {
                uint32_t val;
                if (!sbufReadU32Safe(&val, src)) {
                    return false;
                }
                if (val > max) {
                    return false;
                }
                *((uint32_t*)ptr) = val;
            }
            break;
        case VAR_FLOAT:
            {
                float val;
                if (!sbufReadDataSafe(src, &val, sizeof(float))) {
                    return false;
                }
                if (val < (float)min || val > (float)max) {
                    return false;
                }
                *((float*)ptr) = val;
            }
            break;
    }

    return true;
}
/*
 * Returns MSP_RESULT_ACK, MSP_RESULT_ERROR or MSP_RESULT_NO_REPLY
 */
mspResult_e mspFcProcessCommand(mspPacket_t *cmd, mspPacket_t *reply, mspPostProcessFnPtr *mspPostProcessFn)
{
    int ret = MSP_RESULT_ACK;
    sbuf_t *dst = &reply->buf;
    sbuf_t *src = &cmd->buf;
    const uint16_t cmdMSP = cmd->cmd;
    // initialize reply by default
    reply->cmd = cmd->cmd;

    if (mspFcProcessOutCommand(cmdMSP, dst, mspPostProcessFn)) {
        ret = MSP_RESULT_ACK;
#ifdef USE_SERIAL_4WAY_BLHELI_INTERFACE
    } else if (cmdMSP == MSP_SET_4WAY_IF) {
        mspFc4waySerialCommand(dst, src, mspPostProcessFn);
        ret = MSP_RESULT_ACK;
#endif
#ifdef USE_NAV
    } else if (cmdMSP == MSP_WP) {
        mspFcWaypointOutCommand(dst, src);
        ret = MSP_RESULT_ACK;
#endif
#ifdef USE_FLASHFS
    } else if (cmdMSP == MSP_DATAFLASH_READ) {
        mspFcDataFlashReadCommand(dst, src);
        ret = MSP_RESULT_ACK;
#endif
    } else if (cmdMSP == MSP2_COMMON_SETTING) {
        ret = mspSettingCommand(dst, src) ? MSP_RESULT_ACK : MSP_RESULT_ERROR;
    } else if (cmdMSP == MSP2_COMMON_SET_SETTING) {
        ret = mspSetSettingCommand(dst, src) ? MSP_RESULT_ACK : MSP_RESULT_ERROR;
    } else {
        ret = mspFcProcessInCommand(cmdMSP, src);
    }

    // Process DONT_REPLY flag
    if (cmd->flags & MSP_FLAG_DONT_REPLY) {
        ret = MSP_RESULT_NO_REPLY;
    }

    reply->result = ret;
    return ret;
}

/*
 * Return a pointer to the process command function
 */
void mspFcInit(void)
{
    initActiveBoxIds();
}
