/******************************************************************************
 * File    : FB_FuzzyController.h
 * Version : V2.2
 * Brief   : IEC61131-3 Style Fuzzy Temperature Controller
 *
 * Execution chain:
 *   SV/PV -> Adaptive Scaling -> Membership -> 7x7 Sugeno Rule -> PWM
 *
 * NOTE:
 *   The active controller uses zero-order Sugeno/singleton inference.
 *   Rule outputs are absolute PWM commands (0..1000). A second Mamdani
 *   defuzzification stage must NOT be inserted into this active path.
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
#include "FB_FuzzyOutputManager.h"

#define FUZZY_CONTROLLER_TS      (0.020f)
#define FUZZY_INPUT_COUNT        (7U)

typedef struct
{
    float Ts;
    bool Enable;
    float OutputMin;
    float OutputMax;
} FuzzyControllerConfig_t;

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

typedef struct
{
    FuzzyControllerConfig_t config;
    FuzzyControllerState_t state;
    FB_FuzzyScaling_t scaling;
    FB_FuzzyMembership_t membership;
    FB_FuzzyRule_t ruleEngine;
    FB_FuzzyOutputManager_t output;
} FB_FuzzyController_t;

void FB_FuzzyController_Init(FB_FuzzyController_t *fb);

/* Execute at the configured controller period, default 20 ms. */
/* Return value is an absolute PWM command, default 0..1000. */
float FB_FuzzyController_Run(FB_FuzzyController_t *fb, float SV, float PV);

void FB_FuzzyController_Reset(FB_FuzzyController_t *fb);
void FB_FuzzyController_LoadDefaultRule(FB_FuzzyController_t *fb);

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
