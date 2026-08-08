/******************************************************************************
 * File    : FB_FuzzyMembership.c
 * Version : V2.0
 *
 * Brief   : 7x7 Fuzzy Membership Function Engine
 ******************************************************************************/

#include "FB_FuzzyMembership.h"

#include <stddef.h>

/* ============================================================================
 * Internal Constants
 * ========================================================================== */

#define FUZZY_EPSILON      (0.000001f)

/* ============================================================================
 * Internal Functions
 * ========================================================================== */

static float Fuzzy_Abs(float x)
{
    return (x >= 0.0f) ? x : -x;
}

/* -------------------------------------------------------------------------- */

static float Fuzzy_Min(
    float a,
    float b
)
{
    return (a < b) ? a : b;
}

/* -------------------------------------------------------------------------- */

static float Fuzzy_Max(
    float a,
    float b
)
{
    return (a > b) ? a : b;
}

/* -------------------------------------------------------------------------- */

static float Fuzzy_Clamp01(
    float x
)
{
    if (x < FUZZY_MEMBERSHIP_MIN)
    {
        return FUZZY_MEMBERSHIP_MIN;
    }

    if (x > FUZZY_MEMBERSHIP_MAX)
    {
        return FUZZY_MEMBERSHIP_MAX;
    }

    return x;
}

/* ============================================================================
 * Triangle Membership
 *
 *                1
 *                |
 *               /\
 *              /  \
 *             /    \
 *            /      \
 *           /        \
 * ----------+----+-----+----------
 *           L    C     R
 *
 * ========================================================================== */

static float Fuzzy_Triangle(
    float x,
    float left,
    float center,
    float right
)
{
    float result;

    /*
     * Degenerate configuration protection.
     */
    if ((center <= left) || (right <= center))
    {
        return 0.0f;
    }

    /*
     * Outside range.
     */
    if ((x <= left) || (x >= right))
    {
        return 0.0f;
    }

    /*
     * Peak.
     */
    if (Fuzzy_Abs(x - center) <= FUZZY_EPSILON)
    {
        return 1.0f;
    }

    /*
     * Rising edge.
     */
    if (x < center)
    {
        result = (x - left) / (center - left);

        return Fuzzy_Clamp01(result);
    }

    /*
     * Falling edge.
     */
    result = (right - x) / (right - center);

    return Fuzzy_Clamp01(result);
}

/* ============================================================================
 * Left Shoulder Membership
 *
 *             __________ 1
 *            /
 *           /
 *          /
 * ________/
 *
 * left < center
 *
 * x <= left      -> 1
 * left < x < C   -> linear
 * x >= C         -> 0
 * ========================================================================== */

static float Fuzzy_LeftShoulder(
    float x,
    float left,
    float center
)
{
    float result;

    if (center <= left)
    {
        return 0.0f;
    }

    if (x <= left)
    {
        return 1.0f;
    }

    if (x >= center)
    {
        return 0.0f;
    }

    result = (center - x) / (center - left);

    return Fuzzy_Clamp01(result);
}

/* ============================================================================
 * Right Shoulder Membership
 *
 * 1 __________
 *            \
 *             \
 *              \
 * ______________\
 *
 * center < right
 *
 * x <= center    -> 0
 * center < x < R -> linear
 * x >= right     -> 1
 * ========================================================================== */

static float Fuzzy_RightShoulder(
    float x,
    float center,
    float right
)
{
    float result;

    if (right <= center)
    {
        return 0.0f;
    }

    if (x <= center)
    {
        return 0.0f;
    }

    if (x >= right)
    {
        return 1.0f;
    }

    result = (x - center) / (right - center);

    return Fuzzy_Clamp01(result);
}

/* ============================================================================
 * Default Membership Configuration
 *
 * Input normalized range:
 *
 * -1.0                       0                       +1.0
 *  |-------------------------|-------------------------|
 *
 * NB NM NS ZE PS PM PB
 *
 * ========================================================================== */

