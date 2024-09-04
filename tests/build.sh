#!/usr/bin/env bash

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

RED="\e[0;31m"
YELLOW="\e[1;33m"
GREEN="\e[0;32m"
NC="\e[39m"

# The purpose of this script is on a clean environement, is to give you the latest binary compatible version for testing
# When the major version changes in the ut-core, what that signals is that the testings will have to be upgraded to support that version
# Therefore in that case it warns you but doesnt' chnage to that version, which could cause your tests to break.
# Change this to upgrade your UT-Core Major versions. Non ABI Changes 1.x.x are supported, between major revisions
UT_PROJECT_MAJOR_VERSION="3."

# Clone the Unit Test Requirements
TEST_REPO=git@github.com:rdkcentral/ut-core.git

echo "TARGET= [$TARGET] from [$0]"

# This function checks the latest version of UT core and recommends an upgrade if reuqired
function check_next_revision()
{
    pushd ./ut-core
    # Set default UT_CORE_PROJECT_VERSION to next revision, if it's set then we don't need to tell you again
    if [ -v ${UT_CORE_PROJECT_VERSION} ]; then
        UT_CORE_PROJECT_VERSION=$(git tag | grep ${UT_PROJECT_MAJOR_VERSION} | sort -r | head -n1)
        UT_NEXT_VERSION=$(git tag | sort -r | head -n1)
        echo -e ${YELLOW}ut-core version selected:[${UT_CORE_PROJECT_VERSION}]${NC}
        if [ "${UT_NEXT_VERSION}" != "${UT_CORE_PROJECT_VERSION}" ]; then
            echo -e ${RED}--- New Version of ut-core released [${UT_NEXT_VERSION}] consider upgrading ---${NC}
        fi
    fi
    popd > /dev/null
}

# Check if the common document configuration is present, if not clone it
if [ -d "./ut-core" ]; then
    # ut-core exists so run the makefile from ut
    check_next_revision
    #make test
else
    echo "Cloning Unit Test Core System"
    git clone ${TEST_REPO} ut-core
    check_next_revision
    cd ./ut-core
    #git checkout ${UT_CORE_PROJECT_VERSION} //Commented this as tests can be on latest ut-core
    git checkout feature/gh11-cpp-suppport-ut-core
    ./build.sh no_ut_control
    #cd ..
    #./${0} $@
fi
