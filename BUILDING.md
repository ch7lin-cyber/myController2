# Cross-Platform Build and Regression

This repository builds the fuzzy controller sources as a C library and verifies
that the public C headers can be consumed directly from both C and C++.

## Requirements

- CMake 3.20 or newer
- A C11 compiler
- A C++17 compiler
- Optional: Ninja (required only when using the supplied CMakePresets.json)

Typical supported environments:

- Windows: MinGW-w64 GCC/G++ or Visual Studio MSVC
- Linux: GCC/G++ or Clang/Clang++
- macOS: Apple Clang

## Important include-path rule

`ssm_std_define.h` intentionally lives only at the repository root.
The CMake target exports both include paths:

- repository root
- `FuzzyController_src`

Consumers that do not use the supplied CMake target must provide both paths.

## Static library regression

Using presets and Ninja:

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Equivalent generator-independent commands:

```sh
cmake -S . -B build -DMYCONTROLLER_BUILD_SHARED=OFF -DMYCONTROLLER_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Shared library regression

```sh
cmake --preset shared
cmake --build --preset shared
ctest --preset shared
```

or:

```sh
cmake -S . -B build-shared -DMYCONTROLLER_BUILD_SHARED=ON -DMYCONTROLLER_BUILD_TESTS=ON
cmake --build build-shared --config Debug
ctest --test-dir build-shared -C Debug --output-on-failure
```

The shared build exercises `MY_API` as follows:

- Windows producer: `SSM_FB_BUILD_DLL` -> `__declspec(dllexport)`
- Windows consumer: `SSM_FB_USE_DLL` -> `__declspec(dllimport)`
- GCC/Clang shared build: `SSM_FB_BUILD_SHARED` -> default symbol visibility
- Static/MCU builds: `MY_API` expands to nothing

## Regression targets

CTest currently runs:

1. `fuzzy_c_api`
   - compiles all public fuzzy headers from C
   - initializes Controller and ConfigManager
   - executes one controller cycle
2. `fuzzy_cpp_api`
   - includes all public C headers directly from C++
   - does NOT add an external `extern "C"` wrapper
   - therefore verifies that each header supplies its own C++ compatibility guard
   - links the C++ executable against the C-compiled fuzzy library
3. `fuzzy_config_manager`
   - validates default runtime configuration
   - checks rule and membership rollback behavior
   - applies scaling and feed-forward mappings
4. `identified_plant_smoke`
   - links the fuzzy controller and identified C thermal plant
   - executes the existing Part 10 closed-loop smoke program

## Warnings

The default build enables strong warnings. To temporarily promote warnings to
errors:

```sh
cmake -S . -B build-werror -DMYCONTROLLER_WARNINGS_AS_ERRORS=ON
cmake --build build-werror
```

For production release, it is recommended to make warnings-as-errors part of
CI only after all supported compilers have a clean baseline.

## GitHub Actions

`.github/workflows/cross-platform-regression.yml` runs a matrix of:

- Ubuntu / Windows / macOS
- Static / Shared library

for six total build/test combinations.
