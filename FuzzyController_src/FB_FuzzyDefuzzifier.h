/******************************************************************************
 * File    : FB_FuzzyDefuzzifier.h
 * Version : V2.0
 *
 * Brief   : Mamdani Fuzzy Defuzzification Engine
 *
 * Architecture:
 *
 *   Part 1 : Membership Engine
 *   Part 2 : 7x7 Rule Engine
 *   Part 3 : Adaptive Scaling
 *   Part 4 : Defuzzification
 *
 * Features:
 *   - Mamdani inference
 *   - 7 output membership functions
 *   - Weighted aggregation
 *   - Centroid / Center of Gravity
 *   - Output scaling Ku
 *   - PWM 0 ~ 1000
 *   - Output clamp
 *   - Output slew rate
 *   - Runtime configurable output membership
 *
 ******************************************************************************/

#ifndef FB_FUZZY_DEFUZZIFIER_H
#define FB_FUZZY_DEFUZZIFIER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Constants
 * ========================================================================== */

#define FUZZY_DEFUZZ_OUTPUT_MF_COUNT        (7U)

/*
 * Output range.
 *
 * Your system:
 *
 * PWM = 0 ~ 1000
 *
 * 1000 = 100.0%
 */
#define FUZZY_DEFUZZ_DEFAULT_OUTPUT_MIN     (0.0f)
#define FUZZY_DEFUZZ_DEFAULT_OUTPUT_MAX     (1000.0f)

/*
 * Centroid resolution.
 *
 * Higher:
 *     More accurate
 *
 * Lower:
 *     Faster
 *
 * 101 points is a good starting point for MCU.
 */
#define FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS    (101U)

/*
 * Output slew rate.
 *
 * Unit:
 *     PWM / second
 *
 * Default 5000 means:
 *
 * 0 -> 1000 takes approximately 0.2 sec
 */
#define FUZZY_DEFUZZ_DEFAULT_SLEW_RATE       (5000.0f)

/*
 * Default output membership range.
 *
 * Normalized:
 *
 * -1.0 ~ +1.0
 *
 * The seven output linguistic labels are:
 *
 * NB NM NS ZE PS PM PB
 */
#define FUZZY_DEFUZZ_NORMALIZED_MIN         (-1.0f)
#define FUZZY_DEFUZZ_NORMALIZED_MAX         (1.0f)

/* ============================================================================
 * Output Membership Function
 * ========================================================================== */

typedef struct
{
    /*
     * Center position.
     *
     * Example:
     *
     * NB = -1.0
     * NM = -0.666
     * NS = -0.333
     * ZE =  0.0
     * PS = +0.333
     * PM = +0.666
     * PB = +1.0
     */
    float Center;

    /*
     * Left base point.
     */
    float Left;

    /*
     * Right base point.
     */
    float Right;

    /*
     * Peak membership.
     */
    float Peak;

} FuzzyOutputMF_t;

/* ============================================================================
 * Configuration
 * ========================================================================== */

typedef struct
{
    /*
     * Physical output range.
     *
     * For your heater:
     *
     * 0 ~ 1000
     */
    float OutputMin;
    float OutputMax;

    /*
     * Number of integration points.
     */
    uint16_t CentroidPoints;

    /*
     * Output slew rate.
     */
    float OutputSlewRate;

    /*
     * Enable output scaling.
     */
    bool OutputScalingEnable;

    /*
     * Enable output slew.
     */
    bool OutputSlewEnable;

    /*
     * Output Membership Functions.
     *
     * [0] = NB
     * [1] = NM
     * [2] = NS
     * [3] = ZE
     * [4] = PS
     * [5] = PM
     * [6] = PB
     */
    FuzzyOutputMF_t MF[
        FUZZY_DEFUZZ_OUTPUT_MF_COUNT
    ];

} FuzzyDefuzzifierConfig_t;

/* ============================================================================
 * Runtime State
 * ========================================================================== */

