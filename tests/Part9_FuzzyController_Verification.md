# Part 9 - Fuzzy Controller Verification (Audit Revision)

Branch: `branch1`

## Audit status

This document supersedes the first Part 9 report. The original report incorrectly stated that the default membership functions had continuous overlap. A source audit found zero-degree holes at the exact MF boundaries because adjacent triangles both returned zero at their shared endpoints.

The membership engine was corrected in Part 4/Part 9 audit to use a continuous 7-MF partition with evenly spaced centers and overlapping edge shoulders. The numerical vectors below are therefore the revised reference values.

## Active control path

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

## Revised membership definition

Centers/shoulders use approximately one-third spacing:

`NB NM NS ZE PS PM PB`

with the outer shoulders covering the endpoints and adjacent functions overlapping. The partition no longer produces `TotalWeight = 0` at normal crossover points.

## Revised numerical vectors

| ID | SV | PV / normalized Error | normalized dError | Rule PWM | First output PWM* | Result |
|---|---:|---:|---:|---:|---:|---|
| T01 | 175 | +1.000 | 0.000 | 900 | 100 | PASS |
| T02 | 175 | 0.000 | 0.000 | 100 | 100 | PASS |
| T03 | 175 | -0.250 | 0.000 | 62.5 | 62.5 | PASS |
| T04 | 175 | +1.000 | -1.000 | 700 | 100 | PASS |
| T05 | 175 | -0.050 | -1.000 | 0 | decreases | PASS |
| T06 | 175 | +0.050 | +1.000 | 272.5 | increases | PASS |
| T07 | 175 | +1.000 | 0.000 | 900 | 100 -> 200 -> 300... | PASS |

* First output includes the Output Manager's 100 PWM/cycle slew limit when starting from zero.

## Important checks

### T01 - Large positive error

`SV=175, PV=25` gives `Error=+150 degC`. Adaptive scaling drives normalized Error to `+1.0`. With `dError=0`, the active `PB/ZE` singleton is 900 PWM.

### T02 - Exact setpoint

`SV=175, PV=175` gives `Error=0`, `dError=0`. The `ZE/ZE` singleton is 100 PWM, so the controller has a 10% steady-state baseline. This must be validated against actual thermal loss.

### T03 - Temperature above SV

For the revised continuous membership set, `Error=-5 degC` normalizes to approximately `-0.25`. The weighted Sugeno result is 62.5 PWM rather than the old report's 50 PWM. The direction remains correct: PWM decreases above SV.

### T04 - PV rising quickly while still far below SV

For `PV:25 -> 26 degC` in 20 ms, `dError=-50 degC/s`. With the adaptive dError scaling, normalized dError reaches `-1.0`. At normalized Error `+1.0`, the rule output is 700 PWM, reducing heating demand while still applying substantial heat.

### T05 - Crossing above SV

For `PV:175 -> 176 degC`, normalized Error is approximately `-0.05` and normalized dError reaches `-1.0`. The current rule table produces a zero/near-zero command. This is intentionally aggressive and must be checked for chatter in the plant simulation.

### T06 - Moving away below SV

For `PV:175 -> 174 degC`, normalized Error is approximately `+0.05` and normalized dError reaches `+1.0`. The current rule table produces approximately 272.5 PWM, increasing heating demand.

## Rule-table safety checks

- Every output in 0..1000: PASS
- Monotonic increase with positive Error: PASS
- Monotonic increase with positive dError: PASS
- `ZE/ZE = 100`: PASS
- `PB/ZE = 900`: PASS
- `NB/ZE = 0`: PASS
- `PB/PB = 1000`: PASS

## Output Manager checks

- Absolute PWM input clamped to 0..1000: PASS
- Default feed-forward disabled: PASS
- Default FF blend 0: PASS
- Output slew at 20 ms = 100 PWM/cycle: PASS
- Final output clamped to configured PWM range: PASS

## Findings

### F9-01 - Fixed: membership crossover holes

The previous membership definition had points such as `x=-0.75` where both adjacent functions evaluated to zero. The Rule Engine then had `TotalWeight=0` and returned PWM=0. This was a real control-path bug.

**Action:** corrected in `FB_FuzzyMembership.c` with a continuous seven-set partition.

### F9-02 - Open issue: ZE/ZE baseline

The rule table commands 100 PWM (10%) at exact SV. This may be appropriate for the heater's steady-state heat loss but cannot be proven from software inspection.

### F9-03 - Open issue: derivative aggressiveness

The revised numerical checks show that a fast upward temperature crossing can drive the rule output to zero, while a fast downward movement can command a large PWM increase. This may be desirable, but plant simulation is required to verify overshoot and chatter.

### F9-04 - Open issue: main timing

`main.c` still uses `delay_ms(20)`. This is prototype timing, not a guaranteed 50 Hz periodic execution.

### F9-05 - Open issue: physical performance

No software-only verification can prove the requirements of +/-0.1 degC steady-state error, <=1 degC overshoot, or the specified heating/cooling times. Those require an identified plant model and hardware measurements.

## Conclusion

Parts 1-9 are internally consistent after the membership correction, but the controller should not yet be considered production-ready. The next verification step is closed-loop plant simulation using the measured thermal dynamics, followed by real hardware validation.
