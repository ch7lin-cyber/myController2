/******************************************************************************
 * File    : FB_FuzzyProfilePersistence.h
 * Brief   : Versioned, CRC-protected persistence for temperature self-tuning profiles.
 *
 * Design rules:
 *   - Stable byte format; never serialize native C structs directly.
 *   - Little-endian integer/IEEE-754 float encoding.
 *   - CRC32 protects payload integrity.
 *   - Import validates before replacing the live profile.
 *   - Confidence is NOT persisted as trusted state; it is recomputed from counters.
 *   - This module never modifies controller parameters and never applies candidates.
 ******************************************************************************/
#ifndef FB_FUZZY_PROFILE_PERSISTENCE_H
#define FB_FUZZY_PROFILE_PERSISTENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ssm_std_define.h"
#include "FB_FuzzyTemperatureProfile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FUZZY_PROFILE_PERSIST_MAGIC            (0x50545346UL) /* "FSTP" LE */
#define FUZZY_PROFILE_PERSIST_VERSION          (1U)
#define FUZZY_PROFILE_PERSIST_HEADER_SIZE      (16U)
#define FUZZY_PROFILE_PERSIST_REGION_SIZE      (48U)
#define FUZZY_PROFILE_PERSIST_PAYLOAD_BASE     (4U)
#define FUZZY_PROFILE_PERSIST_MAX_SIZE \
    (FUZZY_PROFILE_PERSIST_HEADER_SIZE + FUZZY_PROFILE_PERSIST_PAYLOAD_BASE + \
     (FUZZY_TEMP_PROFILE_MAX_REGIONS * FUZZY_PROFILE_PERSIST_REGION_SIZE))

typedef enum
{
    FUZZY_PROFILE_PERSIST_OK = 0,
    FUZZY_PROFILE_PERSIST_INVALID_ARGUMENT,
    FUZZY_PROFILE_PERSIST_BUFFER_TOO_SMALL,
    FUZZY_PROFILE_PERSIST_BAD_MAGIC,
    FUZZY_PROFILE_PERSIST_UNSUPPORTED_VERSION,
    FUZZY_PROFILE_PERSIST_BAD_LENGTH,
    FUZZY_PROFILE_PERSIST_CRC_MISMATCH,
    FUZZY_PROFILE_PERSIST_BAD_REGION_COUNT,
    FUZZY_PROFILE_PERSIST_BAD_REGION_LAYOUT,
    FUZZY_PROFILE_PERSIST_BAD_COUNTERS,
    FUZZY_PROFILE_PERSIST_BAD_PARAMETERS
} FuzzyProfilePersistenceResult_e;

MY_API size_t FB_FuzzyProfilePersistence_GetSerializedSize(
    const FB_FuzzyTemperatureProfile_t *profile);

MY_API uint32_t FB_FuzzyProfilePersistence_CalculateCRC32(
    const uint8_t *data,
    size_t length);

MY_API FuzzyProfilePersistenceResult_e FB_FuzzyProfilePersistence_Export(
    const FB_FuzzyTemperatureProfile_t *profile,
    uint8_t *buffer,
    size_t capacity,
    size_t *writtenSize);

MY_API FuzzyProfilePersistenceResult_e FB_FuzzyProfilePersistence_Import(
    FB_FuzzyTemperatureProfile_t *profile,
    const uint8_t *buffer,
    size_t size);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_PROFILE_PERSISTENCE_H */