static void Fuzzy_LoadDefaultSet(
    FuzzyMembershipSet_t *set
)
{
    if (set == NULL)
    {
        return;
    }

    /*
     * NB
     *
     * Left shoulder
     */
    set->MF[FUZZY_NB].Type   = FUZZY_MF_LEFT_SHOULDER;
    set->MF[FUZZY_NB].Left   = -1.00f;
    set->MF[FUZZY_NB].Center = -0.75f;
    set->MF[FUZZY_NB].Right  = -0.50f;

    /*
     * NM
     */
    set->MF[FUZZY_NM].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_NM].Left   = -0.75f;
    set->MF[FUZZY_NM].Center = -0.50f;
    set->MF[FUZZY_NM].Right  = -0.25f;

    /*
     * NS
     */
    set->MF[FUZZY_NS].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_NS].Left   = -0.50f;
    set->MF[FUZZY_NS].Center = -0.25f;
    set->MF[FUZZY_NS].Right  =  0.00f;

    /*
     * ZE
     */
    set->MF[FUZZY_ZE].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_ZE].Left   = -0.25f;
    set->MF[FUZZY_ZE].Center =  0.00f;
    set->MF[FUZZY_ZE].Right  =  0.25f;

    /*
     * PS
     */
    set->MF[FUZZY_PS].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_PS].Left   =  0.00f;
    set->MF[FUZZY_PS].Center =  0.25f;
    set->MF[FUZZY_PS].Right  =  0.50f;

    /*
     * PM
     */
    set->MF[FUZZY_PM].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_PM].Left   =  0.25f;
    set->MF[FUZZY_PM].Center =  0.50f;
    set->MF[FUZZY_PM].Right  =  0.75f;

    /*
     * PB
     *
     * Right shoulder
     */
    set->MF[FUZZY_PB].Type   = FUZZY_MF_RIGHT_SHOULDER;
    set->MF[FUZZY_PB].Left   =  0.50f;
    set->MF[FUZZY_PB].Center =  0.75f;
    set->MF[FUZZY_PB].Right  =  1.00f;
}

/* ============================================================================
 * Initialization
 * ========================================================================== */

void FB_FuzzyMembership_Init(
    FB_FuzzyMembership_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    /*
     * Clear runtime state.
     */
    fb->ErrorInput  = 0.0f;
    fb->dErrorInput = 0.0f;

    fb->ErrorResult.DominantIndex = FUZZY_ZE;
    fb->ErrorResult.DominantDegree = 0.0f;

    fb->dErrorResult.DominantIndex = FUZZY_ZE;
    fb->dErrorResult.DominantDegree = 0.0f;

    for (uint8_t i = 0U; i < FUZZY_MF_COUNT; i++)
    {
        fb->ErrorResult.Degree[i] = 0.0f;
        fb->dErrorResult.Degree[i] = 0.0f;
    }

    /*
     * Load default Error Membership.
     */
    Fuzzy_LoadDefaultSet(
        &fb->Config.Error
    );

    /*
     * Load default dError Membership.
     */
    Fuzzy_LoadDefaultSet(
        &fb->Config.dError
    );

    fb->Initialized = true;
}

/* ============================================================================
 * Reset
 * ========================================================================== */

void FB_FuzzyMembership_Reset(
    FB_FuzzyMembership_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->ErrorInput  = 0.0f;
    fb->dErrorInput = 0.0f;

    fb->ErrorResult.DominantIndex = FUZZY_ZE;
    fb->ErrorResult.DominantDegree = 0.0f;

    fb->dErrorResult.DominantIndex = FUZZY_ZE;
    fb->dErrorResult.DominantDegree = 0.0f;

    for (uint8_t i = 0U; i < FUZZY_MF_COUNT; i++)
    {
        fb->ErrorResult.Degree[i] = 0.0f;
        fb->dErrorResult.Degree[i] = 0.0f;
    }
}

/* ============================================================================
 * Clamp Input
 * ========================================================================== */

float FB_FuzzyMembership_ClampInput(
    float input
)
{
    if (input < FUZZY_INPUT_MIN)
    {
        return FUZZY_INPUT_MIN;
    }

    if (input > FUZZY_INPUT_MAX)
    {
        return FUZZY_INPUT_MAX;
    }

    return input;
}

/* ============================================================================
 * Calculate One Membership
 * ========================================================================== */

