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

# Start HTTP server in background
python3 -m http.server 8000 &
HTTP_PID=$!

# Register cleanup trap to stop HTTP server on exit
trap '
    kill $HTTP_PID 2>/dev/null || true
    wait $HTTP_PID 2>/dev/null || true
' EXIT

# Run test
./ut_control_test "$@"
UT_STATUS=$?

# Exit with same status as UT test
exit $UT_STATUS