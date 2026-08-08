Main ExecuteFB_FuzzyController_t HeaterFuzzy;


int main(void)
{


FB_FuzzyController_Init(
        &HeaterFuzzy);



while(1)
{


float pwm;


pwm =
FB_FuzzyController_Run(
        &HeaterFuzzy,
        175.0f,
        temperature);



PWM_SetDuty(
        (uint16_t)pwm);



delay_ms(20);


}


}