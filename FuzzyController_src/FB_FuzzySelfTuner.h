/******************************************************************************
 * File    : FB_FuzzySelfTuner.h
 * Brief   : Supervisory self tuner for fuzzy controller parameters.
 ******************************************************************************/
#ifndef FB_FUZZY_SELF_TUNER_H
#define FB_FUZZY_SELF_TUNER_H

#include <stdbool.h>
#include "ssm_std_define.h"
#include "FB_FuzzyPerformanceMonitor.h"
#include "FB_FuzzyParameterGuard.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    FUZZY_TUNER_DISABLED = 0,
    FUZZY_TUNER_IDLE,
    FUZZY_TUNER_EVALUATE,
    FUZZY_TUNER_ADJUST,
    FUZZY_TUNER_VERIFY,
    FUZZY_TUNER_ACCEPT,
    FUZZY_TUNER_ROLLBACK
} FuzzySelfTunerState_e;

typedef struct
{
    bool Enable;
    float TargetOvershoot_c;
    float TargetRiseTime_s;
    float TargetSettlingTime_s;
    float TargetSteadyStateError_c;
    uint16_t MaxZeroCrossCount;

    float WeightOvershoot;
    float WeightRiseTime;
    float WeightSettlingTime;
    float WeightIAE;
    float WeightSteadyStateError;
    float WeightPWMActivity;

    float GainStepUp;
    float GainStepDown;
    float DampingStepUp;
    float ApproachStep;
    float MinimumImprovement;
} FuzzySelfTunerConfig_t;

typedef struct
{
    FuzzySelfTunerState_e State;
    float BaselineCost;
    float CandidateCost;
    bool HasBaseline;
    bool CandidatePending;
    uint32_t AcceptedCount;
    uint32_t RollbackCount;
} FuzzySelfTunerStatus_t;

typedef struct
{
    FuzzySelfTunerConfig_t Config;
    FuzzySelfTunerStatus_t Status;
    FB_FuzzyParameterGuard_t Guard;
    FuzzyTunableParameters_t Baseline;
    FuzzyTunableParameters_t Candidate;
} FB_FuzzySelfTuner_t;

MY_API void FB_FuzzySelfTuner_Init(FB_FuzzySelfTuner_t *fb);
MY_API void FB_FuzzySelfTuner_Reset(FB_FuzzySelfTuner_t *fb);
MY_API bool FB_FuzzySelfTuner_SetConfig(
    FB_FuzzySelfTuner_t *fb,
    const FuzzySelfTunerConfig_t *config);
MY_API float FB_FuzzySelfTuner_CalculateCost(
    const FB_FuzzySelfTuner_t *fb,
    const FuzzyPerformanceMetrics_t *metrics);
MY_API bool FB_FuzzySelfTuner_EvaluateEpisode(
    FB_FuzzySelfTuner_t *fb,
    const FuzzyPerformanceMetrics_t *metrics,
    const FuzzyTunableParameters_t *current,
    FuzzyTunableParameters_t *nextParameters);
MY_API void FB_FuzzySelfTuner_AcceptCurrent(
    FB_FuzzySelfTuner_t *fb,
    const FuzzyTunableParameters_t *parameters,
    float cost);
MY_API bool FB_FuzzySelfTuner_Rollback(
    FB_FuzzySelfTuner_t *fb,
    FuzzyTunableParameters_t *parameters);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_SELF_TUNER_H */
