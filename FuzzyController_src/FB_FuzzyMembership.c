/******************************************************************************
 * File    : FB_FuzzyMembership.c
 * Version : V2.2
 * Brief   : 7x7 Fuzzy Membership Function Engine
 *
 * The default set is a continuous fuzzy partition over [-1,+1].
 ******************************************************************************/

#include "FB_FuzzyMembership.h"
#include <stddef.h>

#define FUZZY_EPSILON (0.000001f)
#define FUZZY_THIRD   (0.3333333333f)
#define FUZZY_TWO_THIRD (0.6666666667f)

static float Fuzzy_Abs(float x)
{
    return (x >= 0.0f) ? x : -x;
}

static float Fuzzy_Clamp01(float x)
{
    if (x < FUZZY_MEMBERSHIP_MIN) return FUZZY_MEMBERSHIP_MIN;
    if (x > FUZZY_MEMBERSHIP_MAX) return FUZZY_MEMBERSHIP_MAX;
    return x;
}

static float Fuzzy_Triangle(float x, float left, float center, float right)
{
    float result;
    if ((center <= left) || (right <= center)) return 0.0f;
    if ((x <= left) || (x >= right)) return 0.0f;
    if (Fuzzy_Abs(x - center) <= FUZZY_EPSILON) return 1.0f;
    if (x < center)
        result = (x - left) / (center - left);
    else
        result = (right - x) / (right - center);
    return Fuzzy_Clamp01(result);
}

static float Fuzzy_LeftShoulder(float x, float left, float center)
{
    float result;
    if (center <= left) return 0.0f;
    if (x <= left) return 1.0f;
    if (x >= center) return 0.0f;
    result = (center - x) / (center - left);
    return Fuzzy_Clamp01(result);
}

static float Fuzzy_RightShoulder(float x, float center, float right)
{
    float result;
    if (right <= center) return 0.0f;
    if (x <= center) return 0.0f;
    if (x >= right) return 1.0f;
    result = (x - center) / (right - center);
    return Fuzzy_Clamp01(result);
}

static void Fuzzy_LoadDefaultSet(FuzzyMembershipSet_t *set)
{
    if (set == NULL) return;

    /*
     * Seven labels with evenly spaced centers:
     *   NB  NM  NS  ZE  PS  PM  PB
     *   -2/3 -2/3 -1/3 0 +1/3 +2/3 +2/3
     *
     * NB/PB are shoulders; the adjacent triangles overlap their shoulders.
     * This removes the zero-degree holes present in the previous definition.
     */
    set->MF[FUZZY_NB].Type   = FUZZY_MF_LEFT_SHOULDER;
    set->MF[FUZZY_NB].Left   = -1.00f;
    set->MF[FUZZY_NB].Center = -FUZZY_TWO_THIRD;
    set->MF[FUZZY_NB].Right  = -FUZZY_THIRD;

    set->MF[FUZZY_NM].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_NM].Left   = -1.00f;
    set->MF[FUZZY_NM].Center = -FUZZY_TWO_THIRD;
    set->MF[FUZZY_NM].Right  = -FUZZY_THIRD;

    set->MF[FUZZY_NS].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_NS].Left   = -FUZZY_TWO_THIRD;
    set->MF[FUZZY_NS].Center = -FUZZY_THIRD;
    set->MF[FUZZY_NS].Right  = 0.00f;

    set->MF[FUZZY_ZE].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_ZE].Left   = -FUZZY_THIRD;
    set->MF[FUZZY_ZE].Center = 0.00f;
    set->MF[FUZZY_ZE].Right  = FUZZY_THIRD;

    set->MF[FUZZY_PS].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_PS].Left   = 0.00f;
    set->MF[FUZZY_PS].Center = FUZZY_THIRD;
    set->MF[FUZZY_PS].Right  = FUZZY_TWO_THIRD;

    set->MF[FUZZY_PM].Type   = FUZZY_MF_TRIANGLE;
    set->MF[FUZZY_PM].Left   = FUZZY_THIRD;
    set->MF[FUZZY_PM].Center = FUZZY_TWO_THIRD;
    set->MF[FUZZY_PM].Right  = 1.00f;

    set->MF[FUZZY_PB].Type   = FUZZY_MF_RIGHT_SHOULDER;
    set->MF[FUZZY_PB].Left   = FUZZY_THIRD;
    set->MF[FUZZY_PB].Center = FUZZY_TWO_THIRD;
    set->MF[FUZZY_PB].Right  = 1.00f;
}

