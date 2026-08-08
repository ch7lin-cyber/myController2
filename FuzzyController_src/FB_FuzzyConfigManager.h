/******************************************************************************
 *
 * File:
 *      FB_FuzzyConfigManager.h
 *
 * Brief:
 *      Runtime Fuzzy Parameter Manager
 *
 ******************************************************************************/

#ifndef FB_FUZZY_CONFIG_MANAGER_H
#define FB_FUZZY_CONFIG_MANAGER_H


#include <stdint.h>
#include <stdbool.h>



#define FUZZY_CONFIG_VERSION      0x0200



#define FUZZY_RULE_SIZE           49



#define FUZZY_CONFIG_MAGIC        0x55AA





/*
 * Membership Config
 */


typedef struct
{

    float Left;

    float Center;

    float Right;

    float Peak;


}FuzzyMFConfig_t;




/*
 * Scaling Config
 */

typedef struct
{


    float errorScale;


    float dErrorScale;


    float Ku;



}FuzzyScalingConfig_t;






/*
 * FeedForward Point
 */


typedef struct
{

    float temperature;


    float pwm;


}FuzzyFFConfig_t;







/*
 * Complete Configuration
 */

typedef struct
{


    uint16_t magic;


    uint16_t version;



    bool enable;



    /*
     * 7 Membership
     */

    FuzzyMFConfig_t MF[7];



    /*
     * 49 Rule
     */

    uint8_t rule[7][7];



    /*
     * Scaling
     */

    FuzzyScalingConfig_t scaling;



    /*
     * FeedForward
     */

    FuzzyFFConfig_t ff[16];


    uint8_t ffSize;



}FuzzyRuntimeConfig_t;





/*
 * Runtime Manager
 */

typedef struct
{


    FuzzyRuntimeConfig_t config;



    bool changed;



}FB_FuzzyConfigManager_t;






/*
 * Initialize
 */

void FB_FuzzyConfig_Init(
        FB_FuzzyConfigManager_t *fb);




/*
 * Load default
 */

void FB_FuzzyConfig_LoadDefault(
        FB_FuzzyConfigManager_t *fb);





/*
 * Validate
 */

bool FB_FuzzyConfig_Check(
        FB_FuzzyConfigManager_t *fb);






/*
 * Apply configuration
 *
 * Send data into
 *
 * Fuzzy Controller
 *
 */

bool FB_FuzzyConfig_Apply(
        FB_FuzzyConfigManager_t *cfg,
        FB_FuzzyController_t *controller);






/*
 * Set Rule
 */

bool FB_FuzzyConfig_SetRule(
        FB_FuzzyConfigManager_t *fb,
        uint8_t e,
        uint8_t de,
        uint8_t output);





/*
 * Set Membership
 */

bool FB_FuzzyConfig_SetMF(
        FB_FuzzyConfigManager_t *fb,
        uint8_t index,
        FuzzyMFConfig_t *mf);





#endif