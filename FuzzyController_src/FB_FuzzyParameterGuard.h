/******************************************************************************
 * File    : FB_FuzzyParameterGuard.h
 * Brief   : Bounds, step limits and rollback support for self-tuned parameters.
 ******************************************************************************/
#ifndef FB_FUZZY_PARAMETER_GUARD_H
#define FB_FUZZY_PARAMETER_GUARD_H

#include <stdbool.h>
#include "ssm_std_define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float Ke;
    float Kde;
    float Ku;
    float ErrorWindow;
    float FullPowerErrorRatio;
    float PrecisionErrorRatio;
} FuzzyTunableParameters_t;

typedef struct
{
    float MinKe;
    float MaxKe;
    float MinKde;
    float MaxKde;
    float MinKu;
    float MaxKu;
    float MinErrorWindow;
    float MaxErrorWindow;
    float MinFullPowerErrorRatio;
    float MaxFullPowerErrorRatio;
    float MinPrecisionErrorRatio;
    float MaxPrecisionErrorRatio;
    float MaxRelativeStep;
} FuzzyParameterGuardConfig_t;

typedef struct
{
    FuzzyParameterGuardConfig_t Config;
    FuzzyTunableParameters_t Accepted;
    FuzzyTunableParameters_t Candidate;
    bool HasAccepted;
    bool HasCandidate;
} FB_FuzzyParameterGuard_t;

MY_API void FB_FuzzyParameterGuard_Init(FB_FuzzyParameterGuard_t *fb);
MY_API bool FB_FuzzyParameterGuard_SetConfig(
    FB_FuzzyParameterGuard_t *fb,
    const FuzzyParameterGuardConfig_t *config);
MY_API bool FB_FuzzyParameterGuard_MakeCandidate(
    FB_FuzzyParameterGuard_t *fb,
    const FuzzyTunableParameters_t *current,
    const FuzzyTunableParameters_t *requested,
    FuzzyTunableParameters_t *candidate);
MY_API void FB_FuzzyParameterGuard_Accept(
    FB_FuzzyParameterGuard_t *fb,
    const FuzzyTunableParameters_t *parameters);
MY_API bool FB_FuzzyParameterGuard_Rollback(
    const FB_FuzzyParameterGuard_t *fb,
    FuzzyTunableParameters_t *parameters);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_PARAMETER_GUARD_H */
