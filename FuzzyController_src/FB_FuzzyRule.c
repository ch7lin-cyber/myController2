/******************************************************************************
 * File    : FB_FuzzyRule.c
 * Version : V2.0
 *
 * Brief   : 7x7 Fuzzy Rule Engine
 ******************************************************************************/

#include "FB_FuzzyRule.h"

#include <stddef.h>

/* ============================================================================
 * Internal Functions
 * ========================================================================== */

static float FuzzyRule_Min(
    float a,
    float b
)
{
    return (a < b) ? a : b;
}

/* -------------------------------------------------------------------------- */

static float FuzzyRule_ClampWeight(
    float value
)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }

    if (value > 1.0f)
    {
        return 1.0f;
    }

    return value;
}

/* ============================================================================
 * Default Rule Table
 *
 * The output is PWM command:
 *
 * 0    = 0.0%
 * 500  = 50.0%
 * 1000 = 100.0%
 *
 * Error:
 *
 * NB = PV far above SV
 * NM = PV above SV
 * NS = PV slightly above SV
 * ZE = PV near SV
 * PS = PV slightly below SV
 * PM = PV below SV
 * PB = PV far below SV
 *
 * dError:
 *
 * NB = Error rapidly decreasing
 * PB = Error rapidly increasing
 *
 * Heater interpretation:
 *
 * Positive Error -> increase heater output.
 *
 * ========================================================================== */

static const int16_t DefaultRuleTable[
    FUZZY_RULE_SIZE
][
    FUZZY_RULE_SIZE
] =
{
    /*
     * Error = NB
     *
     * Temperature is far above SV.
     * Heater should be OFF.
     */
    {
        0,
        0,
        0,
        0,
        0,
        0,
        0
    },

    /*
     * Error = NM
     */
    {
        0,
        0,
        0,
        0,
        0,
        0,
        0
    },

    /*
     * Error = NS
     */
    {
        0,
        0,
        0,
        50,
        50,
        75,
        100
    },

    /*
     * Error = ZE
     */
    {
        0,
        25,
        50,
        100,
        150,
        200,
        250
    },

    /*
     * Error = PS
     */
    {
        100,
        150,
        200,
        250,
        300,
        350,
        400
    },

    /*
     * Error = PM
     */
    {
        300,
        400,
        500,
        600,
        650,
        700,
        750
    },

    /*
     * Error = PB
     *
     * Temperature is far below SV.
     * Heater should be strong.
     */
    {
        700,
        800,
        850,
        900,
        950,
        1000,
        1000
    }
};

/* ============================================================================
 * Initialization
 * ========================================================================== */

void FB_FuzzyRule_Init(
    FB_FuzzyRule_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    /*
     * Load default Rule Table.
     */
    FB_FuzzyRule_LoadDefault(fb);

    /*
     * Reset calculation result.
     */
    FB_FuzzyRule_Reset(fb);

    fb->Enabled = true;

    fb->Initialized = true;
}

/* ============================================================================
 * Reset
 * ========================================================================== */

void FB_FuzzyRule_Reset(
    FB_FuzzyRule_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->Result.TotalWeight   = 0.0f;
    fb->Result.WeightedOutput = 0.0f;
    fb->Result.RuleOutput   = 0.0f;

    for (uint8_t i = 0U;
         i < FUZZY_RULE_SIZE;
         i++)
    {
        for (uint8_t j = 0U;
             j < FUZZY_RULE_SIZE;
             j++)
        {
            fb->Result.Weight[i][j] =
                0.0f;

            fb->Result.Contribution[i][j] =
                0.0f;
        }
    }
}

/* ============================================================================
 * Execute Rule Engine
 * ========================================================================== */

void FB_FuzzyRule_Run(
    FB_FuzzyRule_t *fb,
    const FB_FuzzyMembership_t *membership
)
{
    float totalWeight = 0.0f;
    float weightedOutput = 0.0f;

    if ((fb == NULL) ||
        (membership == NULL))
    {
        return;
    }

    if (!fb->Initialized)
    {
        FB_FuzzyRule_Init(fb);
    }

    if (!fb->Enabled)
    {
        FB_FuzzyRule_Reset(fb);
        return;
    }

    /*
     * ---------------------------------------------------------
     * 7 x 7 Rule Evaluation
     * ---------------------------------------------------------
     *
     * Weight =
     *
     * MIN(
     *     Error Membership,
     *     dError Membership
     * )
     *
     */

    for (uint8_t i = 0U;
         i < FUZZY_RULE_SIZE;
         i++)
    {
        for (uint8_t j = 0U;
             j < FUZZY_RULE_SIZE;
             j++)
        {
            float errorDegree;
            float dErrorDegree;
            float weight;
            float output;
            float contribution;

            /*
             * Error Membership.
             */
            errorDegree =
                membership->ErrorResult.Degree[i];

            /*
             * dError Membership.
             */
            dErrorDegree =
                membership->dErrorResult.Degree[j];

            /*
             * Mamdani AND:
             *
             * MIN(A,B)
             */
            weight =
                FuzzyRule_Min(
                    errorDegree,
                    dErrorDegree
                );

            weight =
                FuzzyRule_ClampWeight(
                    weight
                );

            /*
             * Rule output.
             */
            output =
                (float)fb->Table.RuleTable[i][j];

            /*
             * Weighted contribution.
             */
            contribution =
                weight * output;

            /*
             * Store diagnostic information.
             */
            fb->Result.Weight[i][j] =
                weight;

            fb->Result.Contribution[i][j] =
                contribution;

            /*
             * Accumulate.
             */
            totalWeight += weight;

            weightedOutput += contribution;
        }
    }

    /*
     * Store total weight.
     */
    fb->Result.TotalWeight =
        totalWeight;

    /*
     * Store weighted output.
     */
    fb->Result.WeightedOutput =
        weightedOutput;

    /*
     * Weighted average.
     *
     * This produces a crisp Rule output.
     */
    if (totalWeight > 0.000001f)
    {
        fb->Result.RuleOutput =
            weightedOutput /
            totalWeight;
    }
    else
    {
        /*
         * No active Rule.
         */
        fb->Result.RuleOutput = 0.0f;
    }
}

