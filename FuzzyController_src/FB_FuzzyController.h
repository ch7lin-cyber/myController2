/******************************************************************************
 * File    : FB_FuzzyController.h
 * Version : V2.7
 * Brief   : IEC61131-3 Style Fuzzy Temperature Controller
 *
 * Execution chain:
 *   SV/PV -> Adaptive Scaling -> derivative-on-PV filter/deadband -> Membership
 *         -> 7x7 Sugeno Rule -> Output Manager / Hybrid FF Output
 *         -> optional large-error boost override
 *
 * NOTE:
 *   The active controller uses zero-order Sugeno/singleton inference.
 *   Rule outputs are absolute PWM commands (0..1000). A second Mamdani
 *   defuzzification stage must NOT be inserted into this active path.
 ******************************************************************************/
#ifndef FB_FUZZY_CONTROLLER_H
#define FB_FUZZY_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "ssm_std_define.h"
#include "FB_FuzzyScaling.h"
#include "FB_FuzzyMembership.h"
#include "FB_FuzzyRule.h"
#include "FB_FuzzyOutputManager.h"
#include "FB_FuzzyHybridOutput.h"

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_CONTROLLER_SAMPLE_TIME_DEFAULT_MS        (20U)
#define FUZZY_CONTROLLER_SAMPLE_TIME_MIN_MS            (1U)
#define FUZZY_CONTROLLER_SAMPLE_TIME_MAX_MS            (6000U)
#define FUZZY_CONTROLLER_DERROR_FILTER_TAU_DEFAULT_S    (0.50f)
#define FUZZY_CONTROLLER_DERROR_FILTER_TAU_MAX_S        (10.0f)
#define FUZZY_CONTROLLER_DERROR_DEADBAND_DEFAULT        (0.20f)
#define FUZZY_CONTROLLER_DERROR_DEADBAND_MAX            (100.0f)
#define FUZZY_CONTROLLER_BOOST_ENTER_DEFAULT_C          (20.0f)
#define FUZZY_CONTROLLER_BOOST_EXIT_DEFAULT_C           (18.0f)
#define FUZZY_INPUT_COUNT                               (7U)

typedef struct
{
    uint32_t SampleTime_ms;
    float Ts;

    /* Derivative conditioning for quantized temperature sensors. */
    float DErrorFilterTau_s;
    float DErrorDeadband_c_per_s;

    /* Large positive error boost with hysteresis. */
    bool EnableBoost;
    float BoostEnterError_c;
    float BoostExitError_c;

    /* false = legacy absolute PWM path, true = FF + fuzzy correction path. */
    bool UseHybridOutput;

    bool Enable;
    float OutputMin;
    float OutputMax;
} FuzzyControllerConfig_t;

typedef struct
{
    float SV;
    float PV;
    float Error;

    /* Diagnostics: raw derivative-on-PV -> LPF -> soft-deadband value used by Fuzzy. */
    float RawDError;
    float FilteredDError;
    float dError;

    float PWM;
    float Centroid;
    bool BoostActive;
    bool initialized;
    bool firstRun;
} FuzzyControllerState_t;

typedef struct
{
    FuzzyControllerConfig_t config;
    FuzzyControllerState_t state;
    FB_FuzzyScaling_t scaling;
    FB_FuzzyMembership_t membership;
    FB_FuzzyRule_t ruleEngine;
    FB_FuzzyOutputManager_t output;
    FB_FuzzyHybridOutput_t hybridOutput;
} FB_FuzzyController_t;

MY_API void FB_FuzzyController_Init(FB_FuzzyController_t *fb);

MY_API bool FB_FuzzyController_SetSampleTime(
    FB_FuzzyController_t *fb,
    uint32_t sampleTime_ms);

MY_API uint32_t FB_FuzzyController_GetSampleTime(
    const FB_FuzzyController_t *fb);

MY_API bool FB_FuzzyController_SetDerivativeFilter(
    FB_FuzzyController_t *fb,
    float filterTau_s,
    float deadband_c_per_s);

MY_API bool FB_FuzzyController_SetBoostConfig(
    FB_FuzzyController_t *fb,
    bool enable,
    float enterError_c,
    float exitError_c);

/*
 * Enable/disable the hybrid output path.
 * Default is false for backward compatibility.
 */
MY_API void FB_FuzzyController_EnableHybridOutput(
    FB_FuzzyController_t *fb,
    bool enable);

/* Load the identified heater FF map used by branch3 experiments. */
MY_API void FB_FuzzyController_LoadIdentifiedFeedForward(
    FB_FuzzyController_t *fb);

MY_API float FB_FuzzyController_Run(FB_FuzzyController_t *fb, float SV, float PV);

MY_API void FB_FuzzyController_Reset(FB_FuzzyController_t *fb);
MY_API void FB_FuzzyController_LoadDefaultRule(FB_FuzzyController_t *fb);

MY_API bool FB_FuzzyController_SetRule(
    FB_FuzzyController_t *fb,
    uint8_t errorIndex,
    uint8_t dErrorIndex,
    int16_t outputPWM
);

//------------------------------------------------------------------------------------//
// C++ compatibility
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------------//

#endif /* FB_FUZZY_CONTROLLER_H */
