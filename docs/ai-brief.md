# ut-control -- AI-Ingestible Framework Brief

> Single-file reference for AI tools. Covers the primary public C APIs, the
> build system, vendored dependencies, and integration points with ut-core and
> ut-raft. It is not an exhaustive symbol listing -- some public constants and
> macros (e.g. `UT_KVP_STATUS_MAX`, `UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES`,
> `UT_LOG_MAX_PATH`, the `UT_LOG_ASCII_*` colour codes) are referenced only
> where relevant; consult the headers for the complete set.

**Repository:** `rdkcentral/ut-control`
**License:** Apache-2.0
**Language:** C (C99), with Python/shell test clients
**Output:** `libut_control.so` (shared library)

---

## 1. Purpose

ut-control is the support library underneath **ut-core** (the RDK unit-testing
framework). It provides three facilities:

| Module | Header | What it does |
|---|---|---|
| **KVP** | `ut_kvp.h` | YAML/JSON key-value parser backed by libfyaml |
| **Control Plane** | `ut_control_plane.h` | WebSocket/HTTP server for remote test control |
| **Logging** | `ut_log.h` | Timestamped, colour-coded logging to stdout + file |

ut-core links against `libut_control.so` to gain KVP profile loading, control
plane callbacks, and structured logging.

---

## 2. KVP Module (`ut_kvp.h` / `ut_kvp.c`)

### 2.1 Design

- Backed by **libfyaml** (`struct fy_document`).
- Opaque handle `ut_kvp_instance_t *` (internally carries a magic-validated
  struct with the fyaml document pointer).
- Keys use **dot-notation** (e.g. `"section.subsection.field"`); dots are
  converted to `/` internally for fyaml path lookups.
- Supports hex literals (`0x1A`) in integer fields.
- Supports `!include` tags and `include` mapping keys for file/URL inclusion
  (recursive up to depth 5). URL includes use libcurl.
- Multiple files can be loaded into the same instance via `ut_kvp_open()`;
  subsequent `ut_kvp_open()` calls merge into the existing document tree.
  (Note: `ut_kvp_openMemory()` does not accumulate across repeated calls --
  it resets the instance root to the newly parsed payload.)

### 2.2 Instance Lifecycle

```c
ut_kvp_instance_t *inst = ut_kvp_createInstance();  // allocate
ut_kvp_open(inst, "profile.yaml");                  // load from file
// ... or ...
ut_kvp_openMemory(inst, yamlBuf, strlen(yamlBuf)+1);// load from buffer
// use getters ...
ut_kvp_close(inst);          // release parsed data (instance still valid)
ut_kvp_destroyInstance(inst); // free the instance itself
```

### 2.3 Complete API

#### Lifecycle

| Signature | Description |
|---|---|
| `ut_kvp_instance_t *ut_kvp_createInstance(void)` | Allocate a new KVP instance. Returns NULL on failure. |
| `void ut_kvp_destroyInstance(ut_kvp_instance_t *pInstance)` | Close + free an instance. |
| `ut_kvp_status_t ut_kvp_open(ut_kvp_instance_t *pInstance, const char *fileNameOrUrl)` | Parse a YAML/JSON file (or URL) into the instance. Merges with existing data. |
| `ut_kvp_status_t ut_kvp_openMemory(ut_kvp_instance_t *pInstance, char *pData, uint32_t length)` | Parse a caller-owned memory buffer. Caller retains ownership of `pData`. Resets the instance root to this payload (does not merge across repeated calls). |
| `void ut_kvp_close(ut_kvp_instance_t *pInstance)` | Release parsed data but keep the instance handle valid. |

#### Typed Getters

All getters take `(ut_kvp_instance_t *pInstance, const char *pszKey)` and
return the typed value (or 0/false on error). Keys use dot-notation.

| Signature | Returns |
|---|---|
| `bool ut_kvp_getBoolField(inst, key)` | `true` if value is `"true"` (case-insensitive), else `false`. |
| `uint8_t ut_kvp_getUInt8Field(inst, key)` | Unsigned 8-bit integer. Supports hex (`0x1A`). |
| `uint16_t ut_kvp_getUInt16Field(inst, key)` | Unsigned 16-bit integer. |
| `uint32_t ut_kvp_getUInt32Field(inst, key)` | Unsigned 32-bit integer. |
| `uint64_t ut_kvp_getUInt64Field(inst, key)` | Unsigned 64-bit integer. |
| `int8_t ut_kvp_getInt8Field(inst, key)` | Signed 8-bit integer. Supports hex (`0x1A`). |
| `int16_t ut_kvp_getInt16Field(inst, key)` | Signed 16-bit integer. |
| `int32_t ut_kvp_getInt32Field(inst, key)` | Signed 32-bit integer. |
| `int64_t ut_kvp_getInt64Field(inst, key)` | Signed 64-bit integer. |
| `float ut_kvp_getFloatField(inst, key)` | Single-precision float. |
| `double ut_kvp_getDoubleField(inst, key)` | Double-precision float. |