void FB_FuzzyMembership_Init(FB_FuzzyMembership_t *fb)
{
    uint8_t i;
    if (fb == NULL) return;
    fb->ErrorInput = 0.0f;
    fb->dErrorInput = 0.0f;
    fb->ErrorResult.DominantIndex = FUZZY_ZE;
    fb->ErrorResult.DominantDegree = 0.0f;
    fb->dErrorResult.DominantIndex = FUZZY_ZE;
    fb->dErrorResult.DominantDegree = 0.0f;
    for (i = 0U; i < FUZZY_MF_COUNT; ++i)
    {
        fb->ErrorResult.Degree[i] = 0.0f;
        fb->dErrorResult.Degree[i] = 0.0f;
    }
    Fuzzy_LoadDefaultSet(&fb->Config.Error);
    Fuzzy_LoadDefaultSet(&fb->Config.dError);
    fb->Initialized = true;
}

void FB_FuzzyMembership_Reset(FB_FuzzyMembership_t *fb)
{
    uint8_t i;
    if (fb == NULL) return;
    fb->ErrorInput = 0.0f;
    fb->dErrorInput = 0.0f;
    fb->ErrorResult.DominantIndex = FUZZY_ZE;
    fb->ErrorResult.DominantDegree = 0.0f;
    fb->dErrorResult.DominantIndex = FUZZY_ZE;
    fb->dErrorResult.DominantDegree = 0.0f;
    for (i = 0U; i < FUZZY_MF_COUNT; ++i)
    {
        fb->ErrorResult.Degree[i] = 0.0f;
        fb->dErrorResult.Degree[i] = 0.0f;
    }
}

float FB_FuzzyMembership_ClampInput(float input)
{
    if (input != input) return 0.0f;
    if (input < FUZZY_INPUT_MIN) return FUZZY_INPUT_MIN;
    if (input > FUZZY_INPUT_MAX) return FUZZY_INPUT_MAX;
    return input;
}

float FB_FuzzyMembership_Calculate(const FuzzyMembershipFunction_t *mf, float x)
{
    if (mf == NULL) return 0.0f;
    x = FB_FuzzyMembership_ClampInput(x);
    switch (mf->Type)
    {
        case FUZZY_MF_TRIANGLE: return Fuzzy_Triangle(x, mf->Left, mf->Center, mf->Right);
        case FUZZY_MF_LEFT_SHOULDER: return Fuzzy_LeftShoulder(x, mf->Left, mf->Center);
        case FUZZY_MF_RIGHT_SHOULDER: return Fuzzy_RightShoulder(x, mf->Center, mf->Right);
        default: return 0.0f;
    }
}

void FB_FuzzyMembership_CalculateSet(const FuzzyMembershipSet_t *set,
                                     float x,
                                     FuzzyMembershipResult_t *result)
{
    float maxDegree = -1.0f;
    uint8_t maxIndex = FUZZY_ZE;
    uint8_t i;
    if ((set == NULL) || (result == NULL)) return;
    x = FB_FuzzyMembership_ClampInput(x);
    for (i = 0U; i < FUZZY_MF_COUNT; ++i)
    {
        result->Degree[i] = FB_FuzzyMembership_Calculate(&set->MF[i], x);
        if (result->Degree[i] > maxDegree)
        {
            maxDegree = result->Degree[i];
            maxIndex = i;
        }
    }
    result->DominantIndex = maxIndex;
    result->DominantDegree = (maxDegree < 0.0f) ? 0.0f : maxDegree;
}

void FB_FuzzyMembership_Run(FB_FuzzyMembership_t *fb, float error, float dError)
{
    if (fb == NULL) return;
    if (!fb->Initialized) FB_FuzzyMembership_Init(fb);
    fb->ErrorInput = FB_FuzzyMembership_ClampInput(error);
    fb->dErrorInput = FB_FuzzyMembership_ClampInput(dError);
    FB_FuzzyMembership_CalculateSet(&fb->Config.Error, fb->ErrorInput, &fb->ErrorResult);
    FB_FuzzyMembership_CalculateSet(&fb->Config.dError, fb->dErrorInput, &fb->dErrorResult);
}

static bool Fuzzy_ValidateFunction(const FuzzyMembershipFunction_t *mf)
{
    if (mf == NULL) return false;
    if ((mf->Left < FUZZY_INPUT_MIN) || (mf->Left > FUZZY_INPUT_MAX) ||
        (mf->Center < FUZZY_INPUT_MIN) || (mf->Center > FUZZY_INPUT_MAX) ||
        (mf->Right < FUZZY_INPUT_MIN) || (mf->Right > FUZZY_INPUT_MAX)) return false;
    switch (mf->Type)
    {
        case FUZZY_MF_TRIANGLE: return (mf->Left < mf->Center) && (mf->Center < mf->Right);
        case FUZZY_MF_LEFT_SHOULDER: return (mf->Left < mf->Center);
        case FUZZY_MF_RIGHT_SHOULDER: return (mf->Center < mf->Right);
        default: return false;
    }
}

