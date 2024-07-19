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

set -e # error out if required

SCRIPT_EXEC="$(realpath $0)"
MY_DIR="$(dirname $SCRIPT_EXEC)"

pushd ${MY_DIR} > /dev/null

FRAMEWORK_DIR=${MY_DIR}/framework
LIBYAML_DIR=${FRAMEWORK_DIR}/libfyaml-master
ASPRINTF_DIR=${FRAMEWORK_DIR}/asprintf
LIBWEBSOCKETS_DIR=${FRAMEWORK_DIR}/libwebsockets-4.3.3
CURL_DIR=${FRAMEWORK_DIR}/curl/curl-8.8.0
OPENSSL_DIR=${FRAMEWORK_DIR}/openssl/openssl-OpenSSL_1_1_1w
CMAKE_DIR=${MY_DIR}/host-tools/CMake-3.30.0
CMAKE_BIN_DIR=${CMAKE_DIR}/build/bin
HOST_CC=gcc
TARGET_CC=${CC}

if [ -d "${LIBYAML_DIR}" ]; then
    echo "Framework [libfyaml] already exists"
else
    echo "Clone libfyaml in ${LIBYAML_DIR}"
    wget https://github.com/pantoniou/libfyaml/archive/refs/heads/master.zip --no-check-certificate -P framework/
    cd framework/
    unzip master.zip
    echo "Patching Framework [${PWD}]"
    cp ../src/libyaml/patches/CorrectWarningsAndBuildIssuesInLibYaml.patch  .
    patch -i CorrectWarningsAndBuildIssuesInLibYaml.patch -p0
    echo "Patching Complete"
    #    ./bootstrap.sh
    #    ./configure --prefix=${LIBYAML_DIR}
    #    make
fi
popd > /dev/null

pushd ${FRAMEWORK_DIR} > /dev/null
if [ -d "${ASPRINTF_DIR}" ]; then
    echo "Framework [asprintf] already exists"
else
    echo "Clone asprintf in ${ASPRINTF_DIR}"
    wget https://github.com/jwerle/asprintf.c/archive/refs/heads/master.zip -P asprintf/. --no-check-certificate
    cd asprintf
    unzip master.zip
    rm asprintf.c-master/test.c
fi
popd > /dev/null

pushd ${MY_DIR}
if command -v cmake &> /dev/null; then
    echo "CMake is installed"
    CMAKE_BIN=$(which cmake)
else
    CMAKE_BIN=${CMAKE_BIN_DIR}/cmake
    if [ -d "${CMAKE_BIN_DIR}" ]; then
        echo "CMake is already built"
    else
        echo "CMake is not installed, building it"
        wget https://github.com/Kitware/CMake/archive/refs/tags/v3.30.0.zip -P host-tools/. --no-check-certificate
        cd host-tools
        unzip v3.30.0.zip
        echo "HOST_CC:${HOST_CC}"
        cd ${CMAKE_DIR}
        mkdir build && cd build
        ../bootstrap --prefix=./. -- -DCMAKE_USE_OPENSSL=OFF
        make CC=${HOST_CC} -j4 BUILD_TESTING=OFF BUILD_EXAMPLES=OFF && make install
    fi
fi
popd > /dev/null

pushd ${FRAMEWORK_DIR} > /dev/null
if [ -d "${LIBWEBSOCKETS_DIR}" ]; then
    echo "Framework [libwebsockets] already exists"
else
    echo "Clone libwebsockets in ${LIBWEBSOCKETS_DIR}"
    wget https://github.com/warmcat/libwebsockets/archive/refs/tags/v4.3.3.zip --no-check-certificate
    unzip v4.3.3.zip
    cd ${LIBWEBSOCKETS_DIR}
    mkdir build
    cd build
    ${CMAKE_BIN} .. -DLWS_WITH_SSL=OFF -DLWS_WITH_ZIP_FOPS=OFF -DLWS_WITH_ZLIB=OFF -DLWS_WITHOUT_BUILTIN_GETIFADDRS=ON \
    -DLWS_WITHOUT_CLIENT=ON -DLWS_WITHOUT_EXTENSIONS=ON -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITH_SHARED=ON \
    -DLWS_WITHOUT_TEST_SERVER=ON -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=ON -DLWS_WITH_MINIMAL_EXAMPLES=ON \
    -DLWS_WITHOUT_DAEMONIZE=ON -DCMAKE_C_FLAGS=-fPIC -DLWS_WITH_NO_LOGS=ON -DCMAKE_BUILD_TYPE=Release
    make $@
