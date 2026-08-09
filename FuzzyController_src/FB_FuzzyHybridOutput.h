/******************************************************************************
 * File    : FB_FuzzyHybridOutput.h
 * Brief   : Feed-forward + fuzzy transient correction + slow bias trim.
 *
 * Purpose:
 *   The zero-order Sugeno rule engine returns an absolute fuzzy demand.
 *   This block converts that demand into a correction around a plant-derived
 *   steady-state feed-forward command and adds a slow integral/bias trim.
 ******************************************************************************/
#ifndef FB_FUZZY_HYBRID_OUTPUT_H
#define FB_FUZZY_HYBRID_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_HYBRID_FF_TABLE_SIZE  (16U)

typedef struct
{
    float temperature;
    float pwm;
} FuzzyHybridFFPoint_t;

typedef struct
{
    float pwmMin;
    float pwmMax;
    float slewRate;
    FuzzyHybridFFPoint_t ffTable[FUZZY_HYBRID_FF_TABLE_SIZE];
    uint8_t ffSize;
    float neutralFuzzyPWM;
    float positiveCorrectionGain;
    float negativeCorrectionGain;
    float biasKi;
    float biasMin;
    float biasMax;
    bool enableFeedForward;
    bool enableBiasTrim;
    bool enableSlew;
} FuzzyHybridOutputConfig_t;

typedef struct
{
    float feedForwardPWM;
    float fuzzyCorrectionPWM;
    float biasPWM;
    float targetPWM;
    float outputPWM;
} FuzzyHybridOutputState_t;

typedef struct
{
    FuzzyHybridOutputConfig_t config;
    FuzzyHybridOutputState_t state;
} FB_FuzzyHybridOutput_t;

void FB_FuzzyHybridOutput_Init(FB_FuzzyHybridOutput_t *fb);
void FB_FuzzyHybridOutput_Reset(FB_FuzzyHybridOutput_t *fb);
bool FB_FuzzyHybridOutput_SetConfig(FB_FuzzyHybridOutput_t *fb, const FuzzyHybridOutputConfig_t *config);
float FB_FuzzyHybridOutput_Run(FB_FuzzyHybridOutput_t *fb, float sv, float pv, float fuzzyPWM, float Ts);
float FB_FuzzyHybridOutput_CalcFF(const FB_FuzzyHybridOutput_t *fb, float sv);

//------------------------------------------------------------------------------------//
// C++ compatibility
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------------//

#endif /* FB_FUZZY_HYBRID_OUTPUT_H */
