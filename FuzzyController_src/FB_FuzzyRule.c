#include "FB_FuzzyRule.h"
#include <stddef.h>

#define FUZZY_RULE_EPSILON (0.000001f)

static float FuzzyRule_Min(float a, float b)
{
    return (a < b) ? a : b;
}

static float FuzzyRule_ClampWeight(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

/* Zero-order Sugeno rule table. Output: 0..1000 = 0..100.0% PWM. */
static const int16_t DefaultRuleTable[FUZZY_RULE_SIZE][FUZZY_RULE_SIZE] =
{
    {   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,  50,  50,  75, 100 },
    {   0,  25,  50, 100, 150, 200, 250 },
    { 100, 150, 200, 250, 300, 350, 400 },
    { 300, 400, 500, 600, 650, 700, 750 },
    { 700, 800, 850, 900, 950,1000,1000 }
};

void FB_FuzzyRule_Init(FB_FuzzyRule_t *fb)
{
    if (fb == NULL) return;
    FB_FuzzyRule_LoadDefault(fb);
    FB_FuzzyRule_Reset(fb);
    fb->Enabled = true;
    fb->Initialized = true;
}

void FB_FuzzyRule_Reset(FB_FuzzyRule_t *fb)
{
    uint8_t i, j;
    if (fb == NULL) return;
    fb->Result.TotalWeight = 0.0f;
    fb->Result.WeightedOutput = 0.0f;
    fb->Result.RuleOutput = 0.0f;
    for (i = 0U; i < FUZZY_RULE_SIZE; ++i)
        for (j = 0U; j < FUZZY_RULE_SIZE; ++j)
        {
            fb->Result.Weight[i][j] = 0.0f;
            fb->Result.Contribution[i][j] = 0.0f;
        }
}

void FB_FuzzyRule_Run(FB_FuzzyRule_t *fb,
                      const FB_FuzzyMembership_t *membership)
{
    uint8_t i, j;
    float totalWeight = 0.0f;
    float weightedOutput = 0.0f;

    if ((fb == NULL) || (membership == NULL)) return;
    if (!fb->Initialized) FB_FuzzyRule_Init(fb);
    if (!fb->Enabled) { FB_FuzzyRule_Reset(fb); return; }

    for (i = 0U; i < FUZZY_RULE_SIZE; ++i)
        for (j = 0U; j < FUZZY_RULE_SIZE; ++j)
        {
            float errorDegree = membership->ErrorResult.Degree[i];
            float dErrorDegree = membership->dErrorResult.Degree[j];
            float weight = FuzzyRule_ClampWeight(
                FuzzyRule_Min(errorDegree, dErrorDegree));
            float output = (float)fb->Table.RuleTable[i][j];
            float contribution = weight * output;

            fb->Result.Weight[i][j] = weight;
            fb->Result.Contribution[i][j] = contribution;
            totalWeight += weight;
            weightedOutput += contribution;
        }

    fb->Result.TotalWeight = totalWeight;
    fb->Result.WeightedOutput = weightedOutput;
    fb->Result.RuleOutput = (totalWeight > FUZZY_RULE_EPSILON) ?
        weightedOutput / totalWeight : 0.0f;

    if (fb->Result.RuleOutput < (float)FUZZY_RULE_OUTPUT_MIN)
        fb->Result.RuleOutput = (float)FUZZY_RULE_OUTPUT_MIN;
    if (fb->Result.RuleOutput > (float)FUZZY_RULE_OUTPUT_MAX)
        fb->Result.RuleOutput = (float)FUZZY_RULE_OUTPUT_MAX;
}

bool FB_FuzzyRule_SetRule(FB_FuzzyRule_t *fb,
                          uint8_t errorIndex,
                          uint8_t dErrorIndex,
                          int16_t output)
{
    int16_t oldValue;
    if (fb == NULL) return false;
    if (errorIndex >= FUZZY_RULE_SIZE || dErrorIndex >= FUZZY_RULE_SIZE) return false;
    if (output < FUZZY_RULE_OUTPUT_MIN || output > FUZZY_RULE_OUTPUT_MAX) return false;

    oldValue = fb->Table.RuleTable[errorIndex][dErrorIndex];
    fb->Table.RuleTable[errorIndex][dErrorIndex] = output;
    if (!FB_FuzzyRule_Validate(&fb->Table))
    {
        fb->Table.RuleTable[errorIndex][dErrorIndex] = oldValue;
        return false;
    }
    return true;
}

bool FB_FuzzyRule_GetRule(const FB_FuzzyRule_t *fb,
                          uint8_t errorIndex,
                          uint8_t dErrorIndex,
                          int16_t *output)
{
    if ((fb == NULL) || (output == NULL)) return false;
    if (errorIndex >= FUZZY_RULE_SIZE || dErrorIndex >= FUZZY_RULE_SIZE) return false;
    *output = fb->Table.RuleTable[errorIndex][dErrorIndex];
    return true;
}

bool FB_FuzzyRule_SetTable(FB_FuzzyRule_t *fb,
                           const FuzzyRuleTable_t *table)
{
    if ((fb == NULL) || (table == NULL)) return false;
    if (!FB_FuzzyRule_Validate(table)) return false;
    fb->Table = *table;
    return true;
}

bool FB_FuzzyRule_GetTable(const FB_FuzzyRule_t *fb,
                           FuzzyRuleTable_t *table)
{
    if ((fb == NULL) || (table == NULL)) return false;
    *table = fb->Table;
    return true;
}

void FB_FuzzyRule_LoadDefault(FB_FuzzyRule_t *fb)
{
    uint8_t i, j;
    if (fb == NULL) return;
    for (i = 0U; i < FUZZY_RULE_SIZE; ++i)
        for (j = 0U; j < FUZZY_RULE_SIZE; ++j)
            fb->Table.RuleTable[i][j] = DefaultRuleTable[i][j];
}

bool FB_FuzzyRule_Validate(const FuzzyRuleTable_t *table)
{
    uint8_t i, j;
    if (table == NULL) return false;

    for (i = 0U; i < FUZZY_RULE_SIZE; ++i)
        for (j = 0U; j < FUZZY_RULE_SIZE; ++j)
            if (table->RuleTable[i][j] < FUZZY_RULE_OUTPUT_MIN ||
                table->RuleTable[i][j] > FUZZY_RULE_OUTPUT_MAX)
                return false;

    /* Heater safety property: larger positive Error cannot reduce PWM. */
    for (i = 0U; i + 1U < FUZZY_RULE_SIZE; ++i)
        for (j = 0U; j < FUZZY_RULE_SIZE; ++j)
            if (table->RuleTable[i + 1U][j] < table->RuleTable[i][j])
                return false;

    /* Larger dError cannot reduce PWM. */
    for (i = 0U; i < FUZZY_RULE_SIZE; ++i)
        for (j = 0U; j + 1U < FUZZY_RULE_SIZE; ++j)
            if (table->RuleTable[i][j + 1U] < table->RuleTable[i][j])
                return false;

    return true;
}

void FB_FuzzyRule_Enable(FB_FuzzyRule_t *fb)
{
    if (fb != NULL) fb->Enabled = true;
}

void FB_FuzzyRule_Disable(FB_FuzzyRule_t *fb)
{
    if (fb == NULL) return;
    fb->Enabled = false;
    FB_FuzzyRule_Reset(fb);
}

bool FB_FuzzyRule_IsEnabled(const FB_FuzzyRule_t *fb)
{
    return (fb != NULL) ? fb->Enabled : false;
}

uint8_t FB_FuzzyRule_GetIndex(uint8_t errorIndex, uint8_t dErrorIndex)
{
    if (errorIndex >= FUZZY_RULE_SIZE || dErrorIndex >= FUZZY_RULE_SIZE)
        return 0U;
    return (uint8_t)(errorIndex * FUZZY_RULE_SIZE + dErrorIndex);
}
