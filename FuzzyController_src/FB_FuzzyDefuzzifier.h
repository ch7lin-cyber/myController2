/******************************************************************************
 * File    : FB_FuzzyDefuzzifier.h
 * Version : V2.0
 * Brief   : Mamdani Fuzzy Defuzzification Engine
 ******************************************************************************/
#ifndef FB_FUZZY_DEFUZZIFIER_H
#define FB_FUZZY_DEFUZZIFIER_H

#include <stdint.h>
#include <stdbool.h>
#include "ssm_std_define.h"

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_DEFUZZ_OUTPUT_MF_COUNT          (7U)
#define FUZZY_DEFUZZ_DEFAULT_OUTPUT_MIN       (0.0f)
#define FUZZY_DEFUZZ_DEFAULT_OUTPUT_MAX       (1000.0f)
#define FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS  (101U)
#define FUZZY_DEFUZZ_DEFAULT_SLEW_RATE        (5000.0f)
#define FUZZY_DEFUZZ_NORMALIZED_MIN           (-1.0f)
#define FUZZY_DEFUZZ_NORMALIZED_MAX           (1.0f)

typedef struct
{
    float Center;
    float Left;
    float Right;
    float Peak;
} FuzzyOutputMF_t;

typedef struct
{
    float OutputMin;
    float OutputMax;
    uint16_t CentroidPoints;
    float OutputSlewRate;
    bool OutputScalingEnable;
    bool OutputSlewEnable;
    FuzzyOutputMF_t MF[FUZZY_DEFUZZ_OUTPUT_MF_COUNT];
} FuzzyDefuzzifierConfig_t;

typedef struct
{
    float Activation[FUZZY_DEFUZZ_OUTPUT_MF_COUNT];
    float Aggregated[FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS];
    float Centroid;
    float RawOutput;
    float ScaledOutput;
    float Output;
    float Ku;
    float TargetOutput;
    float PreviousOutput;
    float Numerator;
    float Denominator;
} FuzzyDefuzzifierState_t;

typedef struct
{
    FuzzyDefuzzifierConfig_t Config;
    FuzzyDefuzzifierState_t State;
} FB_FuzzyDefuzzifier_t;

MY_API void FB_FuzzyDefuzzifier_Init(FB_FuzzyDefuzzifier_t *fb);
MY_API void FB_FuzzyDefuzzifier_Reset(FB_FuzzyDefuzzifier_t *fb);
MY_API void FB_FuzzyDefuzzifier_Clear(FB_FuzzyDefuzzifier_t *fb);

MY_API void FB_FuzzyDefuzzifier_ApplyRule(
    FB_FuzzyDefuzzifier_t *fb,
    uint8_t outputMF,
    float strength);

MY_API void FB_FuzzyDefuzzifier_ApplyRules(
    FB_FuzzyDefuzzifier_t *fb,
    const float ruleStrength[7][7],
    const uint8_t ruleTable[7][7]);

MY_API float FB_FuzzyDefuzzifier_CalculateCentroid(FB_FuzzyDefuzzifier_t *fb);

MY_API float FB_FuzzyDefuzzifier_NormalizedToOutput(
    FB_FuzzyDefuzzifier_t *fb,
    float normalized);

MY_API float FB_FuzzyDefuzzifier_Run(
    FB_FuzzyDefuzzifier_t *fb,
    float Ku);

MY_API float FB_FuzzyDefuzzifier_Slew(
    float current,
    float target,
    float rate,
    float Ts);

MY_API bool FB_FuzzyDefuzzifier_SetConfig(
    FB_FuzzyDefuzzifier_t *fb,
    const FuzzyDefuzzifierConfig_t *config);

MY_API bool FB_FuzzyDefuzzifier_GetConfig(
    const FB_FuzzyDefuzzifier_t *fb,
    FuzzyDefuzzifierConfig_t *config);

MY_API bool FB_FuzzyDefuzzifier_SetKu(
    FB_FuzzyDefuzzifier_t *fb,
    float ku);

MY_API bool FB_FuzzyDefuzzifier_SetOutputRange(
    FB_FuzzyDefuzzifier_t *fb,
    float minOutput,
    float maxOutput);

//------------------------------------------------------------------------------------//
// C++ compatibility
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------------//

#endif /* FB_FUZZY_DEFUZZIFIER_H */
