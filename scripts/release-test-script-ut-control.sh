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

SCRIPT_EXEC="$(realpath $0)"
MY_DIR="$(dirname $SCRIPT_EXEC)"

# ANSI color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print usage
usage() {
    echo -e "${YELLOW}Usage: $0 -u <REPO_URL> -t <ut_control_branch_name>>${NC}"
    exit 1
}

# Parse command-line arguments
while getopts "u:t:" opt; do
    case $opt in
        u) REPO_URL="$OPTARG" ;;
        t) ut_control_branch_name="$OPTARG" ;;
        *) usage ;;
    esac
done

# Function to print error message and exit
error_exit() {
    echo -e "${RED}$1${NC}" 1>&2
    exit 1
}

# Check if the branch name is passed else display the usage
if [ -z "$ut_control_branch_name" ]; then
    echo "ut_control_branch_name is empty"
    usage
    exit 1
fi

# If repo_url is not passed by user set it to default
if [ -z "$REPO_URL" ]; then
    REPO_URL=git@github.com:rdkcentral/ut-control.git
fi
REPO_NAME=$(basename "$REPO_URL" .git)

# # Set compiler type based on the environment passed
# case "$environment" in
#     "ubuntu" | "VM-SYNC" | "dunfell_linux") architecture_type="linux" ;;
#     "dunfell_arm") architecture_type="arm" ;;
#     *)
#         echo "Unknown environment: $environment"
#         echo "Environments are : ubuntu, VM-SYNC, dunfell_arm, dunfell_linux"
#         exit 1
#         ;;
# esac

#git clone and change dir
run_git_clone(){
    local environment="$1"
    echo "environment=$environment"

    # Git clone the branch of ut-control to carry out testing
    REPO_NAME=$(basename "$REPO_URL" .git)
    if [ ! -z "$ut_control_branch_name" ]; then
        # Clone the repository
        echo -e "${YELLOW}Cloning repository from $REPO_URL and branch $ut_control_branch_name${NC}"
        GIT_URL="git clone ${REPO_URL} -b ${ut_control_branch_name} ut-control"
        git clone "$REPO_URL" -b "$ut_control_branch_name" "$REPO_NAME-$environment" || error_exit "Error: Failed to clone repository."
        echo -e "${GREEN}GIT_URL = $GIT_URL${NC}"
    fi
    
    # Change to the repository directory
    echo -e "Current path is: $PWD"
    cd "$REPO_NAME-$environment" || error_exit "Error: Failed to change directory to $REPO_NAME-$environment."
    UT_CNTRL_DIR=${MY_DIR}/$REPO_NAME-$environment
}

# Define a function to run the commands
run_make_with_logs() {
    local architecture_type="$1"
    echo "architecture_type=$architecture_type"

    # Execute make and redirect the log to a file
    echo -e "${YELLOW}Running make ...logs redirected to $PWD/make_log.txt${NC}"
    make TARGET="$architecture_type" > make_log.txt 2>&1
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Make command executed successfully.${NC}"
    else
        echo -e "Make command failed. Check the logs in make_log.txt"
    fi

    # Execute make -C tests/ and redirect the log to a file
    echo -e "${YELLOW}Running make -C tests/...logs redirected to $PWD/make_test_log.txt${NC}"
    make TARGET="$architecture_type" -C tests/ > make_test_log.txt 2>&1
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Make -C tests/ command executed successfully.${NC}"
    else
        echo -e "Make -C tests/ command failed. Check the logs in make_test_log.txt"
    fi

}

# Description: This function validates the CURL static library for dunfell_linux.
validate_curl_dunfell_linux() {
    # We do not expect CURL static library to be rebuilt in dunfell_linux
    local curl_static_lib="$1"
    local environment="$2"

    if [[ "$environment" == "dunfell_linux" ]]; then
        if [ -f "$curl_static_lib" ]; then
            echo -e "${RED}$curl_static_lib exists. FAIL${NC}"
        else
            echo -e "${GREEN}CURL static lib does not exist. PASS${NC}"
        fi
    fi
}

