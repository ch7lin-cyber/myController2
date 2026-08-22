#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FB_FuzzyProfilePersistence.h"
#include "FB_FuzzySelfTuningBridge.h"

static int nearly_equal(float a, float b, float eps)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= eps;
}

static FuzzyTunableParameters_t learned_parameters(void)
{
    FuzzyTunableParameters_t p;
    p.Ke = 0.052f;
    p.Kde = 0.103f;
    p.Ku = 0.970f;
    p.ErrorWindow = 21.0f;
    p.FullPowerErrorRatio = 0.048f;
    p.PrecisionErrorRatio = 0.032f;
    return p;
}

static void build_profile(FB_FuzzyTemperatureProfile_t *profile)
{
    FuzzyTunableParameters_t p = learned_parameters();
    unsigned i;

    FB_FuzzyTemperatureProfile_Init(profile);
    for (i = 0U; i < 6U; ++i)
    {
        assert(FB_FuzzyTemperatureProfile_RecordObservation(profile, 3U));
        assert(FB_FuzzyTemperatureProfile_RecordAccepted(profile, 3U, &p));
    }
    assert(FB_FuzzyTemperatureProfile_RecordObservation(profile, 3U));
    assert(FB_FuzzyTemperatureProfile_RecordRollback(profile, 3U));
}

static void test_round_trip_restores_profile_and_recomputes_confidence(void)
{
    FB_FuzzyTemperatureProfile_t source;
    FB_FuzzyTemperatureProfile_t restored;
    const FuzzyTemperatureRegion_t *src;
    const FuzzyTemperatureRegion_t *dst;
    uint8_t buffer[FUZZY_PROFILE_PERSIST_MAX_SIZE];
    size_t written = 0U;
    size_t expected;

    build_profile(&source);
    FB_FuzzyTemperatureProfile_Init(&restored);

    expected = FB_FuzzyProfilePersistence_GetSerializedSize(&source);
    assert(expected == (size_t)(16U + 4U + 5U * 48U));
    assert(FB_FuzzyProfilePersistence_Export(
        &source, buffer, sizeof(buffer), &written) == FUZZY_PROFILE_PERSIST_OK);
    assert(written == expected);

    assert(FB_FuzzyProfilePersistence_Import(
        &restored, buffer, written) == FUZZY_PROFILE_PERSIST_OK);

    assert(restored.RegionCount == source.RegionCount);
    src = FB_FuzzyTemperatureProfile_GetRegion(&source, 3U);
    dst = FB_FuzzyTemperatureProfile_GetRegion(&restored, 3U);
    assert(src != 0);
    assert(dst != 0);
    assert(dst->ObservationCount == src->ObservationCount);
    assert(dst->AcceptedCount == src->AcceptedCount);
    assert(dst->RollbackCount == src->RollbackCount);
    assert(dst->HasLearnedParameters);
    assert(nearly_equal(dst->LearnedParameters.Ku, src->LearnedParameters.Ku, 0.000001f));
    assert(nearly_equal(dst->Confidence, src->Confidence, 0.000001f));
}

static void test_crc_corruption_is_rejected_atomically(void)
{
    FB_FuzzyTemperatureProfile_t source;
    FB_FuzzyTemperatureProfile_t target;
    uint8_t buffer[FUZZY_PROFILE_PERSIST_MAX_SIZE];
    size_t written = 0U;
    uint32_t originalObservations;

    build_profile(&source);
    build_profile(&target);
    originalObservations = target.Regions[3].ObservationCount;

    assert(FB_FuzzyProfilePersistence_Export(
        &source, buffer, sizeof(buffer), &written) == FUZZY_PROFILE_PERSIST_OK);

    buffer[FUZZY_PROFILE_PERSIST_HEADER_SIZE + 12U] ^= 0x5AU;

    assert(FB_FuzzyProfilePersistence_Import(
        &target, buffer, written) == FUZZY_PROFILE_PERSIST_CRC_MISMATCH);
    assert(target.Regions[3].ObservationCount == originalObservations);
    assert(target.RegionCount == FUZZY_TEMP_PROFILE_DEFAULT_REGIONS);
}

static void test_bad_magic_and_version_are_rejected(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    uint8_t buffer[FUZZY_PROFILE_PERSIST_MAX_SIZE];
    size_t written = 0U;

    build_profile(&profile);
    assert(FB_FuzzyProfilePersistence_Export(
        &profile, buffer, sizeof(buffer), &written) == FUZZY_PROFILE_PERSIST_OK);

    buffer[0] ^= 0x01U;
    assert(FB_FuzzyProfilePersistence_Import(
        &profile, buffer, written) == FUZZY_PROFILE_PERSIST_BAD_MAGIC);
    buffer[0] ^= 0x01U;

    buffer[4] = 0x7FU;
    buffer[5] = 0x00U;
    assert(FB_FuzzyProfilePersistence_Import(
        &profile, buffer, written) == FUZZY_PROFILE_PERSIST_UNSUPPORTED_VERSION);
}

