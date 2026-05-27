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
make TARGET=linux                            # default: UT_LOG_LEVEL_WARNING

# ARM cross-build
make TARGET=arm                              # default: UT_LOG_LEVEL_WARNING
```

The output is `build/<TARGET>/lib/libut_control.so`.

## Running Tests

```sh
cd tests
make TARGET=linux
cd build/bin
./ut_control_test.sh
```

---

## Log Level Filtering

Control which log levels are compiled in by setting `UT_LOG_LEVEL` via a make variable or environment variable:

```sh
make TARGET=linux UT_LOG_LEVEL=4            # compile in DEBUG
make TARGET=linux UT_LOG_LEVEL=0            # compile out everything
export UT_LOG_LEVEL=3 && make TARGET=linux  # INFO via env var
```

The macros delegate to wrapper functions (`UT_logPrefix_error`, `UT_logPrefix_warning`, etc.) which apply the level check internally. Because C evaluates all function arguments before the call, macro arguments are **always evaluated** regardless of the active level; only the output is suppressed.

If `UT_LOG_LEVEL` is not set it defaults to `WARNING` (2).

| Value | Constant               | Active macros                                        |
|-------|------------------------|------------------------------------------------------|
| 0     | `UT_LOG_LEVEL_NONE`    | none                                                 |
| 1     | `UT_LOG_LEVEL_ERROR`   | `UT_LOG_ERROR`                                       |
| 2     | `UT_LOG_LEVEL_WARNING` | `UT_LOG_ERROR`, `UT_LOG_WARNING` **(default)**       |
| 3     | `UT_LOG_LEVEL_INFO`    | `UT_LOG_ERROR`, `UT_LOG_WARNING`, `UT_LOG_INFO`      |
| 4     | `UT_LOG_LEVEL_DEBUG`   | all macros                                           |

Passing an invalid value is caught by the Makefile before the compiler is invoked:

```
Makefile: *** UT_LOG_LEVEL must be a number 0-4: 0=NONE 1=ERROR 2=WARNING 3=INFO 4=DEBUG (got 'DEBUG').  Stop.
Makefile: *** UT_LOG_LEVEL must be a number 0-4: 0=NONE 1=ERROR 2=WARNING 3=INFO 4=DEBUG (got '6').  Stop.
```