typedef struct
{
    /*
     * Rule activation.
     *
     * [7]
     *
     * Each value corresponds to:
     *
     * NB NM NS ZE PS PM PB
     */
    float Activation[
        FUZZY_DEFUZZ_OUTPUT_MF_COUNT
    ];

    /*
     * Aggregated membership result.
     */
    float Aggregated[
        FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS
    ];

    /*
     * Centroid result.
     *
     * Normalized:
     *
     * -1 ~ +1
     */
    float Centroid;

    /*
     * Physical output before Ku.
     */
    float RawOutput;

    /*
     * Physical output after Ku.
     */
    float ScaledOutput;

    /*
     * Final output after slew.
     */
    float Output;

    /*
     * Current Ku.
     */
    float Ku;

    /*
     * Target output.
     */
    float TargetOutput;

    /*
     * Previous output.
     */
    float PreviousOutput;

    /*
     * Diagnostic.
     */
    float Numerator;
    float Denominator;

} FuzzyDefuzzifierState_t;

/* ============================================================================
 * Function Block
 * ========================================================================== */

typedef struct
{
    FuzzyDefuzzifierConfig_t Config;

    FuzzyDefuzzifierState_t State;

} FB_FuzzyDefuzzifier_t;

/* ============================================================================
 * Initialization
 * ========================================================================== */

void FB_FuzzyDefuzzifier_Init(
    FB_FuzzyDefuzzifier_t *fb
);

void FB_FuzzyDefuzzifier_Reset(
    FB_FuzzyDefuzzifier_t *fb
);

/* ============================================================================
 * Rule Activation
 * ========================================================================== */

/**
 * @brief Clear output activation.
 */
void FB_FuzzyDefuzzifier_Clear(
    FB_FuzzyDefuzzifier_t *fb
);

/**
 * @brief Apply one rule activation.
 *
 * @param outputMF
 *        0 = NB
 *        1 = NM
 *        2 = NS
 *        3 = ZE
 *        4 = PS
 *        5 = PM
 *        6 = PB
 *
 * @param strength
 *        Rule firing strength 0 ~ 1.
 */
void FB_FuzzyDefuzzifier_ApplyRule(
    FB_FuzzyDefuzzifier_t *fb,
    uint8_t outputMF,
    float strength
);

/**
 * @brief Apply 49 rule activations.
 *
 * @param ruleOutput
 *        7 x 7 output rule table.
 */
void FB_FuzzyDefuzzifier_ApplyRules(
    FB_FuzzyDefuzzifier_t *fb,
    const float ruleStrength[7][7],
    const uint8_t ruleTable[7][7]
);

/* ============================================================================
 * Defuzzification
 * ========================================================================== */

/**
 * @brief Calculate centroid.
 *
 * @return normalized output -1 ~ +1.
 */
float FB_FuzzyDefuzzifier_CalculateCentroid(
    FB_FuzzyDefuzzifier_t *fb
);

/**
 * @brief Convert normalized output to physical output.
 */
float FB_FuzzyDefuzzifier_NormalizedToOutput(
    FB_FuzzyDefuzzifier_t *fb,
    float normalized
);

/**
 * @brief Execute complete defuzzification.
 *
 * @param Ku
 *        Output scaling from Part 3.
 *
 * @return final PWM 0 ~ 1000.
 */
float FB_FuzzyDefuzzifier_Run(
    FB_FuzzyDefuzzifier_t *fb,
    float Ku
);

/* ============================================================================
 * Output Slew
 * ========================================================================== */

float FB_FuzzyDefuzzifier_Slew(
    float current,
    float target,
    float rate,
    float Ts
);

/* ============================================================================
 * Configuration
 * ========================================================================== */

bool FB_FuzzyDefuzzifier_SetConfig(
    FB_FuzzyDefuzzifier_t *fb,
    const FuzzyDefuzzifierConfig_t *config
);

bool FB_FuzzyDefuzzifier_GetConfig(
    const FB_FuzzyDefuzzifier_t *fb,
    FuzzyDefuzzifierConfig_t *config
);

/* ============================================================================
 * Runtime Parameters
 * ========================================================================== */

bool FB_FuzzyDefuzzifier_SetKu(
    FB_FuzzyDefuzzifier_t *fb,
    float ku
);

bool FB_FuzzyDefuzzifier_SetOutputRange(
    FB_FuzzyDefuzzifier_t *fb,
    float minOutput,
    float maxOutput
);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_DEFUZZIFIER_H */