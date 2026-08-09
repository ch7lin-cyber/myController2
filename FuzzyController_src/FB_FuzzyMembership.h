/******************************************************************************
 * File    : FB_FuzzyMembership.h
 * Version : V2.0
 * Brief   : 7x7 Fuzzy Controller Membership Function Engine
 ******************************************************************************/
#ifndef FB_FUZZY_MEMBERSHIP_H
#define FB_FUZZY_MEMBERSHIP_H

#include <stdint.h>
#include <stdbool.h>
#include "ssm_std_define.h"

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_MF_COUNT              (7U)
#define FUZZY_INPUT_MIN             (-1.0f)
#define FUZZY_INPUT_MAX             ( 1.0f)
#define FUZZY_MEMBERSHIP_MIN        (0.0f)
#define FUZZY_MEMBERSHIP_MAX        (1.0f)

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

typedef enum
{
    FUZZY_MF_TRIANGLE = 0,
    FUZZY_MF_LEFT_SHOULDER,
    FUZZY_MF_RIGHT_SHOULDER
} FuzzyMFType_t;

typedef struct
{
    FuzzyMFType_t Type;
    float Left;
    float Center;
    float Right;
} FuzzyMembershipFunction_t;

typedef struct
{
    float Degree[FUZZY_MF_COUNT];
    uint8_t DominantIndex;
    float DominantDegree;
} FuzzyMembershipResult_t;

typedef struct
{
    FuzzyMembershipFunction_t MF[FUZZY_MF_COUNT];
} FuzzyMembershipSet_t;

typedef struct
{
    FuzzyMembershipSet_t Error;
    FuzzyMembershipSet_t dError;
} FuzzyMembershipConfig_t;

typedef struct
{
    FuzzyMembershipConfig_t Config;
    FuzzyMembershipResult_t ErrorResult;
    FuzzyMembershipResult_t dErrorResult;
    float ErrorInput;
    float dErrorInput;
    bool Initialized;
} FB_FuzzyMembership_t;

/* Initialization / reset */
MY_API void FB_FuzzyMembership_Init(FB_FuzzyMembership_t *fb);
MY_API void FB_FuzzyMembership_Reset(FB_FuzzyMembership_t *fb);

/* Main execution */
MY_API void FB_FuzzyMembership_Run(
    FB_FuzzyMembership_t *fb,
    float error,
    float dError);

/* Membership calculation */
MY_API float FB_FuzzyMembership_Calculate(
    const FuzzyMembershipFunction_t *mf,
    float x);

MY_API void FB_FuzzyMembership_CalculateSet(
    const FuzzyMembershipSet_t *set,
    float x,
    FuzzyMembershipResult_t *result);

/* Runtime configuration */
MY_API bool FB_FuzzyMembership_SetErrorMF(
    FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMFType_t type,
    float left,
    float center,
    float right);

MY_API bool FB_FuzzyMembership_SetDErrorMF(
    FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMFType_t type,
    float left,
    float center,
    float right);

MY_API bool FB_FuzzyMembership_GetErrorMF(
    const FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMembershipFunction_t *mf);

MY_API bool FB_FuzzyMembership_GetDErrorMF(
    const FB_FuzzyMembership_t *fb,
    uint8_t index,
    FuzzyMembershipFunction_t *mf);

MY_API bool FB_FuzzyMembership_SetConfig(
    FB_FuzzyMembership_t *fb,
    const FuzzyMembershipConfig_t *config);

MY_API bool FB_FuzzyMembership_GetConfig(
    const FB_FuzzyMembership_t *fb,
    FuzzyMembershipConfig_t *config);

/* Utility */
MY_API float FB_FuzzyMembership_ClampInput(float input);
MY_API const char *FB_FuzzyMembership_GetName(uint8_t index);
MY_API bool FB_FuzzyMembership_Validate(const FuzzyMembershipSet_t *set);

//------------------------------------------------------------------------------------//
// C++ compatibility
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------------//

#endif /* FB_FUZZY_MEMBERSHIP_H */
