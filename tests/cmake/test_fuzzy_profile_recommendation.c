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

static void test_bridge_recommendation_is_read_only(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    FuzzyTemperatureRecommendation_t recommendation;
    FuzzyTunableParameters_t learned = learned_parameters();
    float oldKuTrim;
    float oldFullRatio;
    unsigned i;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);

    /* Build high confidence in Region 3 (160..200 C) without touching controller. */
    for (i = 0U; i < 10U; ++i)
    {
        assert(FB_FuzzyTemperatureProfile_RecordObservation(
            &bridge.TemperatureProfile, 3U));
        assert(FB_FuzzyTemperatureProfile_RecordAccepted(
            &bridge.TemperatureProfile, 3U, &learned));
    }

    oldKuTrim = controller.scaling.Config.SelfTuneKuTrim;
    oldFullRatio = controller.config.FullPowerErrorRatio;

    assert(FB_FuzzySelfTuningBridge_GetRecommendation(
        &bridge, 175.0f, &recommendation));

    assert(recommendation.RegionIndex == 3);
    assert(recommendation.HasLearnedParameters);
    assert(recommendation.Level == FUZZY_TEMP_RECOMMEND_HIGH_CONFIDENCE);
    assert(recommendation.Confidence > 0.90f);
    assert(nearly_equal(recommendation.Parameters.Ku, learned.Ku, 0.000001f));

    /* Query-only API must not change runtime control state. */
    assert(bridge.Config.ShadowMode);
    assert(!bridge.Status.CandidateAvailable);
    assert(!bridge.Status.CandidateApplied);
    assert(nearly_equal(controller.scaling.Config.SelfTuneKuTrim, oldKuTrim, 0.000001f));
    assert(nearly_equal(controller.config.FullPowerErrorRatio, oldFullRatio, 0.000001f));
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

    FB_FuzzySelfTuningBridge_Init(&bridge);
    assert(!FB_FuzzySelfTuningBridge_GetRecommendation(
        &bridge, 300.0f, &recommendation));
}

int main(void)
{
    test_bridge_recommendation_is_read_only();
    test_bridge_recommendation_reports_unlearned_region();
    test_bridge_recommendation_rejects_out_of_range();
    return 0;
}