# Description: This function validates the CURL static library for Ubuntu when libcurl.a is not in /usr/.
validate_curl_ubuntu_no_system_lib() {
    # We expect CURL static library to be rebuilt in ubuntu, as libcurl.a is not in /usr/
    local curl_static_lib="$1"
    local environment="$2"
    local system_curl_lib="$3"

    if [[ -z "$system_curl_lib" && "$environment" == "ubuntu" ]]; then
        if [ -f "$curl_static_lib" ]; then
            echo -e "${GREEN}$curl_static_lib exists. PASS${NC}"
        else
            echo -e "${RED}CURL static lib does not exist. FAIL${NC}"
        fi
    fi
}

# Description: This function validates the CURL static library for Ubuntu when libcurl.a is in /usr/.
validate_curl_ubuntu_with_system_lib() {
    # We do not expect CURL static library to be rebuilt in ubuntu, as libcurl.a is in /usr/
    local curl_static_lib="$1"
    local environment="$2"
    local system_curl_lib="$3"

    if [[ -n "$system_curl_lib" && "$environment" == "ubuntu" ]]; then
        if [ -f "$curl_static_lib" ]; then
            echo -e "${RED}$curl_static_lib exists. FAIL${NC}"
        else
            echo -e "${GREEN}CURL static lib does not exist. PASS${NC}"
        fi
    fi
}


# Description: This function validates the CURL static library for other platforms.
validate_curl_all_other_platforms() {
    # We expect CURL static library to be rebuilt in all other platforms
    local curl_static_lib="$1"
    local environment="$2"

    if [[ "$environment" != "dunfell_linux" && "$environment" != "ubuntu" ]]; then
        if [ -f "$curl_static_lib" ]; then
            echo -e "${GREEN}$curl_static_lib exists. PASS${NC}"
        else
            echo -e "${RED}CURL static lib does not exist. FAIL${NC}"
        fi
    fi
}


# Description: This function validates the CURL static library based on the environment.
validate_curl_library_created_correctly() {
    local environment="$1"
    local architecture_type="$2"
    local curl_static_lib="build/${architecture_type}/curl/lib/libcurl.a"
    local system_curl_lib

    system_curl_lib=$(find /usr/ -iname "libcurl.a" 2>/dev/null)

    # Validate and print results for CURL static library for dunfell_linux
    validate_curl_dunfell_linux "$curl_static_lib" "$environment"

    # Validate and print results for CURL static library for ubuntu when libcurl.a is not in /usr/
    validate_curl_ubuntu_no_system_lib "$curl_static_lib" "$environment" "$system_curl_lib"

    # Validate and print results for CURL static library for ubuntu when libcurl.a is in /usr/
    validate_curl_ubuntu_with_system_lib "$curl_static_lib" "$environment" "$system_curl_lib"

    # Validate and print results for CURL static library for all other platforms
    validate_curl_all_other_platforms "$curl_static_lib" "$environment"
}

