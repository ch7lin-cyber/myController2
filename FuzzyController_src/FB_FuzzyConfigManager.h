/******************************************************************************
 * File  : FB_FuzzyConfigManager.h
 * Brief : Runtime Fuzzy Parameter Manager
 ******************************************************************************/
#ifndef FB_FUZZY_CONFIG_MANAGER_H
#define FB_FUZZY_CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "ssm_std_define.h"
#include "FB_FuzzyController.h"

//------------------------------------------------------------------------------------//
// C++ compatibility // DO NOT DELETE
#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------------------------------------//

#define FUZZY_CONFIG_VERSION          0x0200
#define FUZZY_CONFIG_RULE_COUNT       49U
#define FUZZY_CONFIG_MAGIC            0x55AA

typedef struct
{
    float Left;
    float Center;
    float Right;
    float Peak;
} FuzzyMFConfig_t;

typedef struct
{
    float errorScale;
    float dErrorScale;
    float Ku;
} FuzzyRuntimeScalingConfig_t;

typedef struct
{
    float temperature;
    float pwm;
} FuzzyFFConfig_t;

typedef struct
{
    uint16_t magic;
    uint16_t version;
    bool enable;
    FuzzyMFConfig_t MF[7];
    int16_t rule[7][7];
    FuzzyRuntimeScalingConfig_t scaling;
    FuzzyFFConfig_t ff[16];
    uint8_t ffSize;
} FuzzyRuntimeConfig_t;

typedef struct
{
    FuzzyRuntimeConfig_t config;
    bool changed;
} FB_FuzzyConfigManager_t;

MY_API void FB_FuzzyConfig_Init(FB_FuzzyConfigManager_t *fb);
MY_API void FB_FuzzyConfig_LoadDefault(FB_FuzzyConfigManager_t *fb);
MY_API bool FB_FuzzyConfig_Check(FB_FuzzyConfigManager_t *fb);
MY_API bool FB_FuzzyConfig_Apply(FB_FuzzyConfigManager_t *cfg, FB_FuzzyController_t *controller);
MY_API bool FB_FuzzyConfig_SetRule(FB_FuzzyConfigManager_t *fb, uint8_t e, uint8_t de, int16_t output);
MY_API bool FB_FuzzyConfig_SetMF(FB_FuzzyConfigManager_t *fb, uint8_t index, FuzzyMFConfig_t *mf);

//------------------------------------------------------------------------------------//
// C++ compatibility
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------------//

#endif /* FB_FUZZY_CONFIG_MANAGER_H */
