# Part 1-9 Regression Test - Round 3

Branch: `branch1`

## Purpose

Regression Round 3 verifies the active fuzzy-controller path after the Part 3
Adaptive Scaling V2.2 hardening for NaN / +/-Inf handling.

Active path:

`SV/PV -> Adaptive Scaling -> 7x7 Membership -> 49-rule zero-order Sugeno -> Absolute PWM -> Output Manager`

Controller period: 20 ms (50 Hz target).
PWM range: 0..1000.

> Scope note: this is a source-logic and independent numerical replay regression.
> It is not a target-MCU compiler/linker or hardware-in-loop result.

## Result summary

| ID | Test | Result |
|---|---|---|
| R3-01 | Controller initialization state | PASS |
| R3-02 | First-cycle derivative suppression | PASS |
| R3-03 | Membership full-range coverage | PASS |
| R3-04 | Membership zero-hole scan | PASS |
| R3-05 | Dense 2-D rule firing grid | PASS |
| R3-06 | Rule output bounds 0..1000 | PASS |
| R3-07 | Error monotonic rule property | PASS |
| R3-08 | dError monotonic rule property | PASS |
| R3-09 | Scaling normalized bounds | PASS |
| R3-10 | Output slew <= 100 PWM / 20 ms | PASS |
| R3-11 | 1000-cycle dynamic stress test | PASS |
| R3-12 | NaN PV fail-safe | PASS |
| R3-13 | +Inf / -Inf PV fail-safe | PASS |
| R3-14 | NaN / Inf SV fail-safe | PASS |
| R3-15 | Valid sample recovery after invalid input | PASS |
| R3-16 | Scaling SetConfig NaN / Inf rejection | PASS by source inspection |
| R3-17 | Scaling individual setter NaN / Inf rejection | PASS by source inspection |
| R3-18 | Controller reset and restart derivative behavior | PASS |

## R3-03 / R3-04 - Membership coverage

The V2.2 default membership partition was swept over the complete normalized
input range `[-1,+1]` using 20,001 input points.

Observed:

- minimum sum of the 7 membership degrees: `1.0`
- maximum sum of the 7 membership degrees: `1.0`
- zero-membership holes: `0`
- every individual membership degree remains within `[0,1]`

Result: PASS.

This confirms the Part 4 crossover-hole defect found in the previous audit is
removed from the current `branch1` implementation.

## R3-05 - Dense Error x dError rule-grid replay

A 401 x 401 grid was evaluated over normalized Error and normalized dError:

- combinations tested: 160,801
- minimum total firing weight: `1.0`
- zero total-weight cases: `0`
- minimum Sugeno rule output: `0`
- maximum Sugeno rule output: `1000`

Result: PASS.

The active rule engine therefore always has a valid denominator for the current
default membership partition.

## R3-07 / R3-08 - Rule-table monotonic properties

The default 49-rule heater table remains monotonic in both axes:

- increasing Error never decreases PWM
- increasing dError never decreases PWM
- all singleton outputs are in 0..1000

Result: PASS.

## R3-09 - Scaling normalized-output bounds

The active Adaptive Scaling path clamps:

- `NormalizedError` to `[-1,+1]`
- `NormalizedDError` to `[-1,+1]`
- ErrorWindow to configured limits
- Ke, Kde and Ku to configured limits

The Round 3 stress sequence produced no normalized-range violation.

Result: PASS.

## R3-10 - Output slew

Default Output Manager settings:

- slew rate = 5000 PWM/s
- Ts = 0.020 s

Therefore the maximum allowed command movement is 100 PWM per control cycle.

The 1000-cycle replay detected zero slew violations.

Result: PASS.

## R3-11 - 1000-cycle dynamic stress replay

A deterministic multi-frequency PV trajectory was applied for 1000 cycles
(20 seconds at 50 Hz) around a 175 degC setpoint.

Checks performed every cycle:

- finite controller output
- PWM in 0..1000
- output step <= 100 PWM
- normalized Error in -1..+1
- normalized dError in -1..+1
- positive Rule Engine TotalWeight

Failures observed: `0`.

Result: PASS.

## R3-12..R3-15 - Invalid process data and recovery

The current Controller fail-safe rejects invalid SV/PV before fuzzy execution.

Expected behavior:

- NaN PV -> PWM = OutputMin (default 0)
- +Inf PV -> PWM = OutputMin
- -Inf PV -> PWM = OutputMin
- NaN / +/-Inf SV -> PWM = OutputMin
- output-manager state is forced to OutputMin
- `firstRun` is set so the next valid sample cannot create a false derivative spike

Representative replay:

| Sample | SV | PV | Output |
|---|---:|---:|---:|
| 1 | 175 | 150 | 100 |
| 2 | 175 | NaN | 0 |
| 3 | 175 | 151 | 100 |
| 4 | +Inf | 151 | 0 |
| 5 | 175 | 152 | 100 |

The first valid cycle after an invalid sample restarts with derivative suppression.

Result: PASS.

## R3-16 - Scaling SetConfig finite validation

`FB_FuzzyScaling_SetConfig()` in V2.2 validates every floating-point
configuration field before accepting the structure, including:

- Ts
- MinTemperature / MaxTemperature
- BaseErrorWindow / MinErrorWindow / MaxErrorWindow
- MinKe / MaxKe
- MinKde / MaxKde
- MinKu / MaxKu
- DynamicGain
- MaxPVRate
- KuSlewRate

NaN and +/-Inf are rejected before the configuration is copied into the active
function block.

Result: PASS by source inspection.

## R3-17 - Individual setter finite validation

The V2.2 individual setters reject non-finite values for:

- Ke
- Kde
- Ku
- ErrorWindow

Result: PASS by source inspection.

## R3-18 - Reset behavior

Controller reset clears output state and marks the following execution as a
first cycle. The first valid sample then initializes PreviousError and PreviousPV
before Adaptive Scaling runs.

Result: PASS.

## Remaining non-regression limitations

### L3-01 - `main.c` timing remains prototype-level

The current application entry still uses `delay_ms(20)`. This is not a guaranteed
20 ms task period because sensor read, fuzzy execution and PWM API time are added
to the delay.

Production action: execute the controller from a 20 ms timer / RTOS periodic task.

### L3-02 - Physical thermal performance is not proven by Round 3

Round 3 verifies controller software mathematics and safety invariants. It does
not prove:

- <= 1 degC overshoot
- +/-0.1 degC steady-state accuracy
- required rise time
- correct 10% ZE/ZE holding PWM

Those are Part 10 plant-model / hardware validation items.

### L3-03 - No target toolchain compile was executed in this regression

The report does not claim GCC/MCUXpresso target compilation or linker success.
A production gate should include warning-clean target compilation and hardware
interface integration.

## Round 3 conclusion

After the Part 3 Scaling V2.2 hardening, the active Part 1-9 controller path
passes the Round 3 numerical and source-safety regression.

No new algorithmic defect was found in the active Part 1-9 path during this
round. The remaining work is real-time scheduling, target compilation, and
thermal-plant performance validation.
