#!/usr/bin/env python3
"""PC closed-loop controller for myController2.

Flow:
    Modbus TCP PV -> C Fuzzy Controller (ctypes) -> Modbus TCP MV

Terminal commands:
    run
    stop
    sv <degC>
    status
    reset
    quit

Safety policy:
    - Starts in STOP.
    - STOP commands MV=0.
    - Communication or controller exceptions force STOP and best-effort MV=0.
    - Ctrl+C also forces MV=0 before exit.
"""

from __future__ import annotations

import argparse
import csv
import ctypes
import math
import os
import queue
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from pymodbus.client import ModbusTcpClient


@dataclass
class RuntimeState:
    run: bool = False
    sv_c: float = 100.0
    quit: bool = False
    reset_requested: bool = False
    force_zero_write: bool = True
    comm_errors: int = 0
    cycle: int = 0
    last_pv_c: float = math.nan
    last_mv: int = 0
    last_period_ms: float = 0.0


class FuzzyLibrary:
    def __init__(self, library_path: Path, sample_time_ms: int) -> None:
        self.lib = ctypes.CDLL(str(library_path))

        self.lib.FuzzyPc_Init.argtypes = [ctypes.c_uint32]
        self.lib.FuzzyPc_Init.restype = ctypes.c_int
        self.lib.FuzzyPc_SetSampleTime.argtypes = [ctypes.c_uint32]
        self.lib.FuzzyPc_SetSampleTime.restype = ctypes.c_int
        self.lib.FuzzyPc_GetSampleTime.argtypes = []
        self.lib.FuzzyPc_GetSampleTime.restype = ctypes.c_uint32
        self.lib.FuzzyPc_Reset.argtypes = []
        self.lib.FuzzyPc_Reset.restype = None
        self.lib.FuzzyPc_SetEnable.argtypes = [ctypes.c_int]
        self.lib.FuzzyPc_SetEnable.restype = None
        self.lib.FuzzyPc_GetEnable.argtypes = []
        self.lib.FuzzyPc_GetEnable.restype = ctypes.c_int
        self.lib.FuzzyPc_Run.argtypes = [ctypes.c_float, ctypes.c_float]
        self.lib.FuzzyPc_Run.restype = ctypes.c_float

        for name in (
            "FuzzyPc_GetError",
            "FuzzyPc_GetDError",
            "FuzzyPc_GetNormalizedError",
            "FuzzyPc_GetNormalizedDError",
            "FuzzyPc_GetRulePWM",
            "FuzzyPc_GetPWM",
            "FuzzyPc_GetCentroid",
        ):
            fn = getattr(self.lib, name)
            fn.argtypes = []
            fn.restype = ctypes.c_float

        if not self.lib.FuzzyPc_Init(sample_time_ms):
            raise RuntimeError(f"FuzzyPc_Init({sample_time_ms}) failed")

    def set_enable(self, enable: bool) -> None:
        self.lib.FuzzyPc_SetEnable(1 if enable else 0)

    def reset(self) -> None:
        self.lib.FuzzyPc_Reset()

    def run(self, sv_c: float, pv_c: float) -> float:
        return float(self.lib.FuzzyPc_Run(float(sv_c), float(pv_c)))

    def diagnostics(self) -> dict[str, float]:
        return {
            "error_c": float(self.lib.FuzzyPc_GetError()),
            "derror_c_per_s": float(self.lib.FuzzyPc_GetDError()),
            "normalized_error": float(self.lib.FuzzyPc_GetNormalizedError()),
            "normalized_derror": float(self.lib.FuzzyPc_GetNormalizedDError()),
            "rule_pwm": float(self.lib.FuzzyPc_GetRulePWM()),
            "pwm": float(self.lib.FuzzyPc_GetPWM()),
            "centroid": float(self.lib.FuzzyPc_GetCentroid()),
        }


