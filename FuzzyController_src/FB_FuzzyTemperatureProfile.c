/******************************************************************************
 * File    : FB_FuzzyTemperatureProfile.c
 * Brief   : Temperature-region profile for slow fuzzy self tuning.
 ******************************************************************************/
#include "FB_FuzzyTemperatureProfile.h"

static float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void clear_parameters(FuzzyTunableParameters_t *parameters)
{
    if (parameters == (FuzzyTunableParameters_t *)0) return;

    parameters->Ke = 0.0f;
    parameters->Kde = 0.0f;
    parameters->Ku = 0.0f;
    parameters->ErrorWindow = 0.0f;
    parameters->FullPowerErrorRatio = 0.0f;
    parameters->PrecisionErrorRatio = 0.0f;
}

static FuzzyTemperatureRecommendationLevel_e classify_confidence(
    float confidence,
    bool hasLearnedParameters,
    float minimumConfidence,
    float highConfidence)
{
    if (!hasLearnedParameters || (confidence < minimumConfidence))
    {
        return FUZZY_TEMP_RECOMMEND_NONE;
    }

    if (confidence < highConfidence)
    {
        return FUZZY_TEMP_RECOMMEND_EXPERIMENTAL;
    }

    return FUZZY_TEMP_RECOMMEND_HIGH_CONFIDENCE;
}

static float calculate_confidence(const FuzzyTemperatureRegion_t *region)
{
    float observations;
    float acceptedRatio;
    float experience;
    float rollbackPenalty;

    if ((region == (const FuzzyTemperatureRegion_t *)0) ||
        (region->ObservationCount == 0U))
    {
        return 0.0f;
    }

    observations = (float)region->ObservationCount;
    acceptedRatio = (float)region->AcceptedCount / observations;
    experience = observations / 10.0f;
    if (experience > 1.0f) experience = 1.0f;

    rollbackPenalty = (float)region->RollbackCount / observations;

    return clamp01((0.65f * acceptedRatio + 0.35f * experience) *
                   (1.0f - 0.50f * rollbackPenalty));
}

static void clear_region(FuzzyTemperatureRegion_t *region)
{
    if (region == (FuzzyTemperatureRegion_t *)0) return;

    region->MinTemperature_c = 0.0f;
    region->MaxTemperature_c = 0.0f;
    clear_parameters(&region->LearnedParameters);
    region->ObservationCount = 0U;
    region->AcceptedCount = 0U;
    region->RollbackCount = 0U;
    region->Confidence = 0.0f;
    region->HasLearnedParameters = false;
}

static float region_center(const FuzzyTemperatureRegion_t *region)
{
    return 0.5f * (region->MinTemperature_c + region->MaxTemperature_c);
}

static float lerpf_local(float lower, float upper, float blend)
{
    return lower + (upper - lower) * blend;
}

static void interpolate_parameters(
    const FuzzyTunableParameters_t *lower,
    const FuzzyTunableParameters_t *upper,
    float blend,
    FuzzyTunableParameters_t *output)
{
    if ((lower == (const FuzzyTunableParameters_t *)0) ||
        (upper == (const FuzzyTunableParameters_t *)0) ||
        (output == (FuzzyTunableParameters_t *)0))
    {
        return;
    }

    output->Ke = lerpf_local(lower->Ke, upper->Ke, blend);
    output->Kde = lerpf_local(lower->Kde, upper->Kde, blend);
    output->Ku = lerpf_local(lower->Ku, upper->Ku, blend);
    output->ErrorWindow = lerpf_local(lower->ErrorWindow, upper->ErrorWindow, blend);
    output->FullPowerErrorRatio = lerpf_local(
        lower->FullPowerErrorRatio,
        upper->FullPowerErrorRatio,
        blend);
    output->PrecisionErrorRatio = lerpf_local(
        lower->PrecisionErrorRatio,
        upper->PrecisionErrorRatio,
        blend);
}

void FB_FuzzyTemperatureProfile_Init(FB_FuzzyTemperatureProfile_t *fb)
{
    static const float boundaries[FUZZY_TEMP_PROFILE_DEFAULT_REGIONS + 1U] =
    {
        50.0f, 80.0f, 120.0f, 160.0f, 200.0f, 250.0f
    };
    uint8_t i;

    if (fb == (FB_FuzzyTemperatureProfile_t *)0) return;

    for (i = 0U; i < FUZZY_TEMP_PROFILE_MAX_REGIONS; ++i)
    {
        clear_region(&fb->Regions[i]);
    }

    fb->RegionCount = FUZZY_TEMP_PROFILE_DEFAULT_REGIONS;
    fb->ActiveRegion = 0U;

    for (i = 0U; i < fb->RegionCount; ++i)
    {
        fb->Regions[i].MinTemperature_c = boundaries[i];
        fb->Regions[i].MaxTemperature_c = boundaries[i + 1U];
    }

    fb->Initialized = true;
}

