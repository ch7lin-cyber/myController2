/******************************************************************************
 * File  : FB_FuzzyConfigManager.c
 * Brief : Runtime Fuzzy Parameter Manager implementation
 *
 * Mapping policy:
 *   - MF[7] is a common membership set applied to Error and dError.
 *   - scaling.{errorScale,dErrorScale,Ku} are fixed/manual gains.
 *     Applying this configuration disables Auto/Adaptive scaling so that the
 *     configured values are not overwritten on the next control cycle.
 *   - ff[] updates the OutputManager feed-forward table only. Existing FF
 *     enable/blend policy is preserved because FuzzyRuntimeConfig_t does not
 *     contain enableFeedForward or ffBlend fields.
 *   - Rule singleton outputs are absolute PWM commands in the range 0..1000.
 ******************************************************************************/

#include "FB_FuzzyConfigManager.h"

#include <float.h>
#include <stddef.h>

#define FUZZY_CONFIG_PEAK_VALUE       (1.0f)
#define FUZZY_CONFIG_FLOAT_EPSILON    (0.000001f)

static bool FuzzyConfig_IsFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX));
}

static float FuzzyConfig_Abs(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static FuzzyMFType_t FuzzyConfig_GetMFType(uint8_t index)
{
    if (index == FUZZY_NB)
        return FUZZY_MF_LEFT_SHOULDER;

    if (index == FUZZY_PB)
        return FUZZY_MF_RIGHT_SHOULDER;

    return FUZZY_MF_TRIANGLE;
}

static bool FuzzyConfig_ValidateMF(const FuzzyMFConfig_t *mf,
                                   uint8_t index)
{
    FuzzyMembershipFunction_t candidate;

    if ((mf == NULL) || (index >= FUZZY_MF_COUNT))
        return false;

    if (!FuzzyConfig_IsFinite(mf->Left) ||
        !FuzzyConfig_IsFinite(mf->Center) ||
        !FuzzyConfig_IsFinite(mf->Right) ||
        !FuzzyConfig_IsFinite(mf->Peak))
        return false;

    /* Current membership engine supports unit-height MFs only. */
    if (FuzzyConfig_Abs(mf->Peak - FUZZY_CONFIG_PEAK_VALUE) >
        FUZZY_CONFIG_FLOAT_EPSILON)
        return false;

    candidate.Type = FuzzyConfig_GetMFType(index);
    candidate.Left = mf->Left;
    candidate.Center = mf->Center;
    candidate.Right = mf->Right;

    if ((candidate.Left < FUZZY_INPUT_MIN) ||
        (candidate.Left > FUZZY_INPUT_MAX) ||
        (candidate.Center < FUZZY_INPUT_MIN) ||
        (candidate.Center > FUZZY_INPUT_MAX) ||
        (candidate.Right < FUZZY_INPUT_MIN) ||
        (candidate.Right > FUZZY_INPUT_MAX))
        return false;

    if (candidate.Type == FUZZY_MF_TRIANGLE)
        return ((candidate.Left < candidate.Center) &&
                (candidate.Center < candidate.Right));

    if (candidate.Type == FUZZY_MF_LEFT_SHOULDER)
        return (candidate.Left < candidate.Center);

    return (candidate.Center < candidate.Right);
}

static bool FuzzyConfig_BuildMembershipSet(
    const FB_FuzzyConfigManager_t *fb,
    FuzzyMembershipSet_t *set)
{
    uint8_t i;

    if ((fb == NULL) || (set == NULL))
        return false;

    for (i = 0U; i < FUZZY_MF_COUNT; ++i)
    {
        if (!FuzzyConfig_ValidateMF(&fb->config.MF[i], i))
            return false;

        set->MF[i].Type = FuzzyConfig_GetMFType(i);
        set->MF[i].Left = fb->config.MF[i].Left;
        set->MF[i].Center = fb->config.MF[i].Center;
        set->MF[i].Right = fb->config.MF[i].Right;
    }

    return FB_FuzzyMembership_Validate(set);
}

static void FuzzyConfig_BuildRuleTable(
    const FB_FuzzyConfigManager_t *fb,
    FuzzyRuleTable_t *table)
{
    uint8_t e;
    uint8_t de;

    for (e = 0U; e < FUZZY_RULE_SIZE; ++e)
    {
        for (de = 0U; de < FUZZY_RULE_SIZE; ++de)
            table->RuleTable[e][de] = fb->config.rule[e][de];
    }
}

static bool FuzzyConfig_ValidateScaling(
    const FuzzyRuntimeScalingConfig_t *scaling)
{
    if (scaling == NULL)
        return false;

    if (!FuzzyConfig_IsFinite(scaling->errorScale) ||
        !FuzzyConfig_IsFinite(scaling->dErrorScale) ||
        !FuzzyConfig_IsFinite(scaling->Ku))
        return false;

    if ((scaling->errorScale < FUZZY_SCALING_MIN_KE) ||
        (scaling->errorScale > FUZZY_SCALING_MAX_KE))
        return false;

    if ((scaling->dErrorScale < FUZZY_SCALING_MIN_KDE) ||
        (scaling->dErrorScale > FUZZY_SCALING_MAX_KDE))
        return false;

    if ((scaling->Ku < FUZZY_SCALING_MIN_KU) ||
        (scaling->Ku > FUZZY_SCALING_MAX_KU))
        return false;

    return true;
}

static bool FuzzyConfig_ValidateFF(const FuzzyRuntimeConfig_t *config)
{
    uint8_t i;

    if (config == NULL)
        return false;

    if (config->ffSize > FUZZY_FF_TABLE_SIZE)
        return false;

    for (i = 0U; i < config->ffSize; ++i)
    {
        if (!FuzzyConfig_IsFinite(config->ff[i].temperature) ||
            !FuzzyConfig_IsFinite(config->ff[i].pwm))
            return false;

        if ((config->ff[i].pwm < FUZZY_PWM_MIN) ||
            (config->ff[i].pwm > FUZZY_PWM_MAX))
            return false;

        if ((i > 0U) &&
            (config->ff[i].temperature <= config->ff[i - 1U].temperature))
            return false;
    }

    return true;
}

void FB_FuzzyConfig_Init(FB_FuzzyConfigManager_t *fb)
{
    if (fb == NULL)
        return;

    FB_FuzzyConfig_LoadDefault(fb);
}

void FB_FuzzyConfig_LoadDefault(FB_FuzzyConfigManager_t *fb)
{
    FB_FuzzyController_t defaults;
    uint8_t i;
    uint8_t e;
    uint8_t de;

    if (fb == NULL)
        return;

    FB_FuzzyController_Init(&defaults);

    fb->config.magic = FUZZY_CONFIG_MAGIC;
    fb->config.version = FUZZY_CONFIG_VERSION;
    fb->config.enable = defaults.config.Enable;

    for (i = 0U; i < FUZZY_MF_COUNT; ++i)
    {
        fb->config.MF[i].Left = defaults.membership.Config.Error.MF[i].Left;
        fb->config.MF[i].Center = defaults.membership.Config.Error.MF[i].Center;
        fb->config.MF[i].Right = defaults.membership.Config.Error.MF[i].Right;
        fb->config.MF[i].Peak = FUZZY_CONFIG_PEAK_VALUE;
    }

    for (e = 0U; e < FUZZY_RULE_SIZE; ++e)
    {
        for (de = 0U; de < FUZZY_RULE_SIZE; ++de)
            fb->config.rule[e][de] =
                defaults.ruleEngine.Table.RuleTable[e][de];
    }

    fb->config.scaling.errorScale = defaults.scaling.State.Ke;
    fb->config.scaling.dErrorScale = defaults.scaling.State.Kde;
    fb->config.scaling.Ku = defaults.scaling.State.Ku;

    fb->config.ffSize = defaults.output.config.ffSize;
    for (i = 0U; i < FUZZY_FF_TABLE_SIZE; ++i)
    {
        fb->config.ff[i].temperature =
            defaults.output.config.ffTable[i].temperature;
        fb->config.ff[i].pwm = defaults.output.config.ffTable[i].pwm;
    }

    fb->changed = false;
}

bool FB_FuzzyConfig_Check(FB_FuzzyConfigManager_t *fb)
{
    FuzzyMembershipSet_t membershipSet;
    FuzzyRuleTable_t ruleTable;

    if (fb == NULL)
        return false;

    if ((fb->config.magic != FUZZY_CONFIG_MAGIC) ||
        (fb->config.version != FUZZY_CONFIG_VERSION))
        return false;

    if (!FuzzyConfig_BuildMembershipSet(fb, &membershipSet))
        return false;

    FuzzyConfig_BuildRuleTable(fb, &ruleTable);
    if (!FB_FuzzyRule_Validate(&ruleTable))
        return false;

    if (!FuzzyConfig_ValidateScaling(&fb->config.scaling))
        return false;

    if (!FuzzyConfig_ValidateFF(&fb->config))
        return false;

    return true;
}

bool FB_FuzzyConfig_Apply(FB_FuzzyConfigManager_t *cfg,
                          FB_FuzzyController_t *controller)
{
    FB_FuzzyMembership_t membershipCandidate;
    FB_FuzzyRule_t ruleCandidate;
    FB_FuzzyScaling_t scalingCandidate;
    FB_FuzzyOutputManager_t outputCandidate;
    FuzzyMembershipConfig_t membershipConfig;
    FuzzyMembershipSet_t commonSet;
    FuzzyRuleTable_t ruleTable;
    FuzzyOutputConfig_t outputConfig;
    uint8_t i;

    if ((cfg == NULL) || (controller == NULL))
        return false;

    if (!FB_FuzzyConfig_Check(cfg))
        return false;

    /* Build all candidates first. Nothing is committed until every check passes. */
    membershipCandidate = controller->membership;
    ruleCandidate = controller->ruleEngine;
    scalingCandidate = controller->scaling;
    outputCandidate = controller->output;

    if (!FuzzyConfig_BuildMembershipSet(cfg, &commonSet))
        return false;

    membershipConfig = membershipCandidate.Config;
    membershipConfig.Error = commonSet;
    membershipConfig.dError = commonSet;
    if (!FB_FuzzyMembership_SetConfig(&membershipCandidate, &membershipConfig))
        return false;

    FuzzyConfig_BuildRuleTable(cfg, &ruleTable);
    if (!FB_FuzzyRule_SetTable(&ruleCandidate, &ruleTable))
        return false;

    if ((cfg->config.scaling.errorScale < scalingCandidate.Config.MinKe) ||
        (cfg->config.scaling.errorScale > scalingCandidate.Config.MaxKe) ||
        (cfg->config.scaling.dErrorScale < scalingCandidate.Config.MinKde) ||
        (cfg->config.scaling.dErrorScale > scalingCandidate.Config.MaxKde) ||
        (cfg->config.scaling.Ku < scalingCandidate.Config.MinKu) ||
        (cfg->config.scaling.Ku > scalingCandidate.Config.MaxKu))
        return false;

    FB_FuzzyScaling_DisableAuto(&scalingCandidate);
    FB_FuzzyScaling_DisableAdaptive(&scalingCandidate);

    if (!FB_FuzzyScaling_SetKe(&scalingCandidate,
                               cfg->config.scaling.errorScale) ||
        !FB_FuzzyScaling_SetKde(&scalingCandidate,
                                cfg->config.scaling.dErrorScale) ||
        !FB_FuzzyScaling_SetKu(&scalingCandidate,
                               cfg->config.scaling.Ku))
        return false;

    /* Apply fixed scaling immediately; targets are kept equal for later cycles. */
    scalingCandidate.State.Ke = cfg->config.scaling.errorScale;
    scalingCandidate.State.Kde = cfg->config.scaling.dErrorScale;
    scalingCandidate.State.Ku = cfg->config.scaling.Ku;

    outputConfig = outputCandidate.config;
    outputConfig.ffSize = cfg->config.ffSize;

    for (i = 0U; i < FUZZY_FF_TABLE_SIZE; ++i)
    {
        if (i < cfg->config.ffSize)
        {
            outputConfig.ffTable[i].temperature = cfg->config.ff[i].temperature;
            outputConfig.ffTable[i].pwm = cfg->config.ff[i].pwm;
        }
        else
        {
            outputConfig.ffTable[i].temperature = 0.0f;
            outputConfig.ffTable[i].pwm = 0.0f;
        }
    }

    if (!FB_FuzzyOutput_SetConfig(&outputCandidate, &outputConfig))
        return false;

    /* Atomic commit of the validated candidate blocks. */
    controller->membership = membershipCandidate;
    controller->ruleEngine = ruleCandidate;
    controller->scaling = scalingCandidate;
    controller->output = outputCandidate;
    controller->config.Enable = cfg->config.enable;
    controller->state.firstRun = true;

    cfg->changed = false;
    return true;
}

bool FB_FuzzyConfig_SetRule(FB_FuzzyConfigManager_t *fb,
                            uint8_t e,
                            uint8_t de,
                            int16_t output)
{
    FuzzyRuleTable_t table;
    int16_t oldValue;

    if (fb == NULL)
        return false;

    if ((e >= FUZZY_RULE_SIZE) || (de >= FUZZY_RULE_SIZE))
        return false;

    if ((output < FUZZY_RULE_OUTPUT_MIN) ||
        (output > FUZZY_RULE_OUTPUT_MAX))
        return false;

    oldValue = fb->config.rule[e][de];
    fb->config.rule[e][de] = output;

    FuzzyConfig_BuildRuleTable(fb, &table);
    if (!FB_FuzzyRule_Validate(&table))
    {
        fb->config.rule[e][de] = oldValue;
        return false;
    }

    fb->changed = true;
    return true;
}

bool FB_FuzzyConfig_SetMF(FB_FuzzyConfigManager_t *fb,
                          uint8_t index,
                          FuzzyMFConfig_t *mf)
{
    FuzzyMFConfig_t oldValue;
    FuzzyMembershipSet_t set;

    if ((fb == NULL) || (mf == NULL) || (index >= FUZZY_MF_COUNT))
        return false;

    if (!FuzzyConfig_ValidateMF(mf, index))
        return false;

    oldValue = fb->config.MF[index];
    fb->config.MF[index] = *mf;

    if (!FuzzyConfig_BuildMembershipSet(fb, &set))
    {
        fb->config.MF[index] = oldValue;
        return false;
    }

    fb->changed = true;
    return true;
}
