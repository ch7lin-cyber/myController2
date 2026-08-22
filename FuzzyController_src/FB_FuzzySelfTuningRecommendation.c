/******************************************************************************
 * File    : FB_FuzzySelfTuningRecommendation.c
 * Brief   : Read-only learned-profile recommendation wrappers and promotion.
 ******************************************************************************/
#include "FB_FuzzySelfTuningBridge.h"

static float absf_local(float value)
{
    return (value >= 0.0f) ? value : -value;
}

bool FB_FuzzySelfTuningBridge_GetRecommendation(
    const FB_FuzzySelfTuningBridge_t *fb,
    float sv,
    FuzzyTemperatureRecommendation_t *recommendation)
{
    if ((fb == (const FB_FuzzySelfTuningBridge_t *)0) ||
        (recommendation == (FuzzyTemperatureRecommendation_t *)0))
    {
        return false;
    }

    return FB_FuzzyTemperatureProfile_GetRecommendation(
        &fb->TemperatureProfile,
        sv,
        FUZZY_TEMP_PROFILE_RECOMMEND_MIN_DEFAULT,
        FUZZY_TEMP_PROFILE_RECOMMEND_HIGH_DEFAULT,
        recommendation);
}

bool FB_FuzzySelfTuningBridge_GetInterpolatedRecommendation(
    const FB_FuzzySelfTuningBridge_t *fb,
    float sv,
    FuzzyTemperatureInterpolatedRecommendation_t *recommendation)
{
    if ((fb == (const FB_FuzzySelfTuningBridge_t *)0) ||
        (recommendation == (FuzzyTemperatureInterpolatedRecommendation_t *)0))
    {
        return false;
    }

    return FB_FuzzyTemperatureProfile_GetInterpolatedRecommendation(
        &fb->TemperatureProfile,
        sv,
        FUZZY_TEMP_PROFILE_RECOMMEND_MIN_DEFAULT,
        FUZZY_TEMP_PROFILE_RECOMMEND_HIGH_DEFAULT,
        recommendation);
}

bool FB_FuzzySelfTuningBridge_PromoteRecommendationToCandidate(
    FB_FuzzySelfTuningBridge_t *fb,
    const FB_FuzzyController_t *controller,
    float sv,
    bool useInterpolation)
{
    const FuzzyPerformanceMetrics_t *metrics;
    FuzzyTemperatureRecommendation_t direct;
    FuzzyTemperatureInterpolatedRecommendation_t smooth;
    FuzzyTunableParameters_t current;
    FuzzyTunableParameters_t requested;
    FuzzyTunableParameters_t guardedCandidate;
    int16_t regionIndex;
    float confidence;

    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (const FB_FuzzyController_t *)0) ||
        !fb->Initialized || !fb->Config.Enable ||
        fb->Status.EpisodeActive ||
        fb->Status.CandidateAvailable ||
        fb->Status.CandidateApplied ||
        fb->Status.RollbackRecommended ||
        fb->Tuner.Status.CandidatePending)
    {
        return false;
    }

    metrics = FB_FuzzyPerformanceMonitor_GetMetrics(&fb->Monitor);
    if ((metrics == (const FuzzyPerformanceMetrics_t *)0) || !metrics->Complete)
    {
        return false;
    }

    /* A historical recommendation may only be promoted against a fresh,
       comparable operating point near the requested SV. */
    if (absf_local(metrics->TargetSV - sv) >
        fb->Tuner.Config.VerificationTargetTolerance_c)
    {
        return false;
    }

    if (useInterpolation)
    {
        if (!FB_FuzzySelfTuningBridge_GetInterpolatedRecommendation(fb, sv, &smooth) ||
            !smooth.Recommendation.HasLearnedParameters ||
            (smooth.Recommendation.Level != FUZZY_TEMP_RECOMMEND_HIGH_CONFIDENCE))
        {
            return false;
        }
        requested = smooth.Recommendation.Parameters;
        regionIndex = smooth.Recommendation.RegionIndex;
        confidence = smooth.Recommendation.Confidence;
    }
    else
    {
        if (!FB_FuzzySelfTuningBridge_GetRecommendation(fb, sv, &direct) ||
            !direct.HasLearnedParameters ||
            (direct.Level != FUZZY_TEMP_RECOMMEND_HIGH_CONFIDENCE))
        {
            return false;
        }
        requested = direct.Parameters;
        regionIndex = direct.RegionIndex;
        confidence = direct.Confidence;
    }

    if ((regionIndex < 0) ||
        !FB_FuzzySelfTuningBridge_GetControllerParameters(controller, &current))
    {
        return false;
    }

    if (!FB_FuzzySelfTuner_PrepareExternalCandidate(
            &fb->Tuner,
            metrics,
            &current,
            &requested,
            &guardedCandidate))
    {
        return false;
    }

    fb->Status.Current = current;
    fb->Status.Candidate = guardedCandidate;
    fb->Status.CandidateAvailable = true;
    fb->Status.CandidateApplied = false;
    fb->Status.RollbackRecommended = false;
    fb->Status.CandidateRegion = regionIndex;
    fb->Status.CandidateRegionConfidence = confidence;

    /* Intentionally do not change fb->Config.ShadowMode here. */
    return true;
}
