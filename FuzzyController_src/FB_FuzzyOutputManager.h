/******************************************************************************
 * File    : FB_FuzzyOutputManager.h
 * Brief   : Absolute PWM output manager for zero-order Sugeno fuzzy control
 *
 * Active path:
 *   FF (optional blend) + absolute fuzzy PWM -> clamp -> slew -> PWM
 *
 * IMPORTANT:
 *   The active Fuzzy Rule Engine already returns an absolute PWM command
 *   (0..1000). It must NOT be multiplied by fuzzyScale or treated as dPWM.
 ******************************************************************************/
#ifndef FB_FUZZY_OUTPUT_MANAGER_H
#define FB_FUZZY_OUTPUT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "ssm_std_define.h"

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_PWM_MIN          0.0f
#define FUZZY_PWM_MAX          1000.0f
#define FUZZY_FF_TABLE_SIZE    16U

typedef struct
{
    float temperature;
    float pwm;
} FuzzyFFPoint_t;

typedef struct
{
    /* Legacy diagnostic gain; not used on the absolute Sugeno path. */
    float fuzzyScale;
    float pwmMin;
    float pwmMax;
    /* PWM units / second. At 20 ms, 5000 = 100 PWM counts / cycle. */
    float slewRate;
    FuzzyFFPoint_t ffTable[FUZZY_FF_TABLE_SIZE];
    uint8_t ffSize;
    bool enableFeedForward;
    bool enableSlew;
    /* 0.0 = pure fuzzy PWM, 1.0 = pure FF PWM. */
    float ffBlend;
} FuzzyOutputConfig_t;

typedef struct
{
    float pwmFF;
    float fuzzyCorrection;
    float targetPWM;
    float outputPWM;
    float previousPWM;
} FuzzyOutputState_t;

typedef struct
{
    FuzzyOutputConfig_t config;
    FuzzyOutputState_t state;
} FB_FuzzyOutputManager_t;

MY_API void FB_FuzzyOutput_Init(FB_FuzzyOutputManager_t *fb);
MY_API float FB_FuzzyOutput_RunAbsolute(FB_FuzzyOutputManager_t *fb, float sv, float fuzzyPWM, float Ts);
MY_API float FB_FuzzyOutput_Run(FB_FuzzyOutputManager_t *fb, float sv, float pv, float centroid, float Ts);
MY_API float FB_FuzzyOutput_CalcFF(FB_FuzzyOutputManager_t *fb, float temperature);
MY_API float FB_FuzzyOutput_Interpolation(const FuzzyFFPoint_t *table, uint8_t size, float x);
MY_API float FB_FuzzyOutput_Slew(float current, float target, float rate, float Ts);
MY_API bool FB_FuzzyOutput_SetConfig(FB_FuzzyOutputManager_t *fb, const FuzzyOutputConfig_t *cfg);

//------------------------------------------------------------------------------------//
// C++ compatibility
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------------//

#endif /* FB_FUZZY_OUTPUT_MANAGER_H */
