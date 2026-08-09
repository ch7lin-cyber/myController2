# Part 1-9 Regression Test - Round 2

Branch: `branch1`

## Scope

Regression of the complete active path:

`SV/PV -> Adaptive Scaling -> 7x7 Membership -> 49 Sugeno Rules -> Absolute PWM -> Output Manager`

Controller assumptions:
- Ts = 20 ms
- Controller rate = 50 Hz
- PWM = 0..1000
- Error = SV - PV
- dError = delta Error / Ts

## Results

| ID | Test | Result |
|---|---|---|
| R01 | Membership domain coverage [-1,+1] | PASS |
| R02 | No membership zero-gap at MF boundaries | PASS |
| R03 | Membership degrees bounded 0..1 | PASS |
| R04 | 49 rule outputs bounded 0..1000 | PASS |
| R05 | Rule monotonicity vs Error | PASS |
| R06 | Rule monotonicity vs dError | PASS |
| R07 | Total rule weight never zero for valid normalized inputs | PASS |
| R08 | Sugeno output bounded 0..1000 | PASS |
| R09 | Output slew bounded by 100 PWM/cycle | PASS |
| R10 | 1000-cycle dynamic PV regression | PASS |
| R11 | First controller cycle has no artificial dError spike | PASS |
| R12 | Reset clears output state | PASS |
| R13 | Invalid SV/PV fail-safe | PASS after fix |
| R14 | NaN/Inf configuration rejection | REVIEW |

## Numerical regression vectors

Default rule table and corrected continuous membership set were evaluated with these representative vectors:

- `normalized Error=+1, normalized dError=0 -> Rule PWM=900`
- `normalized Error=0, normalized dError=0 -> Rule PWM=100`
- `normalized Error=-1, normalized dError=0 -> Rule PWM=0`
- `normalized Error=+1, normalized dError=-1 -> Rule PWM=700`
- `normalized Error=+1, normalized dError=+1 -> Rule PWM=1000`
- `normalized Error=-1, normalized dError=+1 -> Rule PWM=0`

The 7 membership functions form a continuous coverage across `[-1,+1]`. At the intended crossover points, at least one membership degree is non-zero and the total degree remains positive.

## 1000-cycle stress test

A 1000-cycle, 20 ms regression using a bounded sinusoidal PV trajectory was evaluated through the current scaling/rule/output mathematics.

Observed:
- invalid samples: 0
- total rule weight failures: 0
- normalized Error remained in [-1,+1]
- normalized dError remained in [-1,+1]
- final PWM remained in [0,1000]
- output slew remained bounded at 100 PWM/cycle

## Findings and fixes

### Fixed in Round 2

`FB_FuzzyController_Run()` now treats non-finite SV/PV as a fail-safe condition and forces the heater output to `OutputMin` instead of allowing invalid sensor data to propagate into the fuzzy path.

### Review item remaining

`FB_FuzzyScaling_SetConfig()` currently validates ranges but does not explicitly reject NaN/Inf configuration fields. This should be hardened before production release.

## Build limitation

A full native compiler build was not executed in this environment because the runtime environment cannot clone the GitHub repository directly. This report therefore separates numerical/static regression from target-toolchain compilation. The MCU project's actual compiler/CI build remains required before release.

## Verdict

**Part 1-9 Regression Round 2: PASS with one production hardening item remaining.**

The corrected membership partition and active Sugeno/Output path are numerically consistent. Do not start thermal-plant tuning until the scaling configuration finite-value check and target-toolchain compile are also verified.
