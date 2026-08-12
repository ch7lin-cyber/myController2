/******************************************************************************
 * File    : FB_FuzzyController.h
 * Version : V2.4
 * Brief   : IEC61131-3 Style Fuzzy Temperature Controller
 *
 * Execution chain:
 *   SV/PV -> Adaptive Scaling -> dError filter/deadband -> Membership
 *         -> 7x7 Sugeno Rule -> PWM
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

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_CONTROLLER_SAMPLE_TIME_DEFAULT_MS        (20U)
#define FUZZY_CONTROLLER_SAMPLE_TIME_MIN_MS            (1U)
#define FUZZY_CONTROLLER_SAMPLE_TIME_MAX_MS            (6000U)
#define FUZZY_CONTROLLER_DERROR_FILTER_TAU_DEFAULT_S    (0.20f)
#define FUZZY_CONTROLLER_DERROR_FILTER_TAU_MAX_S        (10.0f)
#define FUZZY_CONTROLLER_DERROR_DEADBAND_DEFAULT        (0.20f)
#define FUZZY_CONTROLLER_DERROR_DEADBAND_MAX            (100.0f)
#define FUZZY_INPUT_COUNT                               (7U)

typedef struct
{
    /*
     * Public execution-period configuration.
     * Set once before cyclic operation through FB_FuzzyController_SetSampleTime().
     * Valid range: 1..6000 ms.
     */
    uint32_t SampleTime_ms;

    /* Cached seconds representation used internally by time-based algorithms. */
    float Ts;

    /*
     * Derivative conditioning for quantized temperature sensors.
     * DErrorFilterTau_s = 0 disables LPF.
     * DErrorDeadband_c_per_s = 0 disables the soft deadband.
     */
    float DErrorFilterTau_s;
    float DErrorDeadband_c_per_s;

    bool Enable;
    float OutputMin;
    float OutputMax;
} FuzzyControllerConfig_t;

typedef struct
{
    float SV;
    float PV;
    float Error;

    /* Diagnostics: raw -> LPF -> soft-deadband value used by Fuzzy. */
    float RawDError;
    float FilteredDError;
    float dError;

    float PWM;
    float Centroid;
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
} FB_FuzzyController_t;

MY_API void FB_FuzzyController_Init(FB_FuzzyController_t *fb);

/*
 * Configure the fixed cyclic execution period.
 * Call during initialization/configuration, before normal cyclic Run().
 */
MY_API bool FB_FuzzyController_SetSampleTime(
    FB_FuzzyController_t *fb,
    uint32_t sampleTime_ms);

MY_API uint32_t FB_FuzzyController_GetSampleTime(
    const FB_FuzzyController_t *fb);

/* Configure dError low-pass filter and soft deadband. */
MY_API bool FB_FuzzyController_SetDerivativeFilter(
    FB_FuzzyController_t *fb,
    float filterTau_s,
    float deadband_c_per_s);

/* Execute at the configured controller period, default 20 ms. */
/* Return value is an absolute PWM command, default 0..1000. */
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
