/******************************************************************************
 * File    : FB_FuzzySelfTuningBridge.h
 * Brief   : Non-intrusive bridge between the fuzzy controller and self tuner.
 *
 * Safety rule:
 *   Shadow mode is the default. Candidate parameters are calculated and exposed
 *   for diagnostics, but are NEVER applied automatically. The application must
 *   explicitly call FB_FuzzySelfTuningBridge_ApplyCandidate().
 ******************************************************************************/
#ifndef FB_FUZZY_SELF_TUNING_BRIDGE_H
#define FB_FUZZY_SELF_TUNING_BRIDGE_H

#include <stdbool.h>
#include "ssm_std_define.h"
#include "FB_FuzzyController.h"
#include "FB_FuzzyPerformanceMonitor.h"
#include "FB_FuzzySelfTuner.h"
#include "FB_FuzzyTemperatureProfile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FUZZY_SELF_TUNING_REGION_INVALID   (-1)

typedef struct
{
    bool Enable;
    bool ShadowMode;
    bool AutoStartOnSVChange;
    float SVChangeThreshold_c;
} FuzzySelfTuningBridgeConfig_t;

typedef struct
{
    bool EpisodeActive;
    bool CandidateAvailable;
    bool CandidateApplied;
    bool RollbackRecommended;
    bool ApplyBlockedByScalingMode;
    uint32_t EpisodeCount;
    int16_t ActiveRegion;
    int16_t CandidateRegion;
    float ActiveRegionConfidence;
    float CandidateRegionConfidence;
    FuzzyTunableParameters_t Current;
    FuzzyTunableParameters_t Candidate;
} FuzzySelfTuningBridgeStatus_t;

typedef struct
{
    FuzzySelfTuningBridgeConfig_t Config;
    FuzzySelfTuningBridgeStatus_t Status;
    FB_FuzzyPerformanceMonitor_t Monitor;
    FB_FuzzySelfTuner_t Tuner;
    FB_FuzzyTemperatureProfile_t TemperatureProfile;
    FuzzyScalingConfig_t AppliedScalingConfigBackup;
    float AppliedFullPowerErrorRatioBackup;
    float AppliedPrecisionErrorRatioBackup;
    bool HasApplyBackup;
    float PreviousSV;
    bool Initialized;
} FB_FuzzySelfTuningBridge_t;

MY_API void FB_FuzzySelfTuningBridge_Init(FB_FuzzySelfTuningBridge_t *fb);
MY_API void FB_FuzzySelfTuningBridge_Reset(FB_FuzzySelfTuningBridge_t *fb);
MY_API void FB_FuzzySelfTuningBridge_SetShadowMode(FB_FuzzySelfTuningBridge_t *fb, bool shadowMode);
MY_API bool FB_FuzzySelfTuningBridge_GetControllerParameters(const FB_FuzzyController_t *controller, FuzzyTunableParameters_t *parameters);
MY_API bool FB_FuzzySelfTuningBridge_StartEpisode(FB_FuzzySelfTuningBridge_t *fb, const FB_FuzzyController_t *controller, float sv, float pv, float pwm);
MY_API void FB_FuzzySelfTuningBridge_Run(FB_FuzzySelfTuningBridge_t *fb, const FB_FuzzyController_t *controller, float sv, float pv, float pwm);
MY_API bool FB_FuzzySelfTuningBridge_GetCandidate(const FB_FuzzySelfTuningBridge_t *fb, FuzzyTunableParameters_t *candidate);
MY_API const FuzzyPerformanceMetrics_t *FB_FuzzySelfTuningBridge_GetMetrics(const FB_FuzzySelfTuningBridge_t *fb);
MY_API const FuzzySelfTunerStatus_t *FB_FuzzySelfTuningBridge_GetTunerStatus(const FB_FuzzySelfTuningBridge_t *fb);
MY_API const FuzzyTemperatureRegion_t *FB_FuzzySelfTuningBridge_GetActiveRegion(const FB_FuzzySelfTuningBridge_t *fb);
MY_API const FuzzyTemperatureRegion_t *FB_FuzzySelfTuningBridge_GetCandidateRegion(const FB_FuzzySelfTuningBridge_t *fb);
MY_API bool FB_FuzzySelfTuningBridge_GetRecommendation(const FB_FuzzySelfTuningBridge_t *fb, float sv, FuzzyTemperatureRecommendation_t *recommendation);
MY_API bool FB_FuzzySelfTuningBridge_GetInterpolatedRecommendation(const FB_FuzzySelfTuningBridge_t *fb, float sv, FuzzyTemperatureInterpolatedRecommendation_t *recommendation);

/*
 * Convert an already learned HIGH_CONFIDENCE profile recommendation into a
 * normal guarded Candidate. A fresh completed baseline episode near sv is
 * required. This never changes ShadowMode and never writes controller values.
 */
MY_API bool FB_FuzzySelfTuningBridge_PromoteRecommendationToCandidate(
    FB_FuzzySelfTuningBridge_t *fb,
    const FB_FuzzyController_t *controller,
    float sv,
    bool useInterpolation);

MY_API bool FB_FuzzySelfTuningBridge_RejectCandidate(FB_FuzzySelfTuningBridge_t *fb);
MY_API bool FB_FuzzySelfTuningBridge_ApplyCandidate(FB_FuzzySelfTuningBridge_t *fb, FB_FuzzyController_t *controller);
MY_API bool FB_FuzzySelfTuningBridge_Rollback(FB_FuzzySelfTuningBridge_t *fb, FB_FuzzyController_t *controller);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_SELF_TUNING_BRIDGE_H */