float FB_FuzzyMembership_Calculate(
    const FuzzyMembershipFunction_t *mf,
    float x
)
{
    if (mf == NULL)
    {
        return 0.0f;
    }

    x = FB_FuzzyMembership_ClampInput(x);

    switch (mf->Type)
    {
        case FUZZY_MF_TRIANGLE:

            return Fuzzy_Triangle(
                x,
                mf->Left,
                mf->Center,
                mf->Right
            );

        case FUZZY_MF_LEFT_SHOULDER:

            return Fuzzy_LeftShoulder(
                x,
                mf->Left,
                mf->Center
            );

        case FUZZY_MF_RIGHT_SHOULDER:

            return Fuzzy_RightShoulder(
                x,
                mf->Center,
                mf->Right
            );

        default:

            return 0.0f;
    }
}

/* ============================================================================
 * Calculate Complete Membership Set
 * ========================================================================== */

void FB_FuzzyMembership_CalculateSet(
    const FuzzyMembershipSet_t *set,
    float x,
    FuzzyMembershipResult_t *result
)
{
    float maxDegree = 0.0f;
    uint8_t maxIndex = FUZZY_ZE;

    if ((set == NULL) || (result == NULL))
    {
        return;
    }

    x = FB_FuzzyMembership_ClampInput(x);

    for (uint8_t i = 0U; i < FUZZY_MF_COUNT; i++)
    {
        result->Degree[i] =
            FB_FuzzyMembership_Calculate(
                &set->MF[i],
                x
            );

        /*
         * Find dominant Membership.
         */
        if (result->Degree[i] > maxDegree)
        {
            maxDegree = result->Degree[i];
            maxIndex = i;
        }
    }

    result->DominantIndex  = maxIndex;
    result->DominantDegree = maxDegree;
}

/* ============================================================================
 * Main Run
 * ========================================================================== */

void FB_FuzzyMembership_Run(
    FB_FuzzyMembership_t *fb,
    float error,
    float dError
)
{
    if (fb == NULL)
    {
        return;
    }

    if (!fb->Initialized)
    {
        FB_FuzzyMembership_Init(fb);
    }

    /*
     * Normalize / Clamp input.
     *
     * Scaling Engine from Part 3 will provide
     * already normalized values.
     */
    fb->ErrorInput =
        FB_FuzzyMembership_ClampInput(error);

    fb->dErrorInput =
        FB_FuzzyMembership_ClampInput(dError);

    /*
     * Error Membership.
     */
    FB_FuzzyMembership_CalculateSet(
        &fb->Config.Error,
        fb->ErrorInput,
        &fb->ErrorResult
    );

    /*
     * dError Membership.
     */
    FB_FuzzyMembership_CalculateSet(
        &fb->Config.dError,
        fb->dErrorInput,
        &fb->dErrorResult
    );
}

/* ============================================================================
 * Set Error Membership
 * ========================================================================== */

bool FB_FuzzyMembership_SetErrorMF(
    FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMFType_t type,
    float left,
    float center,
    float right
)
{
    if (fb == NULL)
    {
        return false;
    }

    if (index >= FUZZY_MF_COUNT)
    {
        return false;
    }

    /*
     * Validate parameter.
     */
    if (left > center)
    {
        return false;
    }

    if (center > right)
    {
        return false;
    }

    if (left < FUZZY_INPUT_MIN)
    {
        return false;
    }

    if (right > FUZZY_INPUT_MAX)
    {
        return false;
    }

    /*
     * Store configuration.
     */
    fb->Config.Error.MF[index].Type   = type;
    fb->Config.Error.MF[index].Left   = left;
    fb->Config.Error.MF[index].Center = center;
    fb->Config.Error.MF[index].Right  = right;

    return true;
}

/* ============================================================================
 * Set dError Membership
 * ========================================================================== */