bool FB_FuzzyTemperatureProfile_SetRegions(
    FB_FuzzyTemperatureProfile_t *fb,
    const FuzzyTemperatureRegion_t *regions,
    uint8_t count)
{
    uint8_t i;

    if ((fb == (FB_FuzzyTemperatureProfile_t *)0) ||
        (regions == (const FuzzyTemperatureRegion_t *)0) ||
        (count == 0U) ||
        (count > FUZZY_TEMP_PROFILE_MAX_REGIONS))
    {
        return false;
    }

    for (i = 0U; i < count; ++i)
    {
        if (regions[i].MinTemperature_c >= regions[i].MaxTemperature_c)
        {
            return false;
        }

        if ((i > 0U) &&
            (regions[i - 1U].MaxTemperature_c > regions[i].MinTemperature_c))
        {
            return false;
        }
    }

    for (i = 0U; i < FUZZY_TEMP_PROFILE_MAX_REGIONS; ++i)
    {
        clear_region(&fb->Regions[i]);
    }

    for (i = 0U; i < count; ++i)
    {
        fb->Regions[i] = regions[i];
        fb->Regions[i].Confidence = calculate_confidence(&fb->Regions[i]);
    }

    fb->RegionCount = count;
    fb->ActiveRegion = 0U;
    fb->Initialized = true;
    return true;
}

int16_t FB_FuzzyTemperatureProfile_FindRegion(
    const FB_FuzzyTemperatureProfile_t *fb,
    float temperature_c)
{
    uint8_t i;

    if ((fb == (const FB_FuzzyTemperatureProfile_t *)0) || !fb->Initialized)
    {
        return -1;
    }

    for (i = 0U; i < fb->RegionCount; ++i)
    {
        if ((temperature_c >= fb->Regions[i].MinTemperature_c) &&
            ((temperature_c < fb->Regions[i].MaxTemperature_c) ||
             ((i == (uint8_t)(fb->RegionCount - 1U)) &&
              (temperature_c <= fb->Regions[i].MaxTemperature_c))))
        {
            return (int16_t)i;
        }
    }

    return -1;
}

bool FB_FuzzyTemperatureProfile_RecordObservation(
    FB_FuzzyTemperatureProfile_t *fb,
    uint8_t regionIndex)
{
    FuzzyTemperatureRegion_t *region;

    if ((fb == (FB_FuzzyTemperatureProfile_t *)0) ||
        (regionIndex >= fb->RegionCount))
    {
        return false;
    }

    region = &fb->Regions[regionIndex];
    if (region->ObservationCount < UINT32_MAX)
    {
        region->ObservationCount++;
    }
    region->Confidence = calculate_confidence(region);
    fb->ActiveRegion = regionIndex;
    return true;
}

bool FB_FuzzyTemperatureProfile_RecordAccepted(
    FB_FuzzyTemperatureProfile_t *fb,
    uint8_t regionIndex,
    const FuzzyTunableParameters_t *parameters)
{
    FuzzyTemperatureRegion_t *region;

    if ((fb == (FB_FuzzyTemperatureProfile_t *)0) ||
        (parameters == (const FuzzyTunableParameters_t *)0) ||
        (regionIndex >= fb->RegionCount))
    {
        return false;
    }

    region = &fb->Regions[regionIndex];
    region->LearnedParameters = *parameters;
    region->HasLearnedParameters = true;
    if (region->AcceptedCount < UINT32_MAX)
    {
        region->AcceptedCount++;
    }
    region->Confidence = calculate_confidence(region);
    fb->ActiveRegion = regionIndex;
    return true;
}

bool FB_FuzzyTemperatureProfile_RecordRollback(
    FB_FuzzyTemperatureProfile_t *fb,
    uint8_t regionIndex)
{
    FuzzyTemperatureRegion_t *region;

    if ((fb == (FB_FuzzyTemperatureProfile_t *)0) ||
        (regionIndex >= fb->RegionCount))
    {
        return false;
    }

    region = &fb->Regions[regionIndex];
    if (region->RollbackCount < UINT32_MAX)
    {
        region->RollbackCount++;
    }
    region->Confidence = calculate_confidence(region);
    fb->ActiveRegion = regionIndex;
    return true;
}

