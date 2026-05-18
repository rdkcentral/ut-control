# ut-control

A C library that provides a control plane, key-value pair (KVP) configuration, and structured logging utilities for unit-test frameworks targeting Linux and ARM embedded platforms.

## Prerequisites

- GCC (host) or a cross-compiler toolchain for ARM targets
- CMake ≥ 3.13 (if missing or older, `configure.sh` fetches and builds CMake 3.30.0 for Linux builds)
- `libcurl` (system package, or built by `configure.sh`)
- `libwebsockets` 4.3.3 and `libfyaml` (fetched and built by `configure.sh`)

## Building

`make` will call `configure.sh` first to download and build third-party dependencies:

```sh
# Linux host build
make TARGET=linux                            # defaults to UT_LOG_LEVEL_WARNING

# ARM cross-build
make TARGET=arm                              # defaults to UT_LOG_LEVEL_WARNING
```

The output is `build/<TARGET>/lib/libut_control.so`.

## Running Tests

```sh
cd tests
make TARGET=linux
cd build/bin
./ut_control_test.sh
```

> **Note:** Each binary is compiled at a fixed log level because the build system applies a single `UT_LOG_LEVEL` value to all compilation units. Tests that assert on `UT_LOG_LEVEL` or macro suppression (e.g. `test_ut_log_default_level`) assume the binary was built **without** a `UT_LOG_LEVEL` override. Separate builds are required to cover other log levels.

---

## Compile-Time Log Level Filtering

Control which log levels are compiled in by setting `UT_LOG_LEVEL` via a `make` variable or an environment variable :

```sh
make TARGET=linux UT_LOG_LEVEL=4            # enable DEBUG
```

or

```sh
export UT_LOG_LEVEL=3 && make TARGET=linux  # enable INFO via env var
```

As a **fallback only** (e.g. when building outside of this Makefile), `UT_LOG_LEVEL` can be defined before including `ut_log.h`:

```c
// Fallback: only use this if the build system cannot pass -DUT_LOG_LEVEL
#define UT_LOG_LEVEL UT_LOG_LEVEL_ERROR
#include "ut_log.h"
```

### Default Configuration

If `UT_LOG_LEVEL` is not defined, it defaults to `UT_LOG_LEVEL_WARNING` (2).
This means `UT_LOG_ERROR` and `UT_LOG_WARNING` are active, while `UT_LOG_INFO` and `UT_LOG_DEBUG` calls are compiled out as no-ops.

### Valid Values

| Value | Constant              | Active macros                                      |
|-------|-----------------------|----------------------------------------------------|
| 0     | `UT_LOG_LEVEL_NONE`   | none                                               |
| 1     | `UT_LOG_LEVEL_ERROR`  | `UT_LOG_ERROR`                                     |
| 2     | `UT_LOG_LEVEL_WARNING`| `UT_LOG_ERROR`, `UT_LOG_WARNING` **(default)**     |
| 3     | `UT_LOG_LEVEL_INFO`   | `UT_LOG_ERROR`, `UT_LOG_WARNING`, `UT_LOG_INFO`    |
| 4     | `UT_LOG_LEVEL_DEBUG`  | all macros                                         |

Passing an invalid value — either a non-numeric token or an out-of-range number — is caught by the Makefile before the compiler is invoked:

```
Makefile: *** UT_LOG_LEVEL must be a number 0-4: 0=NONE 1=ERROR 2=WARNING 3=INFO 4=DEBUG (got 'DEBUG').  Stop.
Makefile: *** UT_LOG_LEVEL must be a number 0-4: 0=NONE 1=ERROR 2=WARNING 3=INFO 4=DEBUG (got '6').  Stop.
```

> **Note:** The preprocessor `#error` guard in `ut_log.h` provides a secondary safety net only when `UT_LOG_LEVEL` is set outside of the Makefile (e.g. via a third-party build system). It cannot catch non-numeric tokens because the preprocessor treats unknown identifiers as `0`.