bool FB_FuzzyMembership_SetDErrorMF(
    FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMFType_t type,
    float left,
    float center,
    float right
)
{
    if (fb == NULL)
    {
        return false;
    }

    if (index >= FUZZY_MF_COUNT)
    {
        return false;
    }

    if (left > center)
    {
        return false;
    }

    if (center > right)
    {
        return false;
    }

    if (left < FUZZY_INPUT_MIN)
    {
        return false;
    }

    if (right > FUZZY_INPUT_MAX)
    {
        return false;
    }

    fb->Config.dError.MF[index].Type   = type;
    fb->Config.dError.MF[index].Left   = left;
    fb->Config.dError.MF[index].Center = center;
    fb->Config.dError.MF[index].Right  = right;

    return true;
}

/* ============================================================================
 * Get Error Membership
 * ========================================================================== */

bool FB_FuzzyMembership_GetErrorMF(
    const FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMembershipFunction_t *mf
)
{
    if ((fb == NULL) || (mf == NULL))
    {
        return false;
    }

    if (index >= FUZZY_MF_COUNT)
    {
        return false;
    }

    *mf = fb->Config.Error.MF[index];

    return true;
}

/* ============================================================================
 * Get dError Membership
 * ========================================================================== */

bool FB_FuzzyMembership_GetDErrorMF(
    const FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMembershipFunction_t *mf
)
{
    if ((fb == NULL) || (mf == NULL))
    {
        return false;
    }

    if (index >= FUZZY_MF_COUNT)
    {
        return false;
    }

    *mf = fb->Config.dError.MF[index];

    return true;
}

/* ============================================================================
 * Set Complete Configuration
 * ========================================================================== */

bool FB_FuzzyMembership_SetConfig(
    FB_FuzzyMembership_t *fb,
    const FuzzyMembershipConfig_t *config
)
{
    if ((fb == NULL) || (config == NULL))
    {
        return false;
    }

    if (!FB_FuzzyMembership_Validate(
            &config->Error))
    {
        return false;
    }

    if (!FB_FuzzyMembership_Validate(
            &config->dError))
    {
        return false;
    }

    fb->Config = *config;

    return true;
}

/* ============================================================================
 * Get Complete Configuration
 * ========================================================================== */

bool FB_FuzzyMembership_GetConfig(
    const FB_FuzzyMembership_t *fb,
    FuzzyMembershipConfig_t *config
)
{
    if ((fb == NULL) || (config == NULL))
    {
        return false;
    }

    *config = fb->Config;

    return true;
}

/* ============================================================================
 * Validation
 * ========================================================================== */

bool FB_FuzzyMembership_Validate(
    const FuzzyMembershipSet_t *set
)
{
    if (set == NULL)
    {
        return false;
    }

    for (uint8_t i = 0U; i < FUZZY_MF_COUNT; i++)
    {
        const FuzzyMembershipFunction_t *mf =
            &set->MF[i];

        /*
         * Basic range.
         */
        if (mf->Left < FUZZY_INPUT_MIN)
        {
            return false;
        }

        if (mf->Right > FUZZY_INPUT_MAX)
        {
            return false;
        }

        /*
         * Ordering.
         */
        if (mf->Left > mf->Center)
        {
            return false;
        }

        if (mf->Center > mf->Right)
        {
            return false;
        }

        /*
         * Type validation.
         */
        switch (mf->Type)
        {
            case FUZZY_MF_TRIANGLE:

                if (mf->Center <= mf->Left)
                {
                    return false;
                }

                if (mf->Right <= mf->Center)
                {
                    return false;
                }

                break;

            case FUZZY_MF_LEFT_SHOULDER:

                if (mf->Center <= mf->Left)
                {
                    return false;
                }

                break;

            case FUZZY_MF_RIGHT_SHOULDER:

                if (mf->Right <= mf->Center)
                {
                    return false;
                }

                break;

            default:

                return false;
        }
    }

    return true;
}

/* ============================================================================
 * Linguistic Name
 * ========================================================================== */

const char *FB_FuzzyMembership_GetName(
    uint8_t index
)
{
    switch (index)
    {
        case FUZZY_NB:
            return "NB";

        case FUZZY_NM:
            return "NM";

        case FUZZY_NS:
            return "NS";

        case FUZZY_ZE:
            return "ZE";

        case FUZZY_PS:
            return "PS";

        case FUZZY_PM:
            return "PM";

        case FUZZY_PB:
            return "PB";

        default:
            return "UNKNOWN";
    }
}