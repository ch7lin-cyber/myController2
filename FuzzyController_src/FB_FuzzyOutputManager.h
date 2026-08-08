/******************************************************************************
 * File:
 *      FB_FuzzyOutputManager.h
 *
 * Brief:
 *      Fuzzy Correction + FeedForward PWM Manager
 *
 ******************************************************************************/

#ifndef FB_FUZZY_OUTPUT_MANAGER_H
#define FB_FUZZY_OUTPUT_MANAGER_H


#include <stdint.h>
#include <stdbool.h>


#define FUZZY_PWM_MIN          0
#define FUZZY_PWM_MAX          1000


#define FUZZY_FF_TABLE_SIZE    16



/*
 * FeedForward lookup table
 */

typedef struct
{
    float temperature;

    float pwm;

} FuzzyFFPoint_t;



/*
 * Configuration
 */

typedef struct
{

    /*
     * Fuzzy correction gain
     *
     * centroid:
     *
     * -1 ~ +1
     *
     * multiply this
     */

    float fuzzyScale;



    /*
     * PWM limit
     */

    float pwmMin;

    float pwmMax;



    /*
     * Output slew
     */

    float slewRate;



    /*
     * FeedForward table
     */

    FuzzyFFPoint_t ffTable[
        FUZZY_FF_TABLE_SIZE
    ];


    uint8_t ffSize;



    bool enableFeedForward;


    bool enableSlew;


}FuzzyOutputConfig_t;



/*
 * Runtime
 */

typedef struct
{


    float pwmFF;


    float fuzzyCorrection;


    float targetPWM;


    float outputPWM;



    float previousPWM;



}FuzzyOutputState_t;



/*
 * Function Block
 */

typedef struct
{

    FuzzyOutputConfig_t config;


    FuzzyOutputState_t state;


}FB_FuzzyOutputManager_t;



/*
 * Init
 */

void FB_FuzzyOutput_Init(
        FB_FuzzyOutputManager_t *fb);



/*
 * Main execute
 *
 * centroid:
 *
 * -1 ~ +1
 *
 * from Part4
 *
 */

float FB_FuzzyOutput_Run(
        FB_FuzzyOutputManager_t *fb,
        float sv,
        float pv,
        float centroid,
        float Ts);



/*
 * FeedForward calculation
 */

float FB_FuzzyOutput_CalcFF(
        FB_FuzzyOutputManager_t *fb,
        float temperature);



/*
 * Linear interpolation
 */

float FB_FuzzyOutput_Interpolation(
        FuzzyFFPoint_t *table,
        uint8_t size,
        float x);



/*
 * Slew limiter
 */

float FB_FuzzyOutput_Slew(
        float current,
        float target,
        float rate,
        float Ts);



/*
 * Configuration
 */

bool FB_FuzzyOutput_SetConfig(
        FB_FuzzyOutputManager_t *fb,
        FuzzyOutputConfig_t *cfg);



#endif