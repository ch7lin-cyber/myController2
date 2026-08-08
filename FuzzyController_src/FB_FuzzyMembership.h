/******************************************************************************
 * File    : FB_FuzzyMembership.h
 * Version : V2.0
 *
 * Brief   : 7x7 Fuzzy Controller Membership Function Engine
 *
 * Features:
 *   - 7 linguistic variables
 *   - NB / NM / NS / ZE / PS / PM / PB
 *   - Error Membership
 *   - dError Membership
 *   - Runtime configurable
 *   - Triangle / Left Shoulder / Right Shoulder
 *   - Normalized input range [-1.0, +1.0]
 *   - No dynamic memory
 *
 * IEC 61131-3 Style:
 *   FB initialization
 *   FB execution
 *   Runtime parameter configuration
 ******************************************************************************/

#ifndef FB_FUZZY_MEMBERSHIP_H
#define FB_FUZZY_MEMBERSHIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Configuration
 * ========================================================================== */

#define FUZZY_MF_COUNT              (7U)

#define FUZZY_INPUT_MIN             (-1.0f)
#define FUZZY_INPUT_MAX             ( 1.0f)

#define FUZZY_MEMBERSHIP_MIN        (0.0f)
#define FUZZY_MEMBERSHIP_MAX        (1.0f)

/* ============================================================================
 * Linguistic Index
 * ========================================================================== */

typedef enum
{
    FUZZY_NB = 0,
    FUZZY_NM,
    FUZZY_NS,
    FUZZY_ZE,
    FUZZY_PS,
    FUZZY_PM,
    FUZZY_PB

} FuzzyLinguistic_t;

/* ============================================================================
 * Membership Function Type
 * ========================================================================== */

typedef enum
{
    FUZZY_MF_TRIANGLE = 0,

    FUZZY_MF_LEFT_SHOULDER,

    FUZZY_MF_RIGHT_SHOULDER

} FuzzyMFType_t;

/* ============================================================================
 * Membership Function Parameter
 *
 * Triangle:
 *
 *             C
 *            /\
 *           /  \
 *          /    \
 *         /      \
 *        L        R
 *
 * L = Left
 * C = Center
 * R = Right
 *
 * Left Shoulder:
 *
 *       __________
 *      /
 *     /
 * ----
 *
 * Right Shoulder:
 *
 *              ______
 *             /
 *            /
 * __________/
 * ========================================================================== */

typedef struct
{
    FuzzyMFType_t Type;

    float Left;

    float Center;

    float Right;

} FuzzyMembershipFunction_t;

/* ============================================================================
 * Membership Result
 * ========================================================================== */

typedef struct
{
    float Degree[FUZZY_MF_COUNT];

    uint8_t DominantIndex;

    float DominantDegree;

} FuzzyMembershipResult_t;

/* ============================================================================
 * Membership Set
 * ========================================================================== */

typedef struct
{
    FuzzyMembershipFunction_t MF[FUZZY_MF_COUNT];

} FuzzyMembershipSet_t;

/* ============================================================================
 * Membership Configuration
 * ========================================================================== */

typedef struct
{
    FuzzyMembershipSet_t Error;

    FuzzyMembershipSet_t dError;

} FuzzyMembershipConfig_t;

/* ============================================================================
 * Membership Controller
 * ========================================================================== */

typedef struct
{
    FuzzyMembershipConfig_t Config;

    FuzzyMembershipResult_t ErrorResult;

    FuzzyMembershipResult_t dErrorResult;

    float ErrorInput;

    float dErrorInput;

    bool Initialized;

} FB_FuzzyMembership_t;

/* ============================================================================
 * Initialization
 * ========================================================================== */

/**
 * @brief Initialize Membership Engine with default configuration.
 *
 * @param fb Pointer to Membership Function Block.
 */
void FB_FuzzyMembership_Init(
    FB_FuzzyMembership_t *fb
);

/**
 * @brief Reset Membership result.
 *
 * @param fb Pointer to Membership Function Block.
 */
void FB_FuzzyMembership_Reset(
    FB_FuzzyMembership_t *fb
);

/* ============================================================================
 * Execution
 * ========================================================================== */

/**
 * @brief Execute Membership calculation.
 *
 * Input must be normalized to [-1.0, +1.0].
 *
 * @param fb Pointer to Membership Function Block.
 * @param error Normalized Error.
 * @param dError Normalized Error derivative.
 */
void FB_FuzzyMembership_Run(
    FB_FuzzyMembership_t *fb,
    float error,
    float dError
);

/* ============================================================================
 * Membership Calculation
 * ========================================================================== */

/**
 * @brief Calculate one membership function.
 *
 * @param mf Membership function.
 * @param x Input.
 *
 * @return Membership degree [0.0 ~ 1.0].
 */
float FB_FuzzyMembership_Calculate(
    const FuzzyMembershipFunction_t *mf,
    float x
);

/**
 * @brief Calculate complete membership set.
 *
 * @param set Membership set.
 * @param x Input.
 * @param result Result.
 */
void FB_FuzzyMembership_CalculateSet(
    const FuzzyMembershipSet_t *set,
    float x,
    FuzzyMembershipResult_t *result
);

/* ============================================================================
 * Runtime Configuration
 * ========================================================================== */

/**
 * @brief Set one Error Membership function.
 */
bool FB_FuzzyMembership_SetErrorMF(
    FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMFType_t type,
    float left,
    float center,
    float right
);

/**
 * @brief Set one dError Membership function.
 */
bool FB_FuzzyMembership_SetDErrorMF(
    FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMFType_t type,
    float left,
    float center,
    float right
);

/**
 * @brief Get one Error Membership function.
 */
bool FB_FuzzyMembership_GetErrorMF(
    const FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMembershipFunction_t *mf
);

/**
 * @brief Get one dError Membership function.
 */
bool FB_FuzzyMembership_GetDErrorMF(
    const FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMembershipFunction_t *mf
);

/* ============================================================================
 * Whole Configuration
 * ========================================================================== */

/**
 * @brief Set complete Membership configuration.
 */
bool FB_FuzzyMembership_SetConfig(
    FB_FuzzyMembership_t *fb,
    const FuzzyMembershipConfig_t *config
);

/**
 * @brief Get complete Membership configuration.
 */
bool FB_FuzzyMembership_GetConfig(
    const FB_FuzzyMembership_t *fb,
    FuzzyMembershipConfig_t *config
);

/* ============================================================================
 * Input Normalization
 * ========================================================================== */

/**
 * @brief Clamp input into [-1.0, +1.0].
 */
float FB_FuzzyMembership_ClampInput(
    float input
);

/* ============================================================================
 * Utility
 * ========================================================================== */

/**
 * @brief Get linguistic name.
 */
const char *FB_FuzzyMembership_GetName(
    uint8_t index
);

/**
 * @brief Validate Membership configuration.
 */
bool FB_FuzzyMembership_Validate(
    const FuzzyMembershipSet_t *set
);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_MEMBERSHIP_H */