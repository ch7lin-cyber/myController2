# Part 9 - Fuzzy Controller Verification

Branch: `branch1`

## Scope

Verification of the active control path:

`SV/PV -> Adaptive Scaling -> 7x7 Membership -> 49-rule Zero-order Sugeno -> Absolute PWM -> Output Manager`

Controller sample time: 20 ms (50 Hz).

## Reference configuration

- SV: 175 degC
- PWM range: 0..1000 (0.1% units)
- Base error window: 20 degC
- Adaptive error window: 1.5 x |Error|, limited by configured bounds
- Membership range: -1..+1
- Output slew: 5000 PWM/s = 100 PWM/cycle at Ts=20 ms
- Feed-forward: disabled by default

## Test vectors

| ID | SV (degC) | PV (degC) | dError | Normalized Error | Normalized dError | Rule PWM | First output PWM* | Result |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| T01 | 175 | 25 | 0 | +1.000 | 0.000 | 900 | 100 | PASS |
| T02 | 175 | 175 | 0 | 0.000 | 0.000 | 100 | 100 | PASS |
| T03 | 175 | 180 | 0 | -0.250 | 0.000 | 50 | 50 | PASS |
| T04 | 175 | 25 -> 26 | -50 degC/s | +1.000 | -1.000 | 700 | 200** | PASS |
| T05 | 175 | 175 -> 176 | -50 degC/s | -0.050 | -1.000 | near-zero / low output | decreases | PASS |
| T06 | 175 | 175 -> 174 | +50 degC/s | +0.050 | +1.000 | increases | increases | PASS |
| T07 | 175 | 25, constant PV | 0 | +1.000 | 0.000 | 900 | 100 -> 200 -> 300... | PASS |

\* First output includes the Output Manager's 100 PWM/cycle slew limit.

\*\* T04 starts from zero output, so the rule target is 700 but the first output is limited to 100; after the preceding 100-PWM cycle it becomes 200.

## Important numerical checks

### T01 - Large positive error

`SV=175, PV=25` gives `Error=+150 degC`.

Adaptive scaling drives the normalized error to `+1.0`. With `dError=0`, the active rule is `PB/ZE = 900`.

Conclusion: heater command is high for a large positive temperature error.

### T02 - Exact setpoint

`SV=175, PV=175` gives `Error=0`, `dError=0`.

The `ZE/ZE` singleton is `100`, therefore the steady-state baseline is 10% PWM.

Conclusion: the controller does not command zero heat at exact SV. This is intentional and must be validated against the real heater's steady-state heat loss.

### T03 - Temperature above SV

`SV=175, PV=180` gives `Error=-5 degC`, normalized error `-0.25`.

The resulting rule output is approximately 50 PWM (5%).

Conclusion: PWM decreases when PV is above SV.

### T04 - PV rising quickly while still far below SV

For `PV:25 -> 26 degC` in 20 ms:

`dError = -50 degC/s`.

The normalized dError reaches `-1.0`, reducing the rule command from the static 900-PWM condition to about 700 PWM.

Conclusion: the controller anticipates a fast approach and reduces heating demand.

### T05 - Crossing above SV

When PV crosses from 175 to 176 degC in one cycle, Error becomes negative while dError is negative. The rule table drives PWM strongly downward.

Conclusion: the derivative direction is consistent with heater control.

### T06 - Moving away below SV

When PV changes from 175 to 174 degC in one cycle, Error becomes positive and dError becomes positive. The rule table increases PWM.

Conclusion: the derivative direction is consistent with heater control.

## Rule-table safety checks

The 49-rule table was checked for:

- Every output in 0..1000: PASS
- Monotonic increase with positive Error: PASS
- Monotonic increase with positive dError: PASS
- `ZE/ZE = 100`: PASS
- `PB/ZE = 900`: PASS
- `NB/ZE = 0`: PASS
- `PB/PB = 1000`: PASS

## Membership checks

Default membership functions cover the complete `[-1,+1]` input range:

`NB NM NS ZE PS PM PB`

The adjacent triangular/shoulder functions overlap without gaps. At the normal crossover points, adjacent memberships provide continuous interpolation.

## Output Manager checks

- Absolute PWM input is clamped to 0..1000: PASS
- Default feed-forward is disabled: PASS
- Default FF blend is 0: PASS
- Output slew at 20 ms is 100 PWM/cycle: PASS
- Final output is clamped to configured PWM range: PASS

## Findings / follow-up

### F9-01 - Main timing is still prototype-level

`main.c` currently uses `delay_ms(20)`. The actual control period is therefore `20 ms + execution time`, not a guaranteed 20 ms.

**Action:** replace the blocking delay with a hardware timer or RTOS periodic task before production use.

### F9-02 - ZE/ZE baseline requires plant validation

The rule table intentionally commands 100 PWM (10%) at exact SV. This may be correct if approximately 10% heater duty balances steady-state heat loss, but it cannot be proven from software inspection alone.

**Action:** validate the required steady-state PWM at 175 degC using the real heater/thermal load.

### F9-03 - Output slew must be validated against the 8 s heating requirement

The current slew limit is 100 PWM/cycle. It takes approximately 180 ms to move from 0 to the 900-PWM large-error target, ignoring rule changes during heating.

**Action:** verify rise time and overshoot with the identified thermal model and then on hardware.

### F9-04 - No plant simulation is embedded yet

This Part 9 document validates the controller's numerical logic and rule direction. It does not claim that the controller already meets the physical requirements of 175 degC, +/-0.1 degC steady-state error, or <=1 degC overshoot.

**Action:** Part 10 should connect the controller to the identified thermal plant model and run step-response regression tests.

## Part 9 conclusion

The active fuzzy-control mathematics is internally consistent for a heater:

- positive Error -> more heating
- negative Error -> less heating
- positive dError -> more heating
- negative dError -> less heating
- exact SV -> 10% baseline PWM
- output remains bounded to 0..1000

No controller-source modification is required from these numerical checks. The remaining risks are plant-dependent: the 10% steady-state baseline, output slew, and real-time 20 ms scheduling.
