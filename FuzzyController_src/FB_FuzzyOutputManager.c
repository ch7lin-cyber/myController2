#include "FB_FuzzyOutputManager.h"



static float clamp(
        float x,
        float min,
        float max)
{

    if(x<min)
        return min;

    if(x>max)
        return max;


    return x;

}



/*
 * Init
 */

void FB_FuzzyOutput_Init(
        FB_FuzzyOutputManager_t *fb)
{

    if(fb==0)
        return;



    fb->config.fuzzyScale =
            300.0f;



    fb->config.pwmMin =
            FUZZY_PWM_MIN;


    fb->config.pwmMax =
            FUZZY_PWM_MAX;



    fb->config.slewRate =
            5000.0f;



    fb->config.enableFeedForward=
            true;


    fb->config.enableSlew=
            true;



    fb->state.outputPWM=0;


    fb->state.previousPWM=0;

}



/*
 * FeedForward lookup
 */

float FB_FuzzyOutput_CalcFF(
        FB_FuzzyOutputManager_t *fb,
        float temperature)
{


    if(fb->config.enableFeedForward==false)
        return 0;



    return
    FB_FuzzyOutput_Interpolation(
        fb->config.ffTable,
        fb->config.ffSize,
        temperature);


}



/*
 * Linear interpolation
 */

float FB_FuzzyOutput_Interpolation(
        FuzzyFFPoint_t *table,
        uint8_t size,
        float x)
{

    uint8_t i;


    if(size==0)
        return 0;



    if(x<=table[0].temperature)
        return table[0].pwm;



    for(i=0;i<size-1;i++)
    {

        if(x<=table[i+1].temperature)
        {


            float dx;


            dx =
            table[i+1].temperature -
            table[i].temperature;



            return
            table[i].pwm
            +
            (
              (x-table[i].temperature)
              /
              dx
            )
            *
            (
              table[i+1].pwm -
              table[i].pwm
            );

        }

    }



    return table[size-1].pwm;

}



/*
 * Slew limiter
 */

float FB_FuzzyOutput_Slew(
        float current,
        float target,
        float rate,
        float Ts)
{

    float diff;


    float maxStep;



    diff =
    target-current;



    maxStep =
    rate*Ts;



    if(diff>maxStep)
        diff=maxStep;


    if(diff<-maxStep)
        diff=-maxStep;



    return current+diff;

}





/*
 * Main
 */

float FB_FuzzyOutput_Run(
        FB_FuzzyOutputManager_t *fb,
        float sv,
        float pv,
        float centroid,
        float Ts)
{


    float pwmFF;

    float fuzzyPWM;

    float target;



    /*
     * 1.
     * FeedForward
     */

    pwmFF =
    FB_FuzzyOutput_CalcFF(
            fb,
            sv);



    fb->state.pwmFF =
            pwmFF;



    /*
     * 2.
     * Fuzzy correction
     *
     * centroid:
     *
     * -1 ~ +1
     *
     */

    fuzzyPWM =
    centroid *
    fb->config.fuzzyScale;



    fb->state.fuzzyCorrection =
            fuzzyPWM;



    /*
     * 3.
     * Combine
     */

    target =
    pwmFF +
    fuzzyPWM;



    /*
     * Limit
     */

    target =
    clamp(
        target,
        fb->config.pwmMin,
        fb->config.pwmMax);



    fb->state.targetPWM =
            target;



    /*
     * 4.
     * Slew
     */

    if(fb->config.enableSlew)
    {

        fb->state.outputPWM =
        FB_FuzzyOutput_Slew(
            fb->state.outputPWM,
            target,
            fb->config.slewRate,
            Ts);

    }
    else
    {

        fb->state.outputPWM=
                target;

    }



    fb->state.previousPWM =
            fb->state.outputPWM;



    return fb->state.outputPWM;

}