#### String & Data Retrieval

| Signature | Description |
|---|---|
| `ut_kvp_status_t ut_kvp_getStringField(inst, key, char *buf, uint32_t bufSize)` | Copy string value into caller-owned buffer. |
| `char *ut_kvp_getData(ut_kvp_instance_t *pInstance)` | Serialize the entire instance to a malloc'd YAML string. Caller must `free()`. |
| `unsigned char *ut_kvp_getDataBytes(inst, key, int *size)` | Parse a comma-separated byte string (decimal or hex) into a malloc'd byte array. Caller must `free()`. |

#### Query & List

| Signature | Description |
|---|---|
| `bool ut_kvp_fieldPresent(inst, key)` | Returns `true` if the key/node exists in the document. |
| `uint32_t ut_kvp_getListCount(inst, key)` | Returns the number of items in a YAML sequence at `key`. |

### 2.4 Status Codes (`ut_kvp_status_t`)

| Enum | Meaning |
|---|---|
| `UT_KVP_STATUS_SUCCESS` | Operation succeeded |
| `UT_KVP_STATUS_FILE_OPEN_ERROR` | File not accessible |
| `UT_KVP_STATUS_INVALID_PARAM` | Bad parameter |
| `UT_KVP_STATUS_PARSING_ERROR` | YAML/JSON parse failure |
| `UT_KVP_STATUS_KEY_NOT_FOUND` | Key does not exist |
| `UT_KVP_STATUS_NO_DATA` | No document loaded |
| `UT_KVP_STATUS_NULL_PARAM` | NULL pointer passed |
| `UT_KVP_STATUS_INVALID_INSTANCE` | Bad/uninitialized instance handle |
| `UT_KVP_STATUS_MAX` | Out-of-range marker; not a valid return code |

### 2.5 Constants

- `UT_KVP_MAX_ELEMENT_SIZE` = 256 -- max size of a single key or scalar value.

---

## 3. Control Plane Module (`ut_control_plane.h` / `ut_control_plane.c`)

### 3.1 Design

- Built on **libwebsockets** (LWS).
- Two compile-time modes selected by `#define WEBSOCKET_SERVER`:
  - **WebSocket mode** (`WEBSOCKET_SERVER` defined): LWS runs the
    `echo-protocol`; clients connect via `ws://host:port`.
  - **HTTP-POST mode** (default, `WEBSOCKET_SERVER` not defined): LWS serves
    an HTTP endpoint at `POST /api/postKVP`; clients send YAML/JSON via curl.
- Internally uses two pthreads:
  - **WebSocket service thread** -- calls `lws_service()` in a tight loop.
  - **State machine thread** -- dequeues messages, parses them as KVP, and
    dispatches to registered callbacks.
- Message queue: fixed-size array of 32 slots (`MAX_MESSAGES`) with FIFO
  semantics, protected by a mutex + condvar. Dequeue removes the head element
  and shifts the remaining entries down (it is not a ring buffer).
- Callback matching: when a message arrives, it is parsed into a transient
  `ut_kvp_instance_t`. For each registered callback, if
  `ut_kvp_fieldPresent(instance, registeredKey)` is true, that callback fires.

### 3.2 Lifecycle

```c
ut_controlPlane_instance_t *cp = UT_ControlPlane_Init(8080);

// Register callbacks before or after Start
UT_ControlPlane_RegisterCallbackOnMessage(cp, "myKey", myHandler, userData);

UT_ControlPlane_Start(cp);   // spawns service threads
// ... test runs, external client sends YAML/JSON ...
UT_ControlPlane_Stop(cp);    // joins threads
UT_ControlPlane_Exit(cp);    // destroys LWS context + frees memory
```

### 3.3 Complete API

| Signature | Description |
|---|---|
| `ut_controlPlane_instance_t *UT_ControlPlane_Init(uint32_t monitorPort)` | Create an LWS context on `monitorPort`. Returns NULL on failure (e.g. port 0). |
| `ut_control_plane_status_t UT_ControlPlane_RegisterCallbackOnMessage(inst, char *key, ut_control_callback_t cb, void *userData)` | Register `cb` to fire when incoming KVP contains `key`. Max 32 registrations. |
| `void UT_ControlPlane_Start(inst)` | Spawn the state-machine + WebSocket service threads. |
| `void UT_ControlPlane_Stop(inst)` | Enqueue an EXIT message; join both threads. |
| `void UT_ControlPlane_Exit(inst)` | Stop (if running) + destroy LWS context + free instance. |