const FuzzyTemperatureRegion_t *FB_FuzzyTemperatureProfile_GetRegion(
    const FB_FuzzyTemperatureProfile_t *fb,
    uint8_t regionIndex)
{
    if ((fb == (const FB_FuzzyTemperatureProfile_t *)0) ||
        (regionIndex >= fb->RegionCount))
    {
        return (const FuzzyTemperatureRegion_t *)0;
    }

    return &fb->Regions[regionIndex];
}

bool FB_FuzzyTemperatureProfile_GetRecommendation(
    const FB_FuzzyTemperatureProfile_t *fb,
    float temperature_c,
    float minimumConfidence,
    float highConfidence,
    FuzzyTemperatureRecommendation_t *recommendation)
{
    int16_t regionIndex;
    const FuzzyTemperatureRegion_t *region;

    if ((fb == (const FB_FuzzyTemperatureProfile_t *)0) ||
        (recommendation == (FuzzyTemperatureRecommendation_t *)0) ||
        (minimumConfidence < 0.0f) ||
        (highConfidence > 1.0f) ||
        (minimumConfidence > highConfidence))
    {
        return false;
    }

    recommendation->RegionIndex = -1;
    recommendation->Confidence = 0.0f;
    recommendation->Level = FUZZY_TEMP_RECOMMEND_NONE;
    recommendation->HasLearnedParameters = false;
    clear_parameters(&recommendation->Parameters);

    regionIndex = FB_FuzzyTemperatureProfile_FindRegion(fb, temperature_c);
    if (regionIndex < 0)
    {
        return false;
    }

    region = FB_FuzzyTemperatureProfile_GetRegion(fb, (uint8_t)regionIndex);
    if (region == (const FuzzyTemperatureRegion_t *)0)
    {
        return false;
    }

    recommendation->RegionIndex = regionIndex;
    recommendation->Confidence = region->Confidence;
    recommendation->HasLearnedParameters = region->HasLearnedParameters;

    if (!region->HasLearnedParameters)
    {
        return true;
    }

    recommendation->Parameters = region->LearnedParameters;
    recommendation->Level = classify_confidence(
        region->Confidence,
        true,
        minimumConfidence,
        highConfidence);

    return true;
}

bool FB_FuzzyTemperatureProfile_GetInterpolatedRecommendation(
    const FB_FuzzyTemperatureProfile_t *fb,
    float temperature_c,
    float minimumConfidence,
    float highConfidence,
    FuzzyTemperatureInterpolatedRecommendation_t *recommendation)
{
    FuzzyTemperatureRecommendation_t direct;
    uint8_t i;

    if ((fb == (const FB_FuzzyTemperatureProfile_t *)0) ||
        (recommendation == (FuzzyTemperatureInterpolatedRecommendation_t *)0))
    {
        return false;
    }

    if (!FB_FuzzyTemperatureProfile_GetRecommendation(
            fb,
            temperature_c,
            minimumConfidence,
            highConfidence,
            &direct))
    {
        return false;
    }

    recommendation->Recommendation = direct;
    recommendation->LowerRegionIndex = direct.RegionIndex;
    recommendation->UpperRegionIndex = direct.RegionIndex;
    recommendation->BlendFactor = 0.0f;
    recommendation->Interpolated = false;

    if (fb->RegionCount < 2U)
    {
        return true;
    }

    for (i = 0U; i < (uint8_t)(fb->RegionCount - 1U); ++i)
    {
        const FuzzyTemperatureRegion_t *lower = &fb->Regions[i];
        const FuzzyTemperatureRegion_t *upper = &fb->Regions[i + 1U];
        float lowerCenter = region_center(lower);
        float upperCenter = region_center(upper);
        float blend;
        float confidence;

        if ((temperature_c <= lowerCenter) || (temperature_c >= upperCenter))
        {
            continue;
        }

        if (!lower->HasLearnedParameters || !upper->HasLearnedParameters)
        {
            return true;
        }

        if (upperCenter <= lowerCenter)
        {
            return true;
        }

        blend = (temperature_c - lowerCenter) / (upperCenter - lowerCenter);
        blend = clamp01(blend);

        interpolate_parameters(
            &lower->LearnedParameters,
            &upper->LearnedParameters,
            blend,
            &recommendation->Recommendation.Parameters);

        confidence = (lower->Confidence < upper->Confidence) ?
                     lower->Confidence : upper->Confidence;

        recommendation->Recommendation.Confidence = confidence;
        recommendation->Recommendation.HasLearnedParameters = true;
        recommendation->Recommendation.Level = classify_confidence(
            confidence,
            true,
            minimumConfidence,
            highConfidence);
        recommendation->LowerRegionIndex = (int16_t)i;
        recommendation->UpperRegionIndex = (int16_t)(i + 1U);
        recommendation->BlendFactor = blend;
        recommendation->Interpolated = true;
        return true;
    }

    return true;
}
