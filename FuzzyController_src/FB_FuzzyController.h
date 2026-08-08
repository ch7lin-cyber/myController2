/******************************************************************************
 * File    : FB_FuzzyController.h
 *
 * Version : V2.0
 *
 * Brief:
 *   IEC61131-3 Style Fuzzy Temperature Controller
 *
 ******************************************************************************/

#ifndef FB_FUZZY_CONTROLLER_H
#define FB_FUZZY_CONTROLLER_H


#include <stdint.h>
#include <stdbool.h>


#include "FB_FuzzyScaling.h"
#include "FB_FuzzyDefuzzifier.h"
#include "FB_FuzzyOutputManager.h"



#define FUZZY_CONTROLLER_TS      0.020f


#define FUZZY_INPUT_COUNT        7



/*
 * Linguistic Index
 *
 * NB NM NS ZE PS PM PB
 *
 */

typedef enum
{

    FUZZY_NB = 0,

    FUZZY_NM,

    FUZZY_NS,

    FUZZY_ZE,

    FUZZY_PS,

    FUZZY_PM,

    FUZZY_PB


}FuzzyTerm_t;




/*
 * Rule Table
 *
 * Input:
 *
 * Error
 * dError
 *
 * Output:
 *
 * NB~PB
 *
 */

typedef struct
{

    uint8_t table[7][7];


}FuzzyRuleTable_t;





/*
 * Controller Configuration
 */

typedef struct
{


    /*
     * Sample Time
     */

    float Ts;



    /*
     * Enable
     */

    bool Enable;



    /*
     * Output Limit
     */

    float OutputMin;

    float OutputMax;



}FuzzyControllerConfig_t;






/*
 * Runtime State
 */

typedef struct
{


    float SV;


    float PV;



    float Error;



    float dError;



    float PWM;



    float Centroid;



    bool initialized;



}FuzzyControllerState_t;





/*
 * Main Function Block
 */

typedef struct
{


    FuzzyControllerConfig_t config;



    FuzzyControllerState_t state;



    /*
     * Function Blocks
     */

    FB_FuzzyScaling_t scaling;


    FB_FuzzyDefuzzifier_t defuzz;


    FB_FuzzyOutputManager_t output;



    /*
     * Rule Table
     */

    FuzzyRuleTable_t rule;



}FB_FuzzyController_t;






/*
 * Initialize
 */

void FB_FuzzyController_Init(
        FB_FuzzyController_t *fb);




/*
 * Execute
 *
 * Called every 20ms
 *
 */

float FB_FuzzyController_Run(
        FB_FuzzyController_t *fb,
        float SV,
        float PV);




/*
 * Reset
 */

void FB_FuzzyController_Reset(
        FB_FuzzyController_t *fb);





/*
 * Default Rule
 */

void FB_FuzzyController_LoadDefaultRule(
        FB_FuzzyController_t *fb);




/*
 * Update Rule Runtime
 */

bool FB_FuzzyController_SetRule(
        FB_FuzzyController_t *fb,
        uint8_t errorIndex,
        uint8_t dErrorIndex,
        uint8_t outputIndex);




#endif