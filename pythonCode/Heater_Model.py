import math


class HeaterModel:
    """Legacy first-order heater simulation model.

    This is a lightweight Python simulation helper only. The production
    identified plant used by Part 10 is ControllPlant/myPlant_1.c.
    """

    def __init__(self):
        self.Ts = 0.02

        # Legacy approximate parameters.
        self.K = 0.25
        self.tau = 15.0
        self.dead_time = 1.2

        self.ambient = 25.0
        self.temperature = self.ambient

        self.buffer = []
        delay_samples = int(self.dead_time / self.Ts)
        for _ in range(delay_samples):
            self.buffer.append(0.0)

    def update(self, pwm):
        # Clamp to the same controller convention: 0..1000 = 0..100%.
        pwm = max(0.0, min(1000.0, float(pwm)))

        # Dead time.
        self.buffer.append(pwm)
        delayed_pwm = self.buffer.pop(0)
        power = delayed_pwm / 1000.0

        # First-order thermal equation referenced to ambient temperature.
        # At zero heater power the model now returns toward ambient, not 0 C.
        target_temperature = self.ambient + self.K * power * 300.0
        dT = (target_temperature - self.temperature) * (self.Ts / self.tau)
        self.temperature += dT

        return self.temperature
