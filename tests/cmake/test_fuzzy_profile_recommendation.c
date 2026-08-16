#include <assert.h>

#include "FB_FuzzyController.h"
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

static FuzzyTunableParameters_t upper_parameters(void)
{
    FuzzyTunableParameters_t p;
    p.Ke = 0.060f;
    p.Kde = 0.120f;
    p.Ku = 0.900f;
    p.ErrorWindow = 19.0f;
    p.FullPowerErrorRatio = 0.044f;
    p.PrecisionErrorRatio = 0.036f;
    return p;
}

static void build_high_confidence(
    FB_FuzzyTemperatureProfile_t *profile,
    uint8_t region,
    const FuzzyTunableParameters_t *parameters)
{
    unsigned i;
    for (i = 0U; i < 10U; ++i)
    {
        assert(FB_FuzzyTemperatureProfile_RecordObservation(profile, region));
        assert(FB_FuzzyTemperatureProfile_RecordAccepted(profile, region, parameters));
    }
}

static void test_bridge_recommendation_is_read_only(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    FuzzyTemperatureRecommendation_t recommendation;
    FuzzyTunableParameters_t learned = learned_parameters();
    float oldKuTrim;
    float oldFullRatio;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);

    build_high_confidence(&bridge.TemperatureProfile, 3U, &learned);

    oldKuTrim = controller.scaling.Config.SelfTuneKuTrim;
    oldFullRatio = controller.config.FullPowerErrorRatio;

    assert(FB_FuzzySelfTuningBridge_GetRecommendation(
        &bridge, 175.0f, &recommendation));

    assert(recommendation.RegionIndex == 3);
    assert(recommendation.HasLearnedParameters);
    assert(recommendation.Level == FUZZY_TEMP_RECOMMEND_HIGH_CONFIDENCE);
    assert(recommendation.Confidence > 0.90f);
    assert(nearly_equal(recommendation.Parameters.Ku, learned.Ku, 0.000001f));

    assert(bridge.Config.ShadowMode);
    assert(!bridge.Status.CandidateAvailable);
    assert(!bridge.Status.CandidateApplied);
    assert(nearly_equal(controller.scaling.Config.SelfTuneKuTrim, oldKuTrim, 0.000001f));
    assert(nearly_equal(controller.config.FullPowerErrorRatio, oldFullRatio, 0.000001f));
}

static void test_bridge_interpolated_recommendation_is_read_only(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    FuzzyTemperatureInterpolatedRecommendation_t recommendation;
    FuzzyTunableParameters_t lower = learned_parameters();
    FuzzyTunableParameters_t upper = upper_parameters();
    float oldKuTrim;
    uint32_t observations2;
    uint32_t observations3;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);

    build_high_confidence(&bridge.TemperatureProfile, 2U, &lower);
    build_high_confidence(&bridge.TemperatureProfile, 3U, &upper);

    oldKuTrim = controller.scaling.Config.SelfTuneKuTrim;
    observations2 = bridge.TemperatureProfile.Regions[2].ObservationCount;
    observations3 = bridge.TemperatureProfile.Regions[3].ObservationCount;

    assert(FB_FuzzySelfTuningBridge_GetInterpolatedRecommendation(
        &bridge, 160.0f, &recommendation));

    assert(recommendation.Interpolated);
    assert(recommendation.LowerRegionIndex == 2);
    assert(recommendation.UpperRegionIndex == 3);
    assert(nearly_equal(recommendation.BlendFactor, 0.5f, 0.000001f));
    assert(nearly_equal(
        recommendation.Recommendation.Parameters.Ku,
        0.935f,
        0.000001f));
    assert(recommendation.Recommendation.Level == FUZZY_TEMP_RECOMMEND_HIGH_CONFIDENCE);

    assert(bridge.Config.ShadowMode);
    assert(!bridge.Status.CandidateAvailable);
    assert(!bridge.Status.CandidateApplied);
    assert(nearly_equal(controller.scaling.Config.SelfTuneKuTrim, oldKuTrim, 0.000001f));
    assert(bridge.TemperatureProfile.Regions[2].ObservationCount == observations2);
    assert(bridge.TemperatureProfile.Regions[3].ObservationCount == observations3);
}

static void test_bridge_recommendation_reports_unlearned_region(void)
{
    FB_FuzzySelfTuningBridge_t bridge;
    FuzzyTemperatureRecommendation_t recommendation;

    FB_FuzzySelfTuningBridge_Init(&bridge);

    assert(FB_FuzzySelfTuningBridge_GetRecommendation(
        &bridge, 130.0f, &recommendation));
    assert(recommendation.RegionIndex == 2);
    assert(!recommendation.HasLearnedParameters);
    assert(recommendation.Level == FUZZY_TEMP_RECOMMEND_NONE);
}

static void test_bridge_recommendation_rejects_out_of_range(void)
{
    FB_FuzzySelfTuningBridge_t bridge;
    FuzzyTemperatureRecommendation_t recommendation;
    FuzzyTemperatureInterpolatedRecommendation_t interpolated;

    FB_FuzzySelfTuningBridge_Init(&bridge);
    assert(!FB_FuzzySelfTuningBridge_GetRecommendation(
        &bridge, 300.0f, &recommendation));
    assert(!FB_FuzzySelfTuningBridge_GetInterpolatedRecommendation(
        &bridge, 300.0f, &interpolated));
}

int main(void)
{
    test_bridge_recommendation_is_read_only();
    test_bridge_interpolated_recommendation_is_read_only();
    test_bridge_recommendation_reports_unlearned_region();
    test_bridge_recommendation_rejects_out_of_range();
    return 0;
}