bool FB_FuzzyMembership_SetErrorMF(FB_FuzzyMembership_t *fb, uint8_t index,
                                   FuzzyMFType_t type, float left, float center, float right)
{
    FuzzyMembershipFunction_t old, candidate;
    if ((fb == NULL) || (index >= FUZZY_MF_COUNT)) return false;
    old = fb->Config.Error.MF[index];
    candidate.Type = type; candidate.Left = left; candidate.Center = center; candidate.Right = right;
    if (!Fuzzy_ValidateFunction(&candidate)) return false;
    fb->Config.Error.MF[index] = candidate;
    if (!FB_FuzzyMembership_Validate(&fb->Config.Error)) { fb->Config.Error.MF[index] = old; return false; }
    return true;
}

bool FB_FuzzyMembership_SetDErrorMF(FB_FuzzyMembership_t *fb, uint8_t index,
                                    FuzzyMFType_t type, float left, float center, float right)
{
    FuzzyMembershipFunction_t old, candidate;
    if ((fb == NULL) || (index >= FUZZY_MF_COUNT)) return false;
    old = fb->Config.dError.MF[index];
    candidate.Type = type; candidate.Left = left; candidate.Center = center; candidate.Right = right;
    if (!Fuzzy_ValidateFunction(&candidate)) return false;
    fb->Config.dError.MF[index] = candidate;
    if (!FB_FuzzyMembership_Validate(&fb->Config.dError)) { fb->Config.dError.MF[index] = old; return false; }
    return true;
}

bool FB_FuzzyMembership_GetErrorMF(const FB_FuzzyMembership_t *fb, uint8_t index, FuzzyMembershipFunction_t *mf)
{
    if ((fb == NULL) || (mf == NULL) || (index >= FUZZY_MF_COUNT)) return false;
    *mf = fb->Config.Error.MF[index]; return true;
}

bool FB_FuzzyMembership_GetDErrorMF(const FB_FuzzyMembership_t *fb, uint8_t index, FuzzyMembershipFunction_t *mf)
{
    if ((fb == NULL) || (mf == NULL) || (index >= FUZZY_MF_COUNT)) return false;
    *mf = fb->Config.dError.MF[index]; return true;
}

bool FB_FuzzyMembership_SetConfig(FB_FuzzyMembership_t *fb, const FuzzyMembershipConfig_t *config)
{
    if ((fb == NULL) || (config == NULL)) return false;
    if (!FB_FuzzyMembership_Validate(&config->Error)) return false;
    if (!FB_FuzzyMembership_Validate(&config->dError)) return false;
    fb->Config = *config; return true;
}

bool FB_FuzzyMembership_GetConfig(const FB_FuzzyMembership_t *fb, FuzzyMembershipConfig_t *config)
{
    if ((fb == NULL) || (config == NULL)) return false;
    *config = fb->Config; return true;
}

bool FB_FuzzyMembership_Validate(const FuzzyMembershipSet_t *set)
{
    uint8_t i;
    if (set == NULL) return false;
    if (set->MF[FUZZY_NB].Type != FUZZY_MF_LEFT_SHOULDER) return false;
    if (set->MF[FUZZY_PB].Type != FUZZY_MF_RIGHT_SHOULDER) return false;
    if (set->MF[FUZZY_NB].Left != FUZZY_INPUT_MIN) return false;
    if (set->MF[FUZZY_PB].Right != FUZZY_INPUT_MAX) return false;
    for (i = 0U; i < FUZZY_MF_COUNT; ++i)
        if (!Fuzzy_ValidateFunction(&set->MF[i])) return false;

    for (i = 0U; i + 1U < FUZZY_MF_COUNT; ++i)
    {
        const FuzzyMembershipFunction_t *a = &set->MF[i];
        const FuzzyMembershipFunction_t *b = &set->MF[i + 1U];
        float aEnd = (a->Type == FUZZY_MF_LEFT_SHOULDER) ? a->Center : a->Right;
        float bStart = (b->Type == FUZZY_MF_RIGHT_SHOULDER) ? b->Center : b->Left;
        if (!(a->Center < b->Center)) return false;
        if ((aEnd - bStart) <= FUZZY_EPSILON) return false;
    }
    return true;
}

const char *FB_FuzzyMembership_GetName(uint8_t index)
{
    switch (index)
    {
        case FUZZY_NB: return "NB";
        case FUZZY_NM: return "NM";
        case FUZZY_NS: return "NS";
        case FUZZY_ZE: return "ZE";
        case FUZZY_PS: return "PS";
        case FUZZY_PM: return "PM";
        case FUZZY_PB: return "PB";
        default: return "UNKNOWN";
    }
}
