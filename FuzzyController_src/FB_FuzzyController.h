/******************************************************************************
 * File    : FB_FuzzyController.h
 *
 * Version : V2.1
 *
 * Brief:
 *   IEC61131-3 Style Fuzzy Temperature Controller
 *
 * Execution chain:
 *   SV/PV -> Adaptive Scaling -> Membership -> 7x7 Rule -> PWM Output
 *
 ******************************************************************************/

#ifndef FB_FUZZY_CONTROLLER_H
#define FB_FUZZY_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "FB_FuzzyScaling.h"
#include "FB_FuzzyMembership.h"
#include "FB_FuzzyRule.h"
#include "FB_FuzzyDefuzzifier.h"
#include "FB_FuzzyOutputManager.h"

#define FUZZY_CONTROLLER_TS      (0.020f)
#define FUZZY_INPUT_COUNT        (7U)

/*
 * Linguistic index:
 * NB NM NS ZE PS PM PB
 */
typedef enum
{
    FUZZY_NB = 0,
    FUZZY_NM,
    FUZZY_NS,
    FUZZY_ZE,
    FUZZY_PS,
    FUZZY_PM,
    FUZZY_PB
} FuzzyTerm_t;

/* Controller configuration. */
typedef struct
{
    float Ts;
    bool Enable;
    float OutputMin;
    float OutputMax;
} FuzzyControllerConfig_t;

/* Controller runtime state. */
typedef struct
{
    float SV;
    float PV;
    float Error;
    float dError;
    float PWM;
    float Centroid;
    bool initialized;
    bool firstRun;
} FuzzyControllerState_t;

/* Main Function Block. */
typedef struct
{
    FuzzyControllerConfig_t config;
    FuzzyControllerState_t state;

    /* Function Blocks. */
    FB_FuzzyScaling_t scaling;
    FB_FuzzyMembership_t membership;
    FB_FuzzyRule_t ruleEngine;
    FB_FuzzyDefuzzifier_t defuzz;
    FB_FuzzyOutputManager_t output;
} FB_FuzzyController_t;

/* Initialize controller. */
void FB_FuzzyController_Init(
    FB_FuzzyController_t *fb
);

/*
 * Execute controller.
 * Must be called at the configured controller period, default 20 ms.
 *
 * Return value:
 *   PWM command in the configured range, default 0 ~ 1000.
 */
float FB_FuzzyController_Run(
    FB_FuzzyController_t *fb,
    float SV,
    float PV
);

/* Reset controller runtime state. */
void FB_FuzzyController_Reset(
    FB_FuzzyController_t *fb
);

/* Load the default 7x7 heater rule table. */
void FB_FuzzyController_LoadDefaultRule(
    FB_FuzzyController_t *fb
);

/*
 * Update one rule.
 * outputPWM is an absolute PWM command in the range 0 ~ 1000.
 */
bool FB_FuzzyController_SetRule(
    FB_FuzzyController_t *fb,
    uint8_t errorIndex,
    uint8_t dErrorIndex,
    int16_t outputPWM
);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_CONTROLLER_H */