import math



class HeaterModel:


    def __init__(self):


        self.Ts = 0.02



        #
        # Identified parameter
        #

        self.K = 0.25


        self.tau = 15.0



        self.dead_time = 1.2



        self.temperature = 25.0



        self.buffer=[]



        delay_samples = int(
            self.dead_time /
            self.Ts
        )


        for i in range(delay_samples):

            self.buffer.append(0)





    def update(
            self,
            pwm):



        #
        # Dead time
        #

        self.buffer.append(
            pwm
        )


        delayed_pwm = (
            self.buffer.pop(0)
        )



        power = (
            delayed_pwm /
            1000.0
        )



        #
        # Thermal equation
        #

        dT = (

            (
              self.K *
              power *
              300.0
            )

            -
            self.temperature

        ) * (

            self.Ts /
            self.tau

        )



        self.temperature += dT



        return self.temperature