class ModbusIO:
    def __init__(self, host: str, port: int, device_id: int, timeout_s: float) -> None:
        self.client = ModbusTcpClient(host=host, port=port, timeout=timeout_s)
        self.device_id = device_id

    def connect(self) -> bool:
        return bool(self.client.connect())

    def close(self) -> None:
        self.client.close()

    def read_holding(self, address: int) -> int:
        try:
            response = self.client.read_holding_registers(
                address=address, count=1, device_id=self.device_id
            )
        except TypeError:
            # PyModbus <= 3.9 compatibility.
            response = self.client.read_holding_registers(
                address=address, count=1, slave=self.device_id
            )

        if response.isError():
            raise IOError(f"Modbus read error at address {address}: {response}")
        if not getattr(response, "registers", None):
            raise IOError(f"Empty Modbus read response at address {address}")
        return int(response.registers[0])

    def write_register(self, address: int, value: int) -> None:
        value = max(0, min(0xFFFF, int(value)))
        try:
            response = self.client.write_register(
                address=address, value=value, device_id=self.device_id
            )
        except TypeError:
            response = self.client.write_register(
                address=address, value=value, slave=self.device_id
            )

        if response.isError():
            raise IOError(f"Modbus write error at address {address}: {response}")


def uint16_to_int16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def find_default_library(repo_root: Path) -> Optional[Path]:
    names = (
        "fuzzy_pc_bridge.dll",
        "libfuzzy_pc_bridge.so",
        "libfuzzy_pc_bridge.dylib",
    )
    candidates = []
    for directory in (
        repo_root / "build" / "Debug",
        repo_root / "build" / "Release",
        repo_root / "build",
        repo_root / "build-default" / "Debug",
        repo_root / "build-default",
        repo_root / "build-shared" / "Debug",
        repo_root / "build-shared",
    ):
        for name in names:
            candidates.append(directory / name)
    return next((p for p in candidates if p.exists()), None)


def command_worker(commands: "queue.Queue[str]") -> None:
    while True:
        try:
            line = input("cmd> ").strip()
        except EOFError:
            commands.put("quit")
            return
        if line:
            commands.put(line)
        if line.lower() in {"quit", "exit", "q"}:
            return


def process_commands(
    commands: "queue.Queue[str]",
    state: RuntimeState,
    fuzzy: FuzzyLibrary,
) -> None:
    while True:
        try:
            line = commands.get_nowait()
        except queue.Empty:
            return

        parts = line.split()
        cmd = parts[0].lower()

        if cmd == "run":
            state.run = True
            fuzzy.set_enable(True)
            print(f"RUN: SV={state.sv_c:.1f} C")
        elif cmd == "stop":
            state.run = False
            fuzzy.set_enable(False)
            state.force_zero_write = True
            state.last_mv = 0
            print("STOP: MV will be forced to 0")
        elif cmd == "sv" and len(parts) == 2:
            try:
                new_sv = float(parts[1])
            except ValueError:
                print("Usage: sv <temperature_C>")
                continue
            if not math.isfinite(new_sv):
                print("SV must be finite")
                continue
            state.sv_c = new_sv
            print(f"SV={state.sv_c:.3f} C")
        elif cmd == "reset":
            fuzzy.reset()
            state.reset_requested = False
            print("Fuzzy controller reset")
        elif cmd == "status":
            mode = "RUN" if state.run else "STOP"
            print(
                f"{mode} SV={state.sv_c:.3f} C "
                f"PV={state.last_pv_c:.3f} C MV={state.last_mv} "
                f"period={state.last_period_ms:.3f} ms "
                f"comm_errors={state.comm_errors}"
            )
        elif cmd in {"help", "?"}:
            print("Commands: run | stop | sv <C> | status | reset | quit")
        elif cmd in {"quit", "exit", "q"}:
            state.quit = True
            state.run = False
            fuzzy.set_enable(False)
            state.force_zero_write = True
            return
        else:
            print("Unknown command. Use: run | stop | sv <C> | status | reset | quit")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Modbus TCP + C Fuzzy closed-loop controller")
    parser.add_argument("--ip", default="192.168.1.10")
    parser.add_argument("--port", type=int, default=2502)
    parser.add_argument("--device-id", type=int, default=1)
    parser.add_argument("--pv-address", type=int, default=92)
    parser.add_argument("--mv-address", type=int, default=108)
    parser.add_argument("--period-ms", type=int, default=20)
    parser.add_argument("--sv", type=float, default=100.0)
    parser.add_argument(
        "--pv-scale",
        type=float,
        default=0.1,
        help="PV engineering-unit scale; default raw int16 * 0.1 C",
    )
    parser.add_argument(
        "--pv-unsigned",
        action="store_true",
        help="Treat PV register as uint16 instead of int16",
    )
    parser.add_argument(
        "--mv-scale",
        type=float,
        default=1.0,
        help="Controller PWM-to-register scale; default PWM 0..1000 writes 0..1000",
    )
    parser.add_argument("--mv-min", type=int, default=0)
    parser.add_argument("--mv-max", type=int, default=1000)
    parser.add_argument("--timeout", type=float, default=0.5)
    parser.add_argument("--csv", default="")
    parser.add_argument("--dll", default="")
    return parser


