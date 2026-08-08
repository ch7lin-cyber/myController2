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
 *   - Mamdani MIN inference
 *   - No dynamic memory
 *
 * Rule:
 *
 *     IF Error is E_i
 *     AND dError is dE_j
 *     THEN Output is Rule[i][j]
 *
 ******************************************************************************/

#ifndef FB_FUZZY_RULE_H
#define FB_FUZZY_RULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "FB_FuzzyMembership.h"

/* ============================================================================
 * Configuration
 * ========================================================================== */

#define FUZZY_RULE_SIZE        (7U)

#define FUZZY_RULE_COUNT       (49U)

/*
 * Output Rule range.
 *
 * For your heater controller:
 *
 * 0    = 0.0%
 * 1000 = 100.0%
 *
 * Therefore Rule output is represented by:
 *
 * 0 ~ 1000
 */
#define FUZZY_RULE_OUTPUT_MIN  (0)
#define FUZZY_RULE_OUTPUT_MAX  (1000)

/* ============================================================================
 * Rule Table
 * ========================================================================== */

typedef struct
{
    /*
     * RuleTable[Error][dError]
     *
     * Error:
     *
     * NB NM NS ZE PS PM PB
     *
     * dError:
     *
     * NB NM NS ZE PS PM PB
     */
    int16_t RuleTable[
        FUZZY_RULE_SIZE
    ][
        FUZZY_RULE_SIZE
    ];

} FuzzyRuleTable_t;

/* ============================================================================
 * Rule Firing Result
 * ========================================================================== */

typedef struct
{
    /*
     * Firing strength for every rule.
     *
     * [Error][dError]
     */
    float Weight[
        FUZZY_RULE_SIZE
    ][
        FUZZY_RULE_SIZE
    ];

    /*
     * Weighted contribution.
     */
    float Contribution[
        FUZZY_RULE_SIZE
    ][
        FUZZY_RULE_SIZE
    ];

    /*
     * Sum of all firing weights.
     */
    float TotalWeight;

    /*
     * Sum of weighted rule outputs.
     */
    float WeightedOutput;

    /*
     * Average rule output.
     */
    float RuleOutput;

} FuzzyRuleResult_t;

/* ============================================================================
 * Fuzzy Rule Function Block
 * ========================================================================== */

typedef struct
{
    /*
     * Rule configuration.
     */
    FuzzyRuleTable_t Table;

    /*
     * Rule calculation result.
     */
    FuzzyRuleResult_t Result;

    /*
     * Enable flag.
     */
    bool Enabled;

    /*
     * Initialization flag.
     */
    bool Initialized;

} FB_FuzzyRule_t;

/* ============================================================================
 * Initialization
 * ========================================================================== */

/**
 * @brief Initialize Fuzzy Rule Engine.
 *
 * Loads default 7x7 Rule Table.
 */
void FB_FuzzyRule_Init(
    FB_FuzzyRule_t *fb
);

/**
 * @brief Reset Rule calculation result.
 */
void FB_FuzzyRule_Reset(
    FB_FuzzyRule_t *fb
);

/* ============================================================================
 * Execution
 * ========================================================================== */

/**
 * @brief Execute Fuzzy Rule inference.
 *
 * Uses:
 *
 * Weight = MIN(
 *     Membership(Error),
 *     Membership(dError)
 * )
 *
 * @param fb         Rule Function Block.
 * @param membership Membership result.
 */
void FB_FuzzyRule_Run(
    FB_FuzzyRule_t *fb,
    const FB_FuzzyMembership_t *membership
);

/* ============================================================================
 * Rule Access
 * ========================================================================== */

/**
 * @brief Set one Rule.
 *
 * @param errorIndex    Error linguistic index.
 * @param dErrorIndex   dError linguistic index.
 * @param output        Rule output [0 ~ 1000].
 */
bool FB_FuzzyRule_SetRule(
    FB_FuzzyRule_t *fb,
    uint8_t errorIndex,
    uint8_t dErrorIndex,
    int16_t output
);

/**
 * @brief Get one Rule.
 */
bool FB_FuzzyRule_GetRule(
    const FB_FuzzyRule_t *fb,
    uint8_t errorIndex,
    uint8_t dErrorIndex,
    int16_t *output
);

/* ============================================================================
 * Whole Rule Table
 * ========================================================================== */

/**
 * @brief Set complete Rule Table.
 */
bool FB_FuzzyRule_SetTable(
    FB_FuzzyRule_t *fb,
    const FuzzyRuleTable_t *table
);

/**
 * @brief Get complete Rule Table.
 */
bool FB_FuzzyRule_GetTable(
    const FB_FuzzyRule_t *fb,
    FuzzyRuleTable_t *table
);

/* ============================================================================
 * Default Rule Table
 * ========================================================================== */

/**
 * @brief Load default heater Rule Table.
 */
void FB_FuzzyRule_LoadDefault(
    FB_FuzzyRule_t *fb
);

/* ============================================================================
 * Validation
 * ========================================================================== */

/**
 * @brief Validate Rule Table.
 */
bool FB_FuzzyRule_Validate(
    const FuzzyRuleTable_t *table
);

/* ============================================================================
 * Enable / Disable
 * ========================================================================== */

void FB_FuzzyRule_Enable(
    FB_FuzzyRule_t *fb
);

void FB_FuzzyRule_Disable(
    FB_FuzzyRule_t *fb
);

bool FB_FuzzyRule_IsEnabled(
    const FB_FuzzyRule_t *fb
);

/* ============================================================================
 * Utility
 * ========================================================================== */

/**
 * @brief Get Rule index from Error/dError index.
 *
 * index = errorIndex * 7 + dErrorIndex
 */
uint8_t FB_FuzzyRule_GetIndex(
    uint8_t errorIndex,
    uint8_t dErrorIndex
);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_RULE_H */