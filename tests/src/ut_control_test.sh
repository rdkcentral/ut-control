#!/bin/bash

# *
# * If not stated otherwise in this file or this component's LICENSE file the
# * following copyright and licenses apply:
# *
# * Copyright 2023 RDK Management
# *
# * Licensed under the Apache License, Version 2.0 (the "License");
# * you may not use this file except in compliance with the License.
# * You may obtain a copy of the License at
# *
# * http://www.apache.org/licenses/LICENSE-2.0
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *

set -e # error out if required
SCRIPT_EXEC="$(realpath $0)"
MY_DIR="$(dirname $SCRIPT_EXEC)"

cd "$(dirname "$0")"

export LD_LIBRARY_PATH=/usr/lib:/lib:/home/root:${MY_DIR}

HTTP_PORT=8000
HTTP_PID=""

# Check if HTTP server is already running
if ! lsof -iTCP:${HTTP_PORT} -sTCP:LISTEN >/dev/null 2>&1; then
    python3 -m http.server ${HTTP_PORT} &
    HTTP_PID=$!
fi

# Cleanup only if this script started the server
cleanup() {
    if [[ -n "$HTTP_PID" ]]; then
        kill "$HTTP_PID" 2>/dev/null || true
        wait "$HTTP_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Run test
LSAN_OPTIONS="suppressions=../../lsan.supp print_suppressions=1" ./ut_control_test "$@"
UT_STATUS=$?

exit $UT_STATUS