def main() -> int:
    args = build_parser().parse_args()

    if not (1 <= args.period_ms <= 6000):
        print("ERROR: --period-ms must be 1..6000", file=sys.stderr)
        return 2
    if not math.isfinite(args.sv):
        print("ERROR: --sv must be finite", file=sys.stderr)
        return 2
    if args.mv_min < 0 or args.mv_max > 0xFFFF or args.mv_min >= args.mv_max:
        print("ERROR: invalid MV limits", file=sys.stderr)
        return 2

    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent.parent
    library_path = Path(args.dll).resolve() if args.dll else find_default_library(repo_root)
    if library_path is None or not library_path.exists():
        print(
            "ERROR: fuzzy_pc_bridge library not found. Build it first or use --dll <path>.",
            file=sys.stderr,
        )
        return 2

    csv_path = (
        Path(args.csv).resolve()
        if args.csv
        else (Path.cwd() / f"fuzzy_control_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
    )

    fuzzy = FuzzyLibrary(library_path, args.period_ms)
    fuzzy.set_enable(False)
    state = RuntimeState(run=False, sv_c=args.sv)
    modbus = ModbusIO(args.ip, args.port, args.device_id, args.timeout)

    if not modbus.connect():
        print(f"ERROR: cannot connect to {args.ip}:{args.port}", file=sys.stderr)
        return 3

    commands: "queue.Queue[str]" = queue.Queue()
    input_thread = threading.Thread(target=command_worker, args=(commands,), daemon=True)
    input_thread.start()

    fields = [
        "timestamp_utc",
        "elapsed_s",
        "cycle",
        "mode",
        "sv_c",
        "pv_raw",
        "pv_c",
        "error_c",
        "derror_c_per_s",
        "normalized_error",
        "normalized_derror",
        "rule_pwm",
        "final_pwm",
        "mv_write_raw",
        "centroid",
        "read_ms",
        "fuzzy_ms",
        "write_ms",
        "actual_period_ms",
        "overrun_ms",
        "comm_errors",
        "status",
    ]

    start_t = time.perf_counter()
    previous_cycle_t = start_t
    next_deadline = start_t
    period_s = args.period_ms / 1000.0

    print(f"Connected to {args.ip}:{args.port}, device_id={args.device_id}")
    print(f"PV={args.pv_address}, MV={args.mv_address}, period={args.period_ms} ms")
    print(f"Fuzzy library: {library_path}")
    print(f"CSV: {csv_path}")
    print("Initial state: STOP (MV=0)")
    print("Commands: run | stop | sv <C> | status | reset | quit")

    try:
        with csv_path.open("w", newline="", encoding="utf-8") as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=fields)
            writer.writeheader()

            while not state.quit:
                cycle_t = time.perf_counter()
                actual_period_ms = (cycle_t - previous_cycle_t) * 1000.0
                previous_cycle_t = cycle_t
                state.last_period_ms = actual_period_ms
                process_commands(commands, state, fuzzy)

                read_ms = 0.0
                fuzzy_ms = 0.0
                write_ms = 0.0
                pv_raw = ""
                pv_c = math.nan
                mv_raw = 0
                status = "OK"
                diag = {
                    "error_c": 0.0,
                    "derror_c_per_s": 0.0,
                    "normalized_error": 0.0,
                    "normalized_derror": 0.0,
                    "rule_pwm": 0.0,
                    "pwm": 0.0,
                    "centroid": 0.0,
                }

                try:
                    t0 = time.perf_counter()
                    raw = modbus.read_holding(args.pv_address)
                    read_ms = (time.perf_counter() - t0) * 1000.0
                    pv_raw = raw
                    signed_raw = raw if args.pv_unsigned else uint16_to_int16(raw)
                    pv_c = float(signed_raw) * args.pv_scale
                    state.last_pv_c = pv_c

                    if state.run:
                        t0 = time.perf_counter()
                        pwm = fuzzy.run(state.sv_c, pv_c)
                        fuzzy_ms = (time.perf_counter() - t0) * 1000.0
                        diag = fuzzy.diagnostics()
                        if not math.isfinite(pwm):
                            raise RuntimeError("Fuzzy controller returned non-finite PWM")
                        mv_raw = int(round(pwm * args.mv_scale))
                        mv_raw = max(args.mv_min, min(args.mv_max, mv_raw))

                        t0 = time.perf_counter()
                        modbus.write_register(args.mv_address, mv_raw)
                        write_ms = (time.perf_counter() - t0) * 1000.0
                        state.last_mv = mv_raw
                    else:
                        if state.force_zero_write:
                            t0 = time.perf_counter()
                            modbus.write_register(args.mv_address, 0)
                            write_ms = (time.perf_counter() - t0) * 1000.0
                            state.force_zero_write = False
                        state.last_mv = 0
                        mv_raw = 0

                except Exception as exc:  # Safety: any runtime fault stops heater output.
                    status = f"ERROR:{type(exc).__name__}:{exc}"
                    state.comm_errors += 1
                    state.run = False
                    fuzzy.set_enable(False)
                    state.force_zero_write = True
                    state.last_mv = 0
                    mv_raw = 0
                    print(f"\nCONTROL ERROR -> STOP: {exc}", file=sys.stderr)
                    try:
                        modbus.write_register(args.mv_address, 0)
                        state.force_zero_write = False
                    except Exception as stop_exc:
                        print(f"Emergency MV=0 write failed: {stop_exc}", file=sys.stderr)

                end_work_t = time.perf_counter()
                next_deadline += period_s
                remaining_s = next_deadline - end_work_t
                overrun_ms = max(0.0, -remaining_s * 1000.0)

                writer.writerow(
                    {
                        "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                        "elapsed_s": f"{cycle_t - start_t:.6f}",
                        "cycle": state.cycle,
                        "mode": "RUN" if state.run else "STOP",
                        "sv_c": f"{state.sv_c:.6f}",
                        "pv_raw": pv_raw,
                        "pv_c": "" if not math.isfinite(pv_c) else f"{pv_c:.6f}",
                        "error_c": f"{diag['error_c']:.6f}",
                        "derror_c_per_s": f"{diag['derror_c_per_s']:.6f}",
                        "normalized_error": f"{diag['normalized_error']:.6f}",
                        "normalized_derror": f"{diag['normalized_derror']:.6f}",
                        "rule_pwm": f"{diag['rule_pwm']:.6f}",
                        "final_pwm": f"{diag['pwm']:.6f}",
                        "mv_write_raw": mv_raw,
                        "centroid": f"{diag['centroid']:.6f}",
                        "read_ms": f"{read_ms:.6f}",
                        "fuzzy_ms": f"{fuzzy_ms:.6f}",
                        "write_ms": f"{write_ms:.6f}",
                        "actual_period_ms": f"{actual_period_ms:.6f}",
                        "overrun_ms": f"{overrun_ms:.6f}",
                        "comm_errors": state.comm_errors,
                        "status": status,
                    }
                )
                state.cycle += 1
                if state.cycle % 50 == 0:
                    csv_file.flush()

                if remaining_s > 0.0:
                    time.sleep(remaining_s)
                elif -remaining_s > period_s:
                    # Do not accumulate an ever-growing backlog after a long stall.
                    next_deadline = time.perf_counter()

    except KeyboardInterrupt:
        print("\nCtrl+C -> STOP")
    finally:
        fuzzy.set_enable(False)
        try:
            modbus.write_register(args.mv_address, 0)
            print("Final MV=0 written")
        except Exception as exc:
            print(f"WARNING: final MV=0 write failed: {exc}", file=sys.stderr)
        modbus.close()
        print(f"CSV saved: {csv_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
