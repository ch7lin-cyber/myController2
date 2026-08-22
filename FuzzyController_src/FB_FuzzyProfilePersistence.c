/******************************************************************************
 * File    : FB_FuzzyProfilePersistence.c
 * Brief   : Stable byte serialization for temperature self-tuning profiles.
 ******************************************************************************/
#include "FB_FuzzyProfilePersistence.h"

#include <string.h>

static void write_u16_le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void write_u32_le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8) & 0xFFU);
    p[2] = (uint8_t)((value >> 16) & 0xFFU);
    p[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_float_le(uint8_t *p, float value)
{
    uint32_t bits = 0U;
    memcpy(&bits, &value, sizeof(bits));
    write_u32_le(p, bits);
}

static float read_float_le(const uint8_t *p)
{
    uint32_t bits = read_u32_le(p);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool float_is_finite(float value)
{
    uint32_t bits = 0U;
    memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7F800000UL) != 0x7F800000UL;
}

static bool learned_parameters_are_valid(const FuzzyTunableParameters_t *p)
{
    FB_FuzzyParameterGuard_t guard;

    if (p == (const FuzzyTunableParameters_t *)0)
    {
        return false;
    }

    if (!float_is_finite(p->Ke) ||
        !float_is_finite(p->Kde) ||
        !float_is_finite(p->Ku) ||
        !float_is_finite(p->ErrorWindow) ||
        !float_is_finite(p->FullPowerErrorRatio) ||
        !float_is_finite(p->PrecisionErrorRatio))
    {
        return false;
    }

    FB_FuzzyParameterGuard_Init(&guard);

    if ((p->Ke < guard.Config.MinKe) || (p->Ke > guard.Config.MaxKe) ||
        (p->Kde < guard.Config.MinKde) || (p->Kde > guard.Config.MaxKde) ||
        (p->Ku < guard.Config.MinKu) || (p->Ku > guard.Config.MaxKu) ||
        (p->ErrorWindow < guard.Config.MinErrorWindow) ||
        (p->ErrorWindow > guard.Config.MaxErrorWindow) ||
        (p->FullPowerErrorRatio < guard.Config.MinFullPowerErrorRatio) ||
        (p->FullPowerErrorRatio > guard.Config.MaxFullPowerErrorRatio) ||
        (p->PrecisionErrorRatio < guard.Config.MinPrecisionErrorRatio) ||
        (p->PrecisionErrorRatio > guard.Config.MaxPrecisionErrorRatio) ||
        (p->FullPowerErrorRatio <= p->PrecisionErrorRatio))
    {
        return false;
    }

    return true;
}

size_t FB_FuzzyProfilePersistence_GetSerializedSize(
    const FB_FuzzyTemperatureProfile_t *profile)
{
    if ((profile == (const FB_FuzzyTemperatureProfile_t *)0) ||
        !profile->Initialized ||
        (profile->RegionCount == 0U) ||
        (profile->RegionCount > FUZZY_TEMP_PROFILE_MAX_REGIONS))
    {
        return 0U;
    }

    return (size_t)FUZZY_PROFILE_PERSIST_HEADER_SIZE +
           (size_t)FUZZY_PROFILE_PERSIST_PAYLOAD_BASE +
           ((size_t)profile->RegionCount * (size_t)FUZZY_PROFILE_PERSIST_REGION_SIZE);
}

uint32_t FB_FuzzyProfilePersistence_CalculateCRC32(
    const uint8_t *data,
    size_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    size_t i;
    uint8_t bit;

    if ((data == (const uint8_t *)0) && (length != 0U))
    {
        return 0U;
    }

    for (i = 0U; i < length; ++i)
    {
        crc ^= (uint32_t)data[i];
        for (bit = 0U; bit < 8U; ++bit)
        {
            uint32_t mask = (uint32_t)(0U - (crc & 1U));
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }

    return ~crc;
}

FuzzyProfilePersistenceResult_e FB_FuzzyProfilePersistence_Export(
    const FB_FuzzyTemperatureProfile_t *profile,
    uint8_t *buffer,
    size_t capacity,
    size_t *writtenSize)
{
    size_t totalSize;
    size_t payloadSize;
    size_t offset;
    uint8_t i;
    uint32_t crc;

    if ((profile == (const FB_FuzzyTemperatureProfile_t *)0) ||
        (buffer == (uint8_t *)0) ||
        (writtenSize == (size_t *)0))
    {
        return FUZZY_PROFILE_PERSIST_INVALID_ARGUMENT;
    }

    *writtenSize = 0U;
    totalSize = FB_FuzzyProfilePersistence_GetSerializedSize(profile);
    if (totalSize == 0U)
    {
        return FUZZY_PROFILE_PERSIST_BAD_REGION_COUNT;
    }
    if (capacity < totalSize)
    {
        return FUZZY_PROFILE_PERSIST_BUFFER_TOO_SMALL;
    }

    payloadSize = totalSize - FUZZY_PROFILE_PERSIST_HEADER_SIZE;
    memset(buffer, 0, totalSize);

    write_u32_le(&buffer[0], FUZZY_PROFILE_PERSIST_MAGIC);
    write_u16_le(&buffer[4], FUZZY_PROFILE_PERSIST_VERSION);
    write_u16_le(&buffer[6], FUZZY_PROFILE_PERSIST_HEADER_SIZE);
    write_u32_le(&buffer[8], (uint32_t)payloadSize);

    offset = FUZZY_PROFILE_PERSIST_HEADER_SIZE;
    buffer[offset] = profile->RegionCount;
    offset += FUZZY_PROFILE_PERSIST_PAYLOAD_BASE;

    for (i = 0U; i < profile->RegionCount; ++i)
    {
        const FuzzyTemperatureRegion_t *r = &profile->Regions[i];
        uint32_t flags = r->HasLearnedParameters ? 1UL : 0UL;

        if (!float_is_finite(r->MinTemperature_c) ||
            !float_is_finite(r->MaxTemperature_c) ||
            (r->MinTemperature_c >= r->MaxTemperature_c))
        {
            return FUZZY_PROFILE_PERSIST_BAD_REGION_LAYOUT;
        }
        if ((i > 0U) &&
            (profile->Regions[i - 1U].MaxTemperature_c > r->MinTemperature_c))
        {
            return FUZZY_PROFILE_PERSIST_BAD_REGION_LAYOUT;
        }
        if ((r->AcceptedCount > r->ObservationCount) ||
            (r->RollbackCount > r->ObservationCount))
        {
            return FUZZY_PROFILE_PERSIST_BAD_COUNTERS;
        }
        if (r->HasLearnedParameters &&
            ((r->AcceptedCount == 0U) || !learned_parameters_are_valid(&r->LearnedParameters)))
        {
            return FUZZY_PROFILE_PERSIST_BAD_PARAMETERS;
        }

        write_float_le(&buffer[offset + 0U], r->MinTemperature_c);
        write_float_le(&buffer[offset + 4U], r->MaxTemperature_c);
        write_float_le(&buffer[offset + 8U], r->LearnedParameters.Ke);
        write_float_le(&buffer[offset + 12U], r->LearnedParameters.Kde);
        write_float_le(&buffer[offset + 16U], r->LearnedParameters.Ku);
        write_float_le(&buffer[offset + 20U], r->LearnedParameters.ErrorWindow);
        write_float_le(&buffer[offset + 24U], r->LearnedParameters.FullPowerErrorRatio);
        write_float_le(&buffer[offset + 28U], r->LearnedParameters.PrecisionErrorRatio);
        write_u32_le(&buffer[offset + 32U], r->ObservationCount);
        write_u32_le(&buffer[offset + 36U], r->AcceptedCount);
        write_u32_le(&buffer[offset + 40U], r->RollbackCount);
        write_u32_le(&buffer[offset + 44U], flags);
        offset += FUZZY_PROFILE_PERSIST_REGION_SIZE;
    }

    crc = FB_FuzzyProfilePersistence_CalculateCRC32(
        &buffer[FUZZY_PROFILE_PERSIST_HEADER_SIZE],
        payloadSize);
    write_u32_le(&buffer[12], crc);

    *writtenSize = totalSize;
    return FUZZY_PROFILE_PERSIST_OK;
}

FuzzyProfilePersistenceResult_e FB_FuzzyProfilePersistence_Import(
    FB_FuzzyTemperatureProfile_t *profile,
    const uint8_t *buffer,
    size_t size)
{
    FuzzyTemperatureRegion_t regions[FUZZY_TEMP_PROFILE_MAX_REGIONS];
    FB_FuzzyTemperatureProfile_t temporary;
    uint32_t payloadSize;
    uint32_t storedCRC;
    uint32_t calculatedCRC;
    uint32_t flags;
    size_t expectedSize;
    size_t offset;
    uint8_t regionCount;
    uint8_t i;

    if ((profile == (FB_FuzzyTemperatureProfile_t *)0) ||
        (buffer == (const uint8_t *)0))
    {
        return FUZZY_PROFILE_PERSIST_INVALID_ARGUMENT;
    }
    if (size < FUZZY_PROFILE_PERSIST_HEADER_SIZE)
    {
        return FUZZY_PROFILE_PERSIST_BAD_LENGTH;
    }
    if (read_u32_le(&buffer[0]) != FUZZY_PROFILE_PERSIST_MAGIC)
    {
        return FUZZY_PROFILE_PERSIST_BAD_MAGIC;
    }
    if (read_u16_le(&buffer[4]) != FUZZY_PROFILE_PERSIST_VERSION)
    {
        return FUZZY_PROFILE_PERSIST_UNSUPPORTED_VERSION;
    }
    if (read_u16_le(&buffer[6]) != FUZZY_PROFILE_PERSIST_HEADER_SIZE)
    {
        return FUZZY_PROFILE_PERSIST_BAD_LENGTH;
    }

    payloadSize = read_u32_le(&buffer[8]);
    expectedSize = (size_t)FUZZY_PROFILE_PERSIST_HEADER_SIZE + (size_t)payloadSize;
    if ((expectedSize != size) ||
        (payloadSize < FUZZY_PROFILE_PERSIST_PAYLOAD_BASE) ||
        (expectedSize > FUZZY_PROFILE_PERSIST_MAX_SIZE))
    {
        return FUZZY_PROFILE_PERSIST_BAD_LENGTH;
    }

    storedCRC = read_u32_le(&buffer[12]);
    calculatedCRC = FB_FuzzyProfilePersistence_CalculateCRC32(
        &buffer[FUZZY_PROFILE_PERSIST_HEADER_SIZE],
        payloadSize);
    if (storedCRC != calculatedCRC)
    {
        return FUZZY_PROFILE_PERSIST_CRC_MISMATCH;
    }

    regionCount = buffer[FUZZY_PROFILE_PERSIST_HEADER_SIZE];
    if ((regionCount == 0U) || (regionCount > FUZZY_TEMP_PROFILE_MAX_REGIONS))
    {
        return FUZZY_PROFILE_PERSIST_BAD_REGION_COUNT;
    }

    expectedSize = (size_t)FUZZY_PROFILE_PERSIST_HEADER_SIZE +
                   (size_t)FUZZY_PROFILE_PERSIST_PAYLOAD_BASE +
                   ((size_t)regionCount * (size_t)FUZZY_PROFILE_PERSIST_REGION_SIZE);
    if (expectedSize != size)
    {
        return FUZZY_PROFILE_PERSIST_BAD_LENGTH;
    }

    memset(regions, 0, sizeof(regions));
    offset = FUZZY_PROFILE_PERSIST_HEADER_SIZE + FUZZY_PROFILE_PERSIST_PAYLOAD_BASE;

    for (i = 0U; i < regionCount; ++i)
    {
        FuzzyTemperatureRegion_t *r = &regions[i];
        r->MinTemperature_c = read_float_le(&buffer[offset + 0U]);
        r->MaxTemperature_c = read_float_le(&buffer[offset + 4U]);
        r->LearnedParameters.Ke = read_float_le(&buffer[offset + 8U]);
        r->LearnedParameters.Kde = read_float_le(&buffer[offset + 12U]);
        r->LearnedParameters.Ku = read_float_le(&buffer[offset + 16U]);
        r->LearnedParameters.ErrorWindow = read_float_le(&buffer[offset + 20U]);
        r->LearnedParameters.FullPowerErrorRatio = read_float_le(&buffer[offset + 24U]);
        r->LearnedParameters.PrecisionErrorRatio = read_float_le(&buffer[offset + 28U]);
        r->ObservationCount = read_u32_le(&buffer[offset + 32U]);
        r->AcceptedCount = read_u32_le(&buffer[offset + 36U]);
        r->RollbackCount = read_u32_le(&buffer[offset + 40U]);
        flags = read_u32_le(&buffer[offset + 44U]);
        r->HasLearnedParameters = (flags & 1UL) != 0UL;
        r->Confidence = 0.0f;

        if ((flags & ~1UL) != 0UL)
        {
            return FUZZY_PROFILE_PERSIST_BAD_PARAMETERS;
        }
        if (!float_is_finite(r->MinTemperature_c) ||
            !float_is_finite(r->MaxTemperature_c) ||
            (r->MinTemperature_c >= r->MaxTemperature_c))
        {
            return FUZZY_PROFILE_PERSIST_BAD_REGION_LAYOUT;
        }
        if ((i > 0U) && (regions[i - 1U].MaxTemperature_c > r->MinTemperature_c))
        {
            return FUZZY_PROFILE_PERSIST_BAD_REGION_LAYOUT;
        }
        if ((r->AcceptedCount > r->ObservationCount) ||
            (r->RollbackCount > r->ObservationCount))
        {
            return FUZZY_PROFILE_PERSIST_BAD_COUNTERS;
        }
        if (r->HasLearnedParameters)
        {
            if ((r->AcceptedCount == 0U) || !learned_parameters_are_valid(&r->LearnedParameters))
            {
                return FUZZY_PROFILE_PERSIST_BAD_PARAMETERS;
            }
        }

        offset += FUZZY_PROFILE_PERSIST_REGION_SIZE;
    }

    FB_FuzzyTemperatureProfile_Init(&temporary);
    if (!FB_FuzzyTemperatureProfile_SetRegions(&temporary, regions, regionCount))
    {
        return FUZZY_PROFILE_PERSIST_BAD_REGION_LAYOUT;
    }

    /* Atomic from the caller's perspective: live profile is replaced only after validation. */
    *profile = temporary;
    return FUZZY_PROFILE_PERSIST_OK;
}