run_checks() {
    # Parameters to be passed to the function
    environment=$1
    architecture_type=$2
    ut_control_branch_name=$3

    echo -e "${RED}==========================================================${NC}"
    # Variables to test existence of different librararies
    CURL_STATIC_LIB="build/${architecture_type}/curl/lib/libcurl.a"
    OPENSSL_STATIC_LIB="build/${architecture_type}/openssl/lib/libssl.a"
    CMAKE_HOST_BIN="host-tools/CMake-3.30.0/build/bin/cmake"

    echo -e "${RED}RESULTS for ${environment} ${NC}"

    current_branch=$(git branch | grep '\*' | sed 's/* //')

    # Compare with the target branch name
    if [ "${current_branch}" == "${ut_control_branch_name}" ]; then
        echo -e "${GREEN}On the branch $ut_control_branch_name. PASS${NC}"
    else
        error_exit "Branch is not switched. FAIL"
    fi

    # Test for CURL static library
    validate_curl_library_created_correctly "$environment" "$architecture_type"

    # Test for OpenSSL static library
    if [[ "$environment" == "ubuntu" ]]; then
        if [ ! -f "$OPENSSL_STATIC_LIB" ]; then
            echo -e "${GREEN}Openssl static lib does not exist. PASS ${NC}"
        else
            echo -e "${RED}Openssl static lib exists. FAIL ${NC}"
        fi
    elif [[ "$environment" == "VM-SYNC" ]]; then
        if [ -f "$OPENSSL_STATIC_LIB" ]; then
            echo -e "${GREEN}$OPENSSL_STATIC_LIB exists. PASS ${NC}"
        else
            echo -e "${RED}Openssl static lib does not exist. FAIL ${NC}"
        fi
    elif [[ "$environment" == "dunfell_arm" ]]; then
        if [ -f "$OPENSSL_STATIC_LIB" ]; then
            echo -e "${GREEN}$OPENSSL_STATIC_LIB exists. PASS ${NC}"
        else
            echo -e "${RED}Openssl static lib does not exist. FAIL ${NC}"
        fi
    elif [[ "$environment" == "dunfell_linux" ]]; then
        if [ -f "$OPENSSL_STATIC_LIB" ]; then
            echo -e "${RED}$OPENSSL_STATIC_LIB exists. FAIL ${NC}"
        else
            echo -e "${GREEN}Openssl static lib does not exist. PASS ${NC}"
        fi
    elif [[ "$environment" == "kirkstone_arm" ]]; then
        if [ -f "$OPENSSL_STATIC_LIB" ]; then
            echo -e "${GREEN}$OPENSSL_STATIC_LIB exists. PASS ${NC}"
        else
            echo -e "${RED}Openssl static lib does not exist. FAIL ${NC}"
        fi
    elif [[ "$environment" == "kirkstone_linux" ]]; then
        if [ -f "$OPENSSL_STATIC_LIB" ]; then
            echo -e "${RED}$OPENSSL_STATIC_LIB exists. FAIL ${NC}"
        else
            echo -e "${GREEN}Openssl static lib does not exist. PASS ${NC}"
        fi
    fi

    # Test for CMAKE host binary
    if [[ "$environment" == "ubuntu" ]]; then
        if [ ! -f "$CMAKE_HOST_BIN" ]; then
            echo -e "${GREEN}CMake host binary does not exist. PASS ${NC}"
        else
            echo -e "${RED}CMake host binary exists. FAIL ${NC}"
        fi
    elif [[ "$environment" == "VM-SYNC" ]]; then
        if [ -f "$CMAKE_HOST_BIN" ]; then
            echo -e "${GREEN}$CMAKE_HOST_BIN exists. PASS ${NC}"
        else
            echo -e "${RED}CMake host binary does not exist. FAIL ${NC}"
        fi
    elif [[ "$environment" == "dunfell_arm" ]]; then
        if [ ! -f "$CMAKE_HOST_BIN" ]; then
            echo -e "${GREEN}CMake host binary does not exist. PASS ${NC}"
        else
            echo -e "${RED}CMake host binary exists. FAIL ${NC}"
        fi
    elif [[ "$environment" == "dunfell_linux" ]]; then
        if [ ! -f "$CMAKE_HOST_BIN" ]; then
            echo -e "${GREEN}CMake host binary does not exist. PASS ${NC}"
        else
            echo -e "${RED}CMake host binary exists. FAIL ${NC}"
        fi
    elif [[ "$environment" == "kirkstone_arm" ]]; then
        if [ ! -f "$CMAKE_HOST_BIN" ]; then
            echo -e "${GREEN}CMake host binary does not exist. PASS ${NC}"
        else
            echo -e "${RED}CMake host binary exists. FAIL ${NC}"
        fi
    elif [[ "$environment" == "kirkstone_linux" ]]; then
        if [ ! -f "$CMAKE_HOST_BIN" ]; then
            echo -e "${GREEN}CMake host binary does not exist. PASS ${NC}"
        else
            echo -e "${RED}CMake host binary exists. FAIL ${NC}"
        fi
    fi

    echo -e "${RED}==========================================================${NC}"
}

