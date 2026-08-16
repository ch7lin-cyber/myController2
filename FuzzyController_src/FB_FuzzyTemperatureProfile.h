/******************************************************************************
 * File    : FB_FuzzyTemperatureProfile.h
 * Brief   : Temperature-region profile for slow fuzzy self tuning.
 ******************************************************************************/
#ifndef FB_FUZZY_TEMPERATURE_PROFILE_H
#define FB_FUZZY_TEMPERATURE_PROFILE_H

#include <stdbool.h>
#include <stdint.h>
#include "ssm_std_define.h"
#include "FB_FuzzyParameterGuard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FUZZY_TEMP_PROFILE_MAX_REGIONS        (8U)
#define FUZZY_TEMP_PROFILE_DEFAULT_REGIONS    (5U)
#define FUZZY_TEMP_PROFILE_RECOMMEND_MIN_DEFAULT   (0.30f)
#define FUZZY_TEMP_PROFILE_RECOMMEND_HIGH_DEFAULT  (0.70f)

typedef enum
{
    FUZZY_TEMP_RECOMMEND_NONE = 0,
    FUZZY_TEMP_RECOMMEND_EXPERIMENTAL,
    FUZZY_TEMP_RECOMMEND_HIGH_CONFIDENCE
} FuzzyTemperatureRecommendationLevel_e;

typedef struct
{
    int16_t RegionIndex;
    float Confidence;
    FuzzyTemperatureRecommendationLevel_e Level;
    bool HasLearnedParameters;
    FuzzyTunableParameters_t Parameters;
} FuzzyTemperatureRecommendation_t;

typedef struct
{
    float MinTemperature_c;
    float MaxTemperature_c;
    FuzzyTunableParameters_t LearnedParameters;
    uint32_t ObservationCount;
    uint32_t AcceptedCount;
    uint32_t RollbackCount;
    float Confidence;
    bool HasLearnedParameters;
} FuzzyTemperatureRegion_t;

typedef struct
{
    FuzzyTemperatureRegion_t Regions[FUZZY_TEMP_PROFILE_MAX_REGIONS];
    uint8_t RegionCount;
    uint8_t ActiveRegion;
    bool Initialized;
} FB_FuzzyTemperatureProfile_t;

MY_API void FB_FuzzyTemperatureProfile_Init(FB_FuzzyTemperatureProfile_t *fb);
MY_API bool FB_FuzzyTemperatureProfile_SetRegions(
    FB_FuzzyTemperatureProfile_t *fb,
    const FuzzyTemperatureRegion_t *regions,
    uint8_t count);
MY_API int16_t FB_FuzzyTemperatureProfile_FindRegion(
    const FB_FuzzyTemperatureProfile_t *fb,
    float temperature_c);
MY_API bool FB_FuzzyTemperatureProfile_RecordObservation(
    FB_FuzzyTemperatureProfile_t *fb,
    uint8_t regionIndex);
MY_API bool FB_FuzzyTemperatureProfile_RecordAccepted(
    FB_FuzzyTemperatureProfile_t *fb,
    uint8_t regionIndex,
    const FuzzyTunableParameters_t *parameters);
MY_API bool FB_FuzzyTemperatureProfile_RecordRollback(
    FB_FuzzyTemperatureProfile_t *fb,
    uint8_t regionIndex);
MY_API const FuzzyTemperatureRegion_t *FB_FuzzyTemperatureProfile_GetRegion(
    const FB_FuzzyTemperatureProfile_t *fb,
    uint8_t regionIndex);

/*
 * Read-only recommendation query.
 * This function never writes controller parameters and never changes learning
 * state. It only classifies already accepted profile data for diagnostics or
 * application-level decisions.
 *
 * Return value:
 *   true  : temperature belongs to a configured region and thresholds are valid.
 *   false : invalid input, invalid thresholds, or temperature outside all regions.
 *
 * Recommendation Level:
 *   NONE         : no learned parameters, or confidence < minimumConfidence.
 *   EXPERIMENTAL : confidence >= minimumConfidence and < highConfidence.
 *   HIGH         : confidence >= highConfidence.
 */
MY_API bool FB_FuzzyTemperatureProfile_GetRecommendation(
    const FB_FuzzyTemperatureProfile_t *fb,
    float temperature_c,
    float minimumConfidence,
    float highConfidence,
    FuzzyTemperatureRecommendation_t *recommendation);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_TEMPERATURE_PROFILE_H */
