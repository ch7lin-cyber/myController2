/******************************************************************************
 * File    : FB_FuzzySelfTuningRecommendation.c
 * Brief   : Read-only learned-profile recommendation wrapper.
 ******************************************************************************/
#include "FB_FuzzySelfTuningBridge.h"

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
