#include <assert.h>

#include "FB_FuzzyTemperatureProfile.h"

static int nearly_equal(float a, float b, float eps)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= eps;
}

static FuzzyTunableParameters_t sample_parameters(void)
{
    FuzzyTunableParameters_t p;
    p.Ke = 0.05f;
    p.Kde = 0.10f;
    p.Ku = 0.95f;
    p.ErrorWindow = 22.0f;
    p.FullPowerErrorRatio = 0.05f;
    p.PrecisionErrorRatio = 0.03f;
    return p;
}

static void test_default_region_mapping(void)
{
    FB_FuzzyTemperatureProfile_t profile;

    FB_FuzzyTemperatureProfile_Init(&profile);

    assert(profile.RegionCount == 5U);
    assert(FB_FuzzyTemperatureProfile_FindRegion(&profile, 50.0f) == 0);
    assert(FB_FuzzyTemperatureProfile_FindRegion(&profile, 79.9f) == 0);
    assert(FB_FuzzyTemperatureProfile_FindRegion(&profile, 80.0f) == 1);
    assert(FB_FuzzyTemperatureProfile_FindRegion(&profile, 130.0f) == 2);
    assert(FB_FuzzyTemperatureProfile_FindRegion(&profile, 175.0f) == 3);
    assert(FB_FuzzyTemperatureProfile_FindRegion(&profile, 250.0f) == 4);
    assert(FB_FuzzyTemperatureProfile_FindRegion(&profile, 49.9f) == -1);
    assert(FB_FuzzyTemperatureProfile_FindRegion(&profile, 250.1f) == -1);
}

static void test_confidence_grows_conservatively(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    FuzzyTunableParameters_t learned = sample_parameters();
    const FuzzyTemperatureRegion_t *region;
    float firstConfidence;
    unsigned i;

    FB_FuzzyTemperatureProfile_Init(&profile);

    assert(FB_FuzzyTemperatureProfile_RecordObservation(&profile, 2U));
    assert(FB_FuzzyTemperatureProfile_RecordAccepted(&profile, 2U, &learned));

    region = FB_FuzzyTemperatureProfile_GetRegion(&profile, 2U);
    assert(region != 0);
    assert(region->HasLearnedParameters);
    assert(region->ObservationCount == 1U);
    assert(region->AcceptedCount == 1U);
    assert(region->Confidence > 0.0f);
    assert(region->Confidence < 0.80f);
    firstConfidence = region->Confidence;

    for (i = 0U; i < 9U; ++i)
    {
        assert(FB_FuzzyTemperatureProfile_RecordObservation(&profile, 2U));
        assert(FB_FuzzyTemperatureProfile_RecordAccepted(&profile, 2U, &learned));
    }

    region = FB_FuzzyTemperatureProfile_GetRegion(&profile, 2U);
    assert(region->ObservationCount == 10U);
    assert(region->AcceptedCount == 10U);
    assert(region->Confidence > firstConfidence);
    assert(region->Confidence > 0.90f);
    assert(nearly_equal(region->LearnedParameters.Ku, learned.Ku, 0.000001f));
}

static void test_rollback_reduces_confidence(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    FuzzyTunableParameters_t learned = sample_parameters();
    const FuzzyTemperatureRegion_t *region;
    float beforeRollback;
    unsigned i;

    FB_FuzzyTemperatureProfile_Init(&profile);

    for (i = 0U; i < 5U; ++i)
    {
        assert(FB_FuzzyTemperatureProfile_RecordObservation(&profile, 3U));
        assert(FB_FuzzyTemperatureProfile_RecordAccepted(&profile, 3U, &learned));
    }

    region = FB_FuzzyTemperatureProfile_GetRegion(&profile, 3U);
    beforeRollback = region->Confidence;

    assert(FB_FuzzyTemperatureProfile_RecordRollback(&profile, 3U));
    region = FB_FuzzyTemperatureProfile_GetRegion(&profile, 3U);

    assert(region->RollbackCount == 1U);
    assert(region->Confidence < beforeRollback);
}

