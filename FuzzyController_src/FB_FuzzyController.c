#include "FB_FuzzyController.h"



/*
 * Initialize
 */

void FB_FuzzyController_Init(
        FB_FuzzyController_t *fb)
{

    if(fb==0)
        return;



    fb->config.Ts =
            FUZZY_CONTROLLER_TS;


    fb->config.Enable =
            true;


    fb->config.OutputMin =
            0;


    fb->config.OutputMax =
            1000;



    FB_FuzzyScaling_Init(
            &fb->scaling);



    FB_FuzzyDefuzzifier_Init(
            &fb->defuzz);



    FB_FuzzyOutput_Init(
            &fb->output);




    FB_FuzzyController_LoadDefaultRule(
            fb);



    fb->state.initialized =
            true;

}