#### Callback Signature

```c
typedef void (*ut_control_callback_t)(char *key,
                                       ut_kvp_instance_t *instance,
                                       void *userData);
```

- `instance` is **transient** -- valid only for the duration of the callback.
- To retain data, call `ut_kvp_getData(instance)`, copy the returned string,
  then `free()` it before returning. Reconstruct later with
  `ut_kvp_openMemory()`.

#### Key-String Mapping Helpers

These are utility functions for enum-to-string conversion tables, used by HAL
test code to map KVP string values to C enum constants.

| Signature | Description |
|---|---|
| `uint32_t UT_Control_GetMapValue(const ut_control_keyStringMapping_t *map, char *searchString, int onNotFoundValue)` | Look up an integer by string in a mapping table. |
| `const char *UT_Control_GetMapString(const ut_control_keyStringMapping_t *map, int32_t key)` | Look up a string by integer key. Returns NULL if not found. |

The mapping table type:
```c
typedef struct {
    const char *string;
    int32_t value;
} ut_control_keyStringMapping_t;
```
Tables are NULL-terminated (last entry has `.string == NULL`).

### 3.4 Status Codes (`ut_control_plane_status_t`)

| Enum | Meaning |
|---|---|
| `UT_CONTROL_PLANE_STATUS_OK` | Success |
| `UT_CONTROL_PLANE_STATUS_LIST_FULL` | 32 callbacks already registered |
| `UT_CONTROL_PLANE_STATUS_INVALID_HANDLE` | NULL or corrupt instance |
| `UT_CONTROL_PLANE_STATUS_INVALID_PARAM` | NULL key, callback, or userData |

### 3.5 Message Format

Messages are YAML or JSON text. Example YAML payload:

```yaml
test1:
  yamlData: somevalue
  x: 1
  on: true
```

If a callback is registered for key `"test1.yamlData"`, it fires because
`ut_kvp_fieldPresent(instance, "test1.yamlData")` returns true.

In HTTP-POST mode, messages are sent with:
```bash
curl -X POST -H "Content-Type: application/x-yaml" \
     --data-binary "@payload.yaml" http://host:8080/api/postKVP
```

The HTTP-POST endpoint has a hard payload limit of 4096 bytes
(`MAX_POST_DATA_SIZE`); posts whose accumulated body would reach or exceed
that size are rejected. Clients must keep individual payloads under this
limit.

In WebSocket mode, Python clients send via the `websockets` library:
```python
async with websockets.connect("ws://host:8080") as ws:
    await ws.send(yaml.dump(data))
```

---

## 4. Logging Module (`ut_log.h` / `ut_log.c`)

### 4.1 Design

- Dual output: writes to **stdout** (with ANSI colour) and to a **log file**.
  The `UT_LOG_*` macros route through `UT_logPrefix()`, which strips ANSI
  colour codes before writing to the file. `UT_log()` writes its buffer to
  the file without stripping; the `UT_LOG_*` macros are the intended public
  interface.
- Default log path: `/tmp/ut-log_YYYY-MM-DD_HHMMSS.log`.
- Log file is opened/closed on every write (acknowledged as a known FIXME for
  future optimization).
- Max line size: 255 characters (`UT_LOG_MAX_LINE_SIZE`).

### 4.2 Macros (Primary Interface)

All macros automatically inject `__FILE__` and `__LINE__`.

| Macro | Prefix/Colour | Use case |
|---|---|---|
| `UT_LOG(fmt, ...)` | `LOG` (magenta) | General logging |
| `UT_LOG_STEP(fmt, ...)` | `STEP` (blue) | Test step markers |
| `UT_LOG_INFO(fmt, ...)` | `INFO` (cyan) | Informational |
| `UT_LOG_DEBUG(fmt, ...)` | `DEBUG` (magenta) | Debug detail |
| `UT_LOG_WARNING(fmt, ...)` | `WARN` (yellow) | Warnings |
| `UT_LOG_ERROR(fmt, ...)` | `ERROR` (red) | Errors |
| `UT_LOG_ASSERT(prefix, fmt, ...)` | `ASSERT` (red) | Assertion failures |
| `UT_LOG_PREFIX(prefix, fmt, ...)` | Custom | User-defined prefix |

### 4.3 Functions

| Signature | Description |
|---|---|
| `void UT_log_setLogFilePath(char *inputFilePath)` | Set log directory. Generates filename `ut-log_YYYY-MM-DD_HHMMSS.log`. |
| `const char *UT_log_getLogFilename(void)` | Return the active log file path. |
| `void UT_log(const char *function, int line, const char *format, ...)` | Core log function (used internally). |
| `void UT_logPrefix(const char *file, int line, const char *prefix, const char *format, ...)` | Core log function with custom prefix (called by all macros). |