static void test_regions_learn_independently(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    FuzzyTunableParameters_t p = sample_parameters();
    const FuzzyTemperatureRegion_t *r2;
    const FuzzyTemperatureRegion_t *r3;

    FB_FuzzyTemperatureProfile_Init(&profile);

    assert(FB_FuzzyTemperatureProfile_RecordObservation(&profile, 2U));
    assert(FB_FuzzyTemperatureProfile_RecordAccepted(&profile, 2U, &p));

    r2 = FB_FuzzyTemperatureProfile_GetRegion(&profile, 2U);
    r3 = FB_FuzzyTemperatureProfile_GetRegion(&profile, 3U);

    assert(r2->HasLearnedParameters);
    assert(!r3->HasLearnedParameters);
    assert(r2->ObservationCount == 1U);
    assert(r3->ObservationCount == 0U);
}

static void test_recommendation_is_read_only(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    FuzzyTemperatureRecommendation_t recommendation;
    const FuzzyTemperatureRegion_t *region;
    uint32_t observationsBefore;

    FB_FuzzyTemperatureProfile_Init(&profile);
    region = FB_FuzzyTemperatureProfile_GetRegion(&profile, 3U);
    observationsBefore = region->ObservationCount;

    assert(FB_FuzzyTemperatureProfile_GetRecommendation(
        &profile,
        175.0f,
        FUZZY_TEMP_PROFILE_RECOMMEND_MIN_DEFAULT,
        FUZZY_TEMP_PROFILE_RECOMMEND_HIGH_DEFAULT,
        &recommendation));

    assert(recommendation.RegionIndex == 3);
    assert(!recommendation.HasLearnedParameters);
    assert(recommendation.Level == FUZZY_TEMP_RECOMMEND_NONE);

    region = FB_FuzzyTemperatureProfile_GetRegion(&profile, 3U);
    assert(region->ObservationCount == observationsBefore);
    assert(!region->HasLearnedParameters);
}

static void test_recommendation_confidence_levels(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    FuzzyTemperatureRecommendation_t recommendation;
    FuzzyTunableParameters_t learned = sample_parameters();
    unsigned i;

    FB_FuzzyTemperatureProfile_Init(&profile);

    /* One successful observation is intentionally only experimental. */
    assert(FB_FuzzyTemperatureProfile_RecordObservation(&profile, 3U));
    assert(FB_FuzzyTemperatureProfile_RecordAccepted(&profile, 3U, &learned));

    assert(FB_FuzzyTemperatureProfile_GetRecommendation(
        &profile,
        175.0f,
        FUZZY_TEMP_PROFILE_RECOMMEND_MIN_DEFAULT,
        FUZZY_TEMP_PROFILE_RECOMMEND_HIGH_DEFAULT,
        &recommendation));

    assert(recommendation.HasLearnedParameters);
    assert(recommendation.Level == FUZZY_TEMP_RECOMMEND_EXPERIMENTAL);
    assert(nearly_equal(recommendation.Parameters.Ku, learned.Ku, 0.000001f));

    for (i = 0U; i < 9U; ++i)
    {
        assert(FB_FuzzyTemperatureProfile_RecordObservation(&profile, 3U));
        assert(FB_FuzzyTemperatureProfile_RecordAccepted(&profile, 3U, &learned));
    }

    assert(FB_FuzzyTemperatureProfile_GetRecommendation(
        &profile,
        175.0f,
        FUZZY_TEMP_PROFILE_RECOMMEND_MIN_DEFAULT,
        FUZZY_TEMP_PROFILE_RECOMMEND_HIGH_DEFAULT,
        &recommendation));

    assert(recommendation.Level == FUZZY_TEMP_RECOMMEND_HIGH_CONFIDENCE);
    assert(recommendation.Confidence > 0.90f);
}

static void test_recommendation_rejects_invalid_query(void)
{
    FB_FuzzyTemperatureProfile_t profile;
    FuzzyTemperatureRecommendation_t recommendation;

    FB_FuzzyTemperatureProfile_Init(&profile);

    assert(!FB_FuzzyTemperatureProfile_GetRecommendation(
        &profile, 300.0f, 0.30f, 0.70f, &recommendation));
    assert(!FB_FuzzyTemperatureProfile_GetRecommendation(
        &profile, 130.0f, 0.80f, 0.70f, &recommendation));
    assert(!FB_FuzzyTemperatureProfile_GetRecommendation(
        &profile, 130.0f, -0.10f, 0.70f, &recommendation));
}

int main(void)
{
    test_default_region_mapping();
    test_confidence_grows_conservatively();
    test_rollback_reduces_confidence();
    test_regions_learn_independently();
    test_recommendation_is_read_only();
    test_recommendation_confidence_levels();
    test_recommendation_rejects_invalid_query();
    return 0;
}