print_results() {
    pushd ${MY_DIR} > /dev/null

    #Results for ubuntu
    PLAT_DIR="${REPO_NAME}-ubuntu"
    pushd ${PLAT_DIR} > /dev/null
    run_checks "ubuntu" "linux" "$ut_control_branch_name"
    popd > /dev/null

    #Results for VM_SYNC
    PLAT_DIR="${REPO_NAME}-VM-SYNC_linux"
    pushd ${PLAT_DIR} > /dev/null
    run_checks "VM-SYNC" "linux" $ut_control_branch_name
    popd > /dev/null

    #Results for dunfell-arm
    PLAT_DIR="${REPO_NAME}-dunfell_arm"
    pushd ${PLAT_DIR} > /dev/null
    run_checks "dunfell_arm" "arm" $ut_control_branch_name
    popd > /dev/null

    #Results for dunfell-linux
    PLAT_DIR="${REPO_NAME}-dunfell_linux"
    pushd ${PLAT_DIR} > /dev/null
    run_checks "dunfell_linux" "linux" $ut_control_branch_name
    popd > /dev/null

    #Results for kirkstone-arm
    PLAT_DIR="${REPO_NAME}-kirkstone_arm"
    pushd ${PLAT_DIR} > /dev/null
    run_checks "kirkstone_arm" "arm" $ut_control_branch_name
    popd > /dev/null

    #Results for kirkstone-linux
    PLAT_DIR="${REPO_NAME}-kirkstone_linux"
    pushd ${PLAT_DIR} > /dev/null
    run_checks "kirkstone_linux" "linux" $ut_control_branch_name
    popd > /dev/null

    popd > /dev/null

}

# Environment-specific setups and execution
export -f run_make_with_logs
export -f run_checks
export -f usage
export -f error_exit

run_on_ubuntu_linux() {
    pushd ${MY_DIR} > /dev/null
    run_git_clone "ubuntu"
    run_make_with_logs "linux"
    run_checks "ubuntu" "linux" "$ut_control_branch_name"
    popd > /dev/null
}

# Function to run the commands in a Docker container
run_on_platform() {
    local PLATFORM=$1
    local TARGET=$2
    local VARIANT_FLAG=$3
    local LOG_SUFFIX=$3

    pushd "${MY_DIR}" > /dev/null
    run_git_clone "${PLATFORM}_${TARGET}" "${VARIANT_FLAG}"

    echo "Running make for ${PLATFORM}_${TARGET}"

    local DOCKER_IMAGE="rdk-${PLATFORM}"
    [[ "$PLATFORM" == "VM-SYNC" ]] && DOCKER_IMAGE="vm-sync"

    local ENV_SETUP=""
    if [[ "$TARGET" == "arm" ]]; then
        if [[ "$PLATFORM" == "dunfell" ]]; then
            ENV_SETUP='[ -z "$OECORE_TARGET_OS" ] && source /opt/toolchains/rdk-glibc-x86_64-arm-toolchain/environment-setup-armv7at2hf-neon-oe-linux-gnueabi;'
        else
            ENV_SETUP='[ -z "$OECORE_TARGET_OS" ] && source /opt/toolchains/rdk-glibc-x86_64-arm-toolchain/environment-setup-armv7vet2hf-neon-oe-linux-gnueabi;'
        fi
    fi

    /bin/bash -c "$(cat <<EOF
sc docker run ${DOCKER_IMAGE} '
${ENV_SETUP}
echo \$CC
make TARGET=${TARGET} > make_log_${LOG_SUFFIX}.txt 2>&1
make -C tests/ TARGET=${TARGET} > make_test_log_${LOG_SUFFIX}.txt 2>&1
'
EOF
)"

    run_checks "${PLATFORM}_${TARGET}" "${TARGET}" "$ut_control_branch_name"
    popd > /dev/null
}

# Run tests in different environments
run_on_ubuntu_linux
run_on_platform "dunfell" "linux"
run_on_platform "kirkstone" "linux"
run_on_platform "VM-SYNC" "linux"
run_on_platform "dunfell" "arm"
run_on_platform "kirkstone" "arm"
print_results
