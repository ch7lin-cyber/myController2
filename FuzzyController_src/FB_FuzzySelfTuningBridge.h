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

#ifdef __cplusplus
extern "C" {
#endif

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
    FuzzyTunableParameters_t Current;
    FuzzyTunableParameters_t Candidate;
} FuzzySelfTuningBridgeStatus_t;

typedef struct
{
    FuzzySelfTuningBridgeConfig_t Config;
    FuzzySelfTuningBridgeStatus_t Status;
    FB_FuzzyPerformanceMonitor_t Monitor;
    FB_FuzzySelfTuner_t Tuner;

    /* Exact rollback snapshot captured only when a candidate is explicitly applied. */
    FuzzyScalingConfig_t AppliedScalingConfigBackup;
    float AppliedFullPowerErrorRatioBackup;
    float AppliedPrecisionErrorRatioBackup;
    bool HasApplyBackup;

    float PreviousSV;
    bool Initialized;
} FB_FuzzySelfTuningBridge_t;

MY_API void FB_FuzzySelfTuningBridge_Init(FB_FuzzySelfTuningBridge_t *fb);
MY_API void FB_FuzzySelfTuningBridge_Reset(FB_FuzzySelfTuningBridge_t *fb);

MY_API void FB_FuzzySelfTuningBridge_SetShadowMode(
    FB_FuzzySelfTuningBridge_t *fb,
    bool shadowMode);

MY_API bool FB_FuzzySelfTuningBridge_GetControllerParameters(
    const FB_FuzzyController_t *controller,
    FuzzyTunableParameters_t *parameters);

MY_API bool FB_FuzzySelfTuningBridge_StartEpisode(
    FB_FuzzySelfTuningBridge_t *fb,
    const FB_FuzzyController_t *controller,
    float sv,
    float pv,
    float pwm);

MY_API void FB_FuzzySelfTuningBridge_Run(
    FB_FuzzySelfTuningBridge_t *fb,
    const FB_FuzzyController_t *controller,
    float sv,
    float pv,
    float pwm);

MY_API bool FB_FuzzySelfTuningBridge_GetCandidate(
    const FB_FuzzySelfTuningBridge_t *fb,
    FuzzyTunableParameters_t *candidate);

MY_API const FuzzyPerformanceMetrics_t *FB_FuzzySelfTuningBridge_GetMetrics(
    const FB_FuzzySelfTuningBridge_t *fb);

MY_API const FuzzySelfTunerStatus_t *FB_FuzzySelfTuningBridge_GetTunerStatus(
    const FB_FuzzySelfTuningBridge_t *fb);

/* Reject a suggested candidate that has not been applied. */
MY_API bool FB_FuzzySelfTuningBridge_RejectCandidate(
    FB_FuzzySelfTuningBridge_t *fb);

/*
 * Explicit write API. Never called automatically by Run().
 * ShadowMode must first be disabled by the application.
 * In the normal branch4 architecture Auto Scaling remains enabled: the candidate
 * is converted to persistent slow trim multipliers instead of overwriting the
 * fast adaptive targets directly.
 */
MY_API bool FB_FuzzySelfTuningBridge_ApplyCandidate(
    FB_FuzzySelfTuningBridge_t *fb,
    FB_FuzzyController_t *controller);

/* Explicit physical rollback. Never executed automatically after verification. */
MY_API bool FB_FuzzySelfTuningBridge_Rollback(
    FB_FuzzySelfTuningBridge_t *fb,
    FB_FuzzyController_t *controller);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_SELF_TUNING_BRIDGE_H */
