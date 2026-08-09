/******************************************************************************
 * File    : FB_FuzzyScaling.h
 * Version : V2.0
 * Brief   : Auto / Adaptive Scaling Engine
 ******************************************************************************/
#ifndef FB_FUZZY_SCALING_H
#define FB_FUZZY_SCALING_H

#include <stdint.h>
#include <stdbool.h>
#include "ssm_std_define.h"

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_SCALING_DEFAULT_TS              (0.020f)
#define FUZZY_SCALING_DEFAULT_MIN_TEMP        (50.0f)
#define FUZZY_SCALING_DEFAULT_MAX_TEMP        (300.0f)
#define FUZZY_SCALING_DEFAULT_ERROR_WINDOW    (20.0f)
#define FUZZY_SCALING_MIN_ERROR_WINDOW        (2.0f)
#define FUZZY_SCALING_MAX_ERROR_WINDOW        (100.0f)
#define FUZZY_SCALING_MIN_KE                  (0.005f)
#define FUZZY_SCALING_MAX_KE                  (0.500f)
#define FUZZY_SCALING_MIN_KDE                 (0.001f)
#define FUZZY_SCALING_MAX_KDE                 (0.500f)
#define FUZZY_SCALING_MIN_KU                  (0.10f)
#define FUZZY_SCALING_MAX_KU                  (1.50f)
#define FUZZY_SCALING_MIN_DYNAMIC_FACTOR      (0.50f)
#define FUZZY_SCALING_MAX_DYNAMIC_FACTOR      (1.50f)
#define FUZZY_SCALING_DEFAULT_DYNAMIC_GAIN    (0.020f)
#define FUZZY_SCALING_DEFAULT_MAX_PV_RATE     (20.0f)
#define FUZZY_SCALING_DEFAULT_KU_SLEW_RATE    (2.0f)

typedef struct
{
    float Ts;
    float MinTemperature;
    float MaxTemperature;
    float BaseErrorWindow;
    float MinErrorWindow;
    float MaxErrorWindow;
    float MinKe;
    float MaxKe;
    float MinKde;
    float MaxKde;
    float MinKu;
    float MaxKu;
    float DynamicGain;
    float MaxPVRate;
    float KuSlewRate;
    bool AutoScalingEnable;
    bool AdaptiveEnable;
} FuzzyScalingConfig_t;

typedef struct
{
    float Ke;
    float Kde;
    float Ku;
    float TargetKe;
    float TargetKde;
    float TargetKu;
    float ErrorWindow;
    float TargetErrorWindow;
    float Error;
    float PreviousError;
    float dError;
    float PV;
    float PreviousPV;
    float PVRate;
    float DynamicFactor;
    float NormalizedError;
    float NormalizedDError;
    bool Initialized;
} FuzzyScalingState_t;

typedef struct
{
    FuzzyScalingConfig_t Config;
    FuzzyScalingState_t State;
} FB_FuzzyScaling_t;

MY_API void FB_FuzzyScaling_Init(FB_FuzzyScaling_t *fb);
MY_API void FB_FuzzyScaling_Reset(FB_FuzzyScaling_t *fb);

MY_API void FB_FuzzyScaling_Run(
    FB_FuzzyScaling_t *fb,
    float sv,
    float pv);

MY_API float FB_FuzzyScaling_CalculateErrorWindow(
    FB_FuzzyScaling_t *fb,
    float sv,
    float pv);

MY_API float FB_FuzzyScaling_CalculateDynamicFactor(
    FB_FuzzyScaling_t *fb,
    float pvRate);

MY_API float FB_FuzzyScaling_CalculateKe(FB_FuzzyScaling_t *fb);
MY_API float FB_FuzzyScaling_CalculateKde(FB_FuzzyScaling_t *fb);
MY_API float FB_FuzzyScaling_CalculateKu(FB_FuzzyScaling_t *fb);

MY_API float FB_FuzzyScaling_Slew(
    float current,
    float target,
    float rate,
    float Ts);

MY_API float FB_FuzzyScaling_NormalizeError(
    float error,
    float ke);

MY_API float FB_FuzzyScaling_NormalizeDError(
    float dError,
    float kde);

MY_API bool FB_FuzzyScaling_SetConfig(
    FB_FuzzyScaling_t *fb,
    const FuzzyScalingConfig_t *config);

MY_API bool FB_FuzzyScaling_GetConfig(
    const FB_FuzzyScaling_t *fb,
    FuzzyScalingConfig_t *config);

MY_API bool FB_FuzzyScaling_SetKe(FB_FuzzyScaling_t *fb, float ke);
MY_API bool FB_FuzzyScaling_SetKde(FB_FuzzyScaling_t *fb, float kde);
MY_API bool FB_FuzzyScaling_SetKu(FB_FuzzyScaling_t *fb, float ku);
MY_API bool FB_FuzzyScaling_SetErrorWindow(FB_FuzzyScaling_t *fb, float window);

MY_API void FB_FuzzyScaling_EnableAuto(FB_FuzzyScaling_t *fb);
MY_API void FB_FuzzyScaling_DisableAuto(FB_FuzzyScaling_t *fb);
MY_API void FB_FuzzyScaling_EnableAdaptive(FB_FuzzyScaling_t *fb);
MY_API void FB_FuzzyScaling_DisableAdaptive(FB_FuzzyScaling_t *fb);

//------------------------------------------------------------------------------------//
// C++ compatibility
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------------//

#endif /* FB_FUZZY_SCALING_H */
