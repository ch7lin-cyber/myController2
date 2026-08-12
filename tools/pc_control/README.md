# PC Modbus TCP Fuzzy Control

This tool runs the existing C fuzzy controller on a PC and closes the loop through Modbus TCP.

## Default connection

- IP: `192.168.1.10`
- Port: `2502`
- Device ID: `1`
- PV holding register: `92`
- MV holding register: `108`
- Control period: `20 ms` (50 Hz)
- CSV logging period: `100 ms` (about 10 rows/s)
- PV format: signed int16, `raw * 0.1 = degC`
- MV format: Fuzzy PWM `0..1000` written directly to the register

The control period and CSV logging period are independent. Reducing the CSV rate does not change the fuzzy controller, Modbus read rate, or Modbus write rate.

All values can be overridden from the command line.

## 1. Build the C bridge

From repository root:

```bash
cmake -S . -B build -DMYCONTROLLER_BUILD_PC_CONTROL=ON
cmake --build build --config Debug
```

Typical bridge output:

- Windows MSVC: `build/Debug/fuzzy_pc_bridge.dll`
- Windows MinGW: `build/fuzzy_pc_bridge.dll`
- Linux: `build/libfuzzy_pc_bridge.so`
- macOS: `build/libfuzzy_pc_bridge.dylib`

The Python script searches these common locations automatically. You can also pass `--dll` explicitly.

## 2. Install Python dependency

```bash
python -m pip install -r tools/pc_control/requirements.txt
```

## 3. Start controller

```bash
python tools/pc_control/fuzzy_modbus_control.py
```

Recommended explicit command:

```bash
python tools/pc_control/fuzzy_modbus_control.py \
  --ip 192.168.1.10 \
  --port 2502 \
  --device-id 1 \
  --pv-address 92 \
  --mv-address 108 \
  --period-ms 20 \
  --log-period-ms 100 \
  --sv 100
```

With these settings:

```text
Control loop : 20 ms  = 50 Hz
CSV logging  : 100 ms = 10 Hz
```

The program always starts in `STOP` and writes `MV=0` before control is enabled.

## Terminal commands

```text
run
stop
sv 130
status
reset
quit
```

Example:

```text
cmd> sv 130
SV=130.000 C
cmd> run
RUN: SV=130.0 C
cmd> status
RUN SV=130.000 C PV=91.200 C MV=625 period=20.011 ms comm_errors=0
cmd> stop
STOP: MV will be forced to 0
cmd> quit
```

## CSV logging frequency

Use `--log-period-ms` to reduce CSV size without changing the closed-loop control period.

Examples:

```text
--period-ms 20 --log-period-ms 20    -> control 50 Hz, CSV 50 Hz
--period-ms 20 --log-period-ms 50    -> control 50 Hz, CSV 20 Hz
--period-ms 20 --log-period-ms 100   -> control 50 Hz, CSV 10 Hz
--period-ms 20 --log-period-ms 200   -> control 50 Hz, CSV 5 Hz
--period-ms 20 --log-period-ms 1000  -> control 50 Hz, CSV 1 Hz
```

`--log-period-ms` must be greater than or equal to `--period-ms`. The default is `100 ms`.

## CSV data

A timestamped CSV is created automatically. Important columns:

- `elapsed_s`: test time
- `mode`: RUN/STOP
- `sv_c`: set value
- `pv_raw`, `pv_c`: raw and engineering PV
- `error_c`, `derror_c_per_s`: controller error information
- `normalized_error`, `normalized_derror`: fuzzy inputs
- `rule_pwm`: Sugeno rule output before output management
- `final_pwm`: final C controller output
- `mv_write_raw`: actual Modbus value written
- `read_ms`, `write_ms`: Modbus latency
- `fuzzy_ms`: C controller execution time
- `actual_period_ms`: measured PC loop period
- `overrun_ms`: missed-period amount
- `comm_errors`: accumulated communication errors
- `status`: error text when a cycle fails

For analysis, provide the CSV after a run. At the default 100 ms logging period, one minute of testing produces about 600 data rows instead of about 3000 rows at 20 ms logging.

These columns allow analysis of rise time, overshoot, steady-state error, PWM saturation, rule behavior, derivative behavior, communication jitter, and 20 ms scheduling overruns.

## PV/MV scaling overrides

If PV is not 0.1 C / LSB:

```bash
--pv-scale 0.01
```

If PV is unsigned:

```bash
--pv-unsigned
```

If controller PWM must be scaled before writing MV:

```bash
--mv-scale 0.1 --mv-max 100
```

For the current expected `0..1000` MV register, keep the defaults.

## Safety behavior

- Program starts in STOP.
- `stop`, `quit`, Ctrl+C, Modbus error, or controller exception forces controller STOP.
- The program performs a best-effort `MV=0` write on faults and again before exit.
- RUN must be entered explicitly from the terminal.

This PC tool is intended for supervised engineering tests. Hardware-level over-temperature, sensor-disconnect, and output shutdown protections should remain active independently of the PC controller.