static void test_invalid_counters_rejected_even_with_valid_crc(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    uint8_t buffer[FUZZY_PROFILE_PERSIST_MAX_SIZE];
    size_t written = 0U;
    size_t region3;
    uint32_t crc;

    build_profile(&profile);
    assert(FB_FuzzyProfilePersistence_Export(
        &profile, buffer, sizeof(buffer), &written) == FUZZY_PROFILE_PERSIST_OK);

    region3 = FUZZY_PROFILE_PERSIST_HEADER_SIZE + FUZZY_PROFILE_PERSIST_PAYLOAD_BASE +
              (3U * FUZZY_PROFILE_PERSIST_REGION_SIZE);

    buffer[region3 + 36U] = 100U;
    buffer[region3 + 37U] = 0U;
    buffer[region3 + 38U] = 0U;
    buffer[region3 + 39U] = 0U;

    crc = FB_FuzzyProfilePersistence_CalculateCRC32(
        &buffer[FUZZY_PROFILE_PERSIST_HEADER_SIZE],
        written - FUZZY_PROFILE_PERSIST_HEADER_SIZE);
    buffer[12] = (uint8_t)(crc & 0xFFU);
    buffer[13] = (uint8_t)((crc >> 8) & 0xFFU);
    buffer[14] = (uint8_t)((crc >> 16) & 0xFFU);
    buffer[15] = (uint8_t)((crc >> 24) & 0xFFU);

    assert(FB_FuzzyProfilePersistence_Import(
        &profile, buffer, written) == FUZZY_PROFILE_PERSIST_BAD_COUNTERS);
}

static void test_export_rejects_small_buffer(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    uint8_t buffer[32];
    size_t written = 123U;

    build_profile(&profile);
    assert(FB_FuzzyProfilePersistence_Export(
        &profile, buffer, sizeof(buffer), &written) == FUZZY_PROFILE_PERSIST_BUFFER_TOO_SMALL);
    assert(written == 0U);
}

static void test_bridge_idle_import_is_safe_and_read_only_to_controller(void)
{
    FB_FuzzySelfTuningBridge_t source;
    FB_FuzzySelfTuningBridge_t target;
    FuzzyTunableParameters_t p = learned_parameters();
    uint8_t buffer[FUZZY_PROFILE_PERSIST_MAX_SIZE];
    size_t written = 0U;
    unsigned i;

    FB_FuzzySelfTuningBridge_Init(&source);
    FB_FuzzySelfTuningBridge_Init(&target);

    for (i = 0U; i < 6U; ++i)
    {
        assert(FB_FuzzyTemperatureProfile_RecordObservation(&source.TemperatureProfile, 3U));
        assert(FB_FuzzyTemperatureProfile_RecordAccepted(&source.TemperatureProfile, 3U, &p));
    }

    assert(FB_FuzzySelfTuningBridge_ExportProfile(
        &source, buffer, sizeof(buffer), &written) == FUZZY_PROFILE_PERSIST_OK);
    assert(FB_FuzzySelfTuningBridge_ImportProfile(
        &target, buffer, written) == FUZZY_PROFILE_PERSIST_OK);

    assert(target.Config.ShadowMode);
    assert(!target.Status.CandidateAvailable);
    assert(!target.Status.CandidateApplied);
    assert(target.Status.ActiveRegion == FUZZY_SELF_TUNING_REGION_INVALID);
    assert(target.TemperatureProfile.Regions[3].HasLearnedParameters);
    assert(target.TemperatureProfile.Regions[3].AcceptedCount == 6U);
}

static void test_bridge_import_rejected_during_tuning_decision(void)
{
    FB_FuzzySelfTuningBridge_t source;
    FB_FuzzySelfTuningBridge_t target;
    uint8_t buffer[FUZZY_PROFILE_PERSIST_MAX_SIZE];
    size_t written = 0U;

    FB_FuzzySelfTuningBridge_Init(&source);
    FB_FuzzySelfTuningBridge_Init(&target);
    assert(FB_FuzzySelfTuningBridge_ExportProfile(
        &source, buffer, sizeof(buffer), &written) == FUZZY_PROFILE_PERSIST_OK);

    target.Status.EpisodeActive = true;
    assert(FB_FuzzySelfTuningBridge_ImportProfile(
        &target, buffer, written) == FUZZY_PROFILE_PERSIST_BUSY);
    target.Status.EpisodeActive = false;

    target.Status.CandidateAvailable = true;
    assert(FB_FuzzySelfTuningBridge_ImportProfile(
        &target, buffer, written) == FUZZY_PROFILE_PERSIST_BUSY);
}

int main(void)
{
    test_round_trip_restores_profile_and_recomputes_confidence();
    test_crc_corruption_is_rejected_atomically();
    test_bad_magic_and_version_are_rejected();
    test_invalid_counters_rejected_even_with_valid_crc();
    test_export_rejects_small_buffer();
    test_bridge_idle_import_is_safe_and_read_only_to_controller();
    test_bridge_import_rejected_during_tuning_decision();
    return 0;
}
