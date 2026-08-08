from Heater_Model import HeaterModel



class FuzzySimulation:



    def __init__(self,
                 controller):


        self.controller = controller


        self.plant = HeaterModel()



        self.time=0



        self.log=[]





    def run(
            self,
            SV,
            duration):



        steps=int(
            duration /
            0.02
        )



        for k in range(steps):


            PV = (
                self.plant.temperature
            )


            pwm = (
                self.controller.run(
                    SV,
                    PV
                )
            )



            PV = (
                self.plant.update(
                    pwm
                )
            )



            self.log.append(
            {

            "time":
                self.time,


            "SV":
                SV,


            "PV":
                PV,


            "PWM":
                pwm

            })


            self.time +=0.02



        return self.log
