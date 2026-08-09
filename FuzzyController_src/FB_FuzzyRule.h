/******************************************************************************
 * File    : FB_FuzzyRule.h
 * Version : V2.0
 *
 * Brief   : 7x7 Fuzzy Rule Engine
 *
 * Features:
 *   - 7 x 7 = 49 Rules
 *   - NB / NM / NS / ZE / PS / PM / PB
 *   - Runtime configurable Rule Table
 *   - Rule firing strength
 *   - Zero-order Sugeno weighted-average inference
 *   - No dynamic memory
 ******************************************************************************/

#ifndef FB_FUZZY_RULE_H
#define FB_FUZZY_RULE_H

#include <stdint.h>
#include <stdbool.h>
#include "ssm_std_define.h"
#include "FB_FuzzyMembership.h"

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_RULE_SIZE        (7U)
#define FUZZY_RULE_COUNT       (49U)
#define FUZZY_RULE_OUTPUT_MIN  (0)
#define FUZZY_RULE_OUTPUT_MAX  (1000)

typedef struct
{
    int16_t RuleTable[FUZZY_RULE_SIZE][FUZZY_RULE_SIZE];
} FuzzyRuleTable_t;

typedef struct
{
    float Weight[FUZZY_RULE_SIZE][FUZZY_RULE_SIZE];
    float Contribution[FUZZY_RULE_SIZE][FUZZY_RULE_SIZE];
    float TotalWeight;
    float WeightedOutput;
    float RuleOutput;
} FuzzyRuleResult_t;

typedef struct
{
    FuzzyRuleTable_t Table;
    FuzzyRuleResult_t Result;
    bool Enabled;
    bool Initialized;
} FB_FuzzyRule_t;

MY_API void FB_FuzzyRule_Init(FB_FuzzyRule_t *fb);
MY_API void FB_FuzzyRule_Reset(FB_FuzzyRule_t *fb);

/**
 * Execute zero-order Sugeno inference.
 *
 * Weight = MIN(Membership(Error), Membership(dError))
 * RuleOutput = SUM(Weight * SingletonOutput) / SUM(Weight)
 */
MY_API void FB_FuzzyRule_Run(FB_FuzzyRule_t *fb,
                             const FB_FuzzyMembership_t *membership);

MY_API bool FB_FuzzyRule_SetRule(FB_FuzzyRule_t *fb,
                                 uint8_t errorIndex,
                                 uint8_t dErrorIndex,
                                 int16_t output);

MY_API bool FB_FuzzyRule_GetRule(const FB_FuzzyRule_t *fb,
                                 uint8_t errorIndex,
                                 uint8_t dErrorIndex,
                                 int16_t *output);

MY_API bool FB_FuzzyRule_SetTable(FB_FuzzyRule_t *fb,
                                  const FuzzyRuleTable_t *table);

MY_API bool FB_FuzzyRule_GetTable(const FB_FuzzyRule_t *fb,
                                  FuzzyRuleTable_t *table);

MY_API void FB_FuzzyRule_LoadDefault(FB_FuzzyRule_t *fb);

/**
 * Validate output range and heater monotonicity.
 * Increasing Error or dError must not decrease PWM.
 */
MY_API bool FB_FuzzyRule_Validate(const FuzzyRuleTable_t *table);

MY_API void FB_FuzzyRule_Enable(FB_FuzzyRule_t *fb);
MY_API void FB_FuzzyRule_Disable(FB_FuzzyRule_t *fb);
MY_API bool FB_FuzzyRule_IsEnabled(const FB_FuzzyRule_t *fb);

MY_API uint8_t FB_FuzzyRule_GetIndex(uint8_t errorIndex,
                                     uint8_t dErrorIndex);

//------------------------------------------------------------------------------------//
// C++ compatibility
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------------//

#endif /* FB_FUZZY_RULE_H */