### 4.4 Output Format

```
<newline><timestamp>, <prefix (padded 16)>, <basename(file)>, <line> : <message>
```

Example:
```
2024-06-15-14:30:22,            STEP  , ut_test_kvp.c,    42 : Loading profile
```

---

## 5. Build System

### 5.1 `configure.sh`

Usage: `./configure.sh <linux|arm>`

Downloads and builds all vendored dependencies into
`framework/<target>/` and `build/<target>/`:

| Dependency | Version / Commit | Purpose |
|---|---|---|
| **libfyaml** | `v0.9.6` (Mar 2026) | YAML parser (compiled into libut_control) |
| **asprintf** | 0.0.3 | Portable `asprintf()` (compiled into libut_control) |
| **libwebsockets** | 4.3.3 | WebSocket/HTTP server (static `.a` linked in) |
| **curl** | 8.8.0 | HTTP client for `!include` URL resolution (static or system) |
| **OpenSSL** | 1.1.1w | Crypto dependency (static or system). Note: the default build disables TLS -- libwebsockets is built with `-DLWS_WITH_SSL=OFF` and curl with `-DCMAKE_USE_OPENSSL=OFF`, so `wss://`/HTTPS are not enabled. |
| **CMake** | 3.30.0 | Build tool for libwebsockets (downloaded only if system cmake < 3.13) |

The script prefers system-installed OpenSSL, curl, and cmake when available.
For `arm` targets, it expects `CC` to be set with a `--sysroot=` flag.

### 5.2 `Makefile`

- Requires `TARGET=linux` or `TARGET=arm`.
- `make framework` (default) runs `configure.sh` then builds the library.
- `make lib` compiles all sources into `build/<target>/lib/libut_control.so`.
- Sources: `src/ut_kvp.c`, `src/ut_control_plane.c`, `src/ut_log.c` plus all
  vendored libfyaml/asprintf sources.
- Key flags: `-fPIC -Wall -shared -DNDEBUG`
- HTTP-POST mode is the default. To enable WebSocket mode, uncomment
  `-DWEBSOCKET_SERVER` in the Makefile.

---

## 6. Integration with ut-core

ut-core (`rdkcentral/ut-core`) consumes ut-control as a git submodule or
vendored dependency. It links against `libut_control.so` and re-exports the
headers. Test binaries built with ut-core get:

- **KVP profiles** for device-specific test configuration (platform profiles
  loaded via `ut_kvp_open()`).
- **Control plane** for remote test orchestration (the test binary starts an
  LWS server; an external harness sends commands).
- **Logging** via the `UT_LOG_*` macros.

---

## 7. Integration with ut-raft (Python side)

ut-raft is the Python-based test orchestration framework. It communicates with
a running test binary's control plane by:

1. Connecting to the LWS server (WebSocket or HTTP-POST depending on build
   mode).
2. Sending YAML/JSON payloads that contain keys matching registered callbacks.
3. The C-side callback fires, reads the KVP data, and acts on it (e.g.
   triggering a specific HAL call or changing test parameters at runtime).

Example clients are in `tests/websocket-clients/`:
- `python-client-send-yaml.py` -- sends YAML over WebSocket
- `python-client-send-json.py` -- sends JSON over WebSocket
- `simple_websocket.py` -- minimal WebSocket client
- `curl-client-yaml.sh` -- sends YAML via HTTP POST
- `curl-client-json.sh` -- sends JSON via HTTP POST
- `curl-client-binary.sh` -- sends binary data via HTTP POST

(`example.yaml` / `example.json` in the same directory are sample payloads.)

---

## 8. Directory Layout

```
ut-control/
  include/
    ut_kvp.h                 -- KVP parser API
    ut_control_plane.h       -- Control plane API
    ut_log.h                 -- Logging API
  src/
    ut_kvp.c                 -- KVP implementation (libfyaml + libcurl)
    ut_control_plane.c       -- Control plane (libwebsockets + pthreads)
    ut_log.c                 -- Logging implementation
    asprintf/patches/        -- Patch for vendored asprintf
    libyaml/patches/         -- Patch for vendored libfyaml
  tests/
    src/                     -- test suites built on the ut-core test framework (`<ut.h>`: `UT_init`, `UT_run_tests`, `UT_add_suite`)
    websocket-clients/       -- Python/curl client scripts for control plane testing
  configure.sh               -- Downloads + builds vendored dependencies
  Makefile                   -- Builds libut_control.so
  docs/
    ai-brief.md              -- This file
```