fi
popd > /dev/null

pushd ${FRAMEWORK_DIR} > /dev/null
check_file_exists() {
    search_paths="/usr/include /usr/local/include /usr/lib /usr/local/lib"
    local TARGET=$1
    local file_name=$2

    if find ${search_paths} -type f -name "${file_name}" -print -quit | grep -q .; then
        echo "found"
    else
        echo "not_found"
    fi
}

if [ "$TARGET" = "arm" ]; then
    TARGET=arm
    LIBCURL_IS_INSTALLED=0
    OPENSSL_IS_INSTALLED=0
else
    TARGET=linux

    CURL_LIB=$(check_file_exists ${TARGET} "libcurl.so*")
    CURL_HEADER=$(check_file_exists ${TARGET} "curl.h")
    LIBCURL_IS_INSTALLED=0
    if [ "$CURL_HEADER" = "found" ] && [ "$CURL_LIB" = "found" ]; then
        LIBCURL_IS_INSTALLED=1
    fi

    # Search for libssl.so* files and check its version
    result=""
    search_paths="/usr/include /usr/local/include /usr/lib /usr/local/lib"
    for file_path in $(find ${search_paths} -iname "libssl.so.1*"); do
        # Check the file type and version
        if file "${file_path}" | grep -i -q "version 1"; then
            result="${file_path}"
            break
        fi
    done

    OPENSSL_IS_INSTALLED=0
    if [ -n "${result}" ]; then
        echo "Version 1 of libssl.so is found for ${TARGET}.Also assuming libcrypto is also available in the same path"
        OPENSSL_IS_INSTALLED=1
    fi
fi
popd > /dev/null # ${FRAMEWORK_DIR}

pushd ${FRAMEWORK_DIR} > /dev/null
if [ "$OPENSSL_IS_INSTALLED" -eq 0 ]; then
    if [ -d "${OPENSSL_DIR}" ]; then
        echo "Framework [openssl] already exists"
    else
        echo "Clone openssl in ${OPENSSL_DIR} for $TARGET"
        wget https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_1_1_1w.zip -P openssl --no-check-certificate
        cd openssl
        unzip OpenSSL_1_1_1w.zip
        cd openssl-OpenSSL_1_1_1w/
        mkdir build
        if [ "$TARGET" = "arm" ]; then
            # For arm
            CROSS_COMPILE=
            COMPILER_FLAGS=$(echo $CC | cut -d' ' -f2-)
            /usr/bin/perl ./Configure linux-armv4 shared --prefix=${OPENSSL_DIR}/build --openssldir=${OPENSSL_DIR} --cross-compile-prefix=${CROSS_COMPILE} $COMPILER_FLAGS
        else
            # For linux
            ./config --prefix=${OPENSSL_DIR}/build
        fi
        make && make install
    fi
fi
popd > /dev/null # ${FRAMEWORK_DIR}

pushd ${FRAMEWORK_DIR} > /dev/null
if [ "${LIBCURL_IS_INSTALLED}" -eq 0 ]; then
    if [ -d "${CURL_DIR}" ]; then
        echo "Framework [curl] already exists"
    else
        echo "Clone curl in ${CURL_DIR} for $TARGET"
        wget https://curl.se/download/curl-8.8.0.zip -P curl --no-check-certificate
        cd curl
        unzip curl-8.8.0.zip
        cd curl-8.8.0
        mkdir build
        if [ "$TARGET" = "arm" ]; then
            # For arm
            ./configure CPPFLAGS="-I${OPENSSL_DIR}/build/include" LDFLAGS="-L${OPENSSL_DIR}/build/lib" --prefix=${CURL_DIR}/build --host=arm-rdk-linux-gnueabi --with-ssl=${OPENSSL_DIR}/build --with-pic
        else
            # For linux
            ./configure --prefix=$(pwd)/build --with-ssl
        fi
        make $@; make $@ install
    fi
fi
popd > /dev/null # ${FRAMEWORK_DIR}