/* ============================================================================
 * Set One Rule
 * ========================================================================== */

bool FB_FuzzyRule_SetRule(
    FB_FuzzyRule_t *fb,
    uint8_t errorIndex,
    uint8_t dErrorIndex,
    int16_t output
)
{
    if (fb == NULL)
    {
        return false;
    }

    if (errorIndex >= FUZZY_RULE_SIZE)
    {
        return false;
    }

    if (dErrorIndex >= FUZZY_RULE_SIZE)
    {
        return false;
    }

    if (output < FUZZY_RULE_OUTPUT_MIN)
    {
        return false;
    }

    if (output > FUZZY_RULE_OUTPUT_MAX)
    {
        return false;
    }

    fb->Table.RuleTable[
        errorIndex
    ][
        dErrorIndex
    ] = output;

    return true;
}

/* ============================================================================
 * Get One Rule
 * ========================================================================== */

bool FB_FuzzyRule_GetRule(
    const FB_FuzzyRule_t *fb,
    uint8_t errorIndex,
    uint8_t dErrorIndex,
    int16_t *output
)
{
    if ((fb == NULL) ||
        (output == NULL))
    {
        return false;
    }

    if (errorIndex >= FUZZY_RULE_SIZE)
    {
        return false;
    }

    if (dErrorIndex >= FUZZY_RULE_SIZE)
    {
        return false;
    }

    *output =
        fb->Table.RuleTable[
            errorIndex
        ][
            dErrorIndex
        ];

    return true;
}

/* ============================================================================
 * Set Complete Rule Table
 * ========================================================================== */

bool FB_FuzzyRule_SetTable(
    FB_FuzzyRule_t *fb,
    const FuzzyRuleTable_t *table
)
{
    if ((fb == NULL) ||
        (table == NULL))
    {
        return false;
    }

    if (!FB_FuzzyRule_Validate(table))
    {
        return false;
    }

    fb->Table = *table;

    return true;
}

/* ============================================================================
 * Get Complete Rule Table
 * ========================================================================== */

bool FB_FuzzyRule_GetTable(
    const FB_FuzzyRule_t *fb,
    FuzzyRuleTable_t *table
)
{
    if ((fb == NULL) ||
        (table == NULL))
    {
        return false;
    }

    *table = fb->Table;

    return true;
}

/* ============================================================================
 * Load Default Rule Table
 * ========================================================================== */

void FB_FuzzyRule_LoadDefault(
    FB_FuzzyRule_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    for (uint8_t i = 0U;
         i < FUZZY_RULE_SIZE;
         i++)
    {
        for (uint8_t j = 0U;
             j < FUZZY_RULE_SIZE;
             j++)
        {
            fb->Table.RuleTable[i][j] =
                DefaultRuleTable[i][j];
        }
    }
}

/* ============================================================================
 * Validate Rule Table
 * ========================================================================== */

bool FB_FuzzyRule_Validate(
    const FuzzyRuleTable_t *table
)
{
    if (table == NULL)
    {
        return false;
    }

    for (uint8_t i = 0U;
         i < FUZZY_RULE_SIZE;
         i++)
    {
        for (uint8_t j = 0U;
             j < FUZZY_RULE_SIZE;
             j++)
        {
            int16_t value =
                table->RuleTable[i][j];

            if (value <
                FUZZY_RULE_OUTPUT_MIN)
            {
                return false;
            }

            if (value >
                FUZZY_RULE_OUTPUT_MAX)
            {
                return false;
            }
        }
    }

    return true;
}

/* ============================================================================
 * Enable
 * ========================================================================== */

void FB_FuzzyRule_Enable(
    FB_FuzzyRule_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->Enabled = true;
}

/* ============================================================================
 * Disable
 * ========================================================================== */

void FB_FuzzyRule_Disable(
    FB_FuzzyRule_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->Enabled = false;

    FB_FuzzyRule_Reset(fb);
}

/* ============================================================================
 * Is Enabled
 * ========================================================================== */

bool FB_FuzzyRule_IsEnabled(
    const FB_FuzzyRule_t *fb
)
{
    if (fb == NULL)
    {
        return false;
    }

    return fb->Enabled;
}

/* ============================================================================
 * Get Linear Rule Index
 * ========================================================================== */

uint8_t FB_FuzzyRule_GetIndex(
    uint8_t errorIndex,
    uint8_t dErrorIndex
)
{
    if (errorIndex >= FUZZY_RULE_SIZE)
    {
        return 0U;
    }

    if (dErrorIndex >= FUZZY_RULE_SIZE)
    {
        return 0U;
    }

    return (uint8_t)(
        errorIndex * FUZZY_RULE_SIZE +
        dErrorIndex
    );
}