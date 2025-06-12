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

# Decide which build target to use based on the presence of TARGET in input args(linux or arm)
if [[ "$1" != "linux" && "$1" != "arm" ]]; then
  echo "Error: argument must be 'linux' or 'arm'"
  exit 1
fi
TARGET=${1}
echo "[$0] TARGET [${TARGET}]"

pushd ${MY_DIR} > /dev/null

FRAMEWORK_DIR=${MY_DIR}/framework/${TARGET}
LIBYAML_DIR=${FRAMEWORK_DIR}/libfyaml-master
LIBYAML_VERSION=997b480cc4239a7f55771535dff52ad69bd4eb5b #30th September 2024

ASPRINTF_DIR=${FRAMEWORK_DIR}/asprintf
ASPRINTF_VERSION=0.0.3

LIBWEBSOCKETS_DIR=${FRAMEWORK_DIR}/libwebsockets-4.3.3
CURL_DIR=${FRAMEWORK_DIR}/curl/curl-8.8.0
OPENSSL_DIR=${FRAMEWORK_DIR}/openssl/openssl-OpenSSL_1_1_1w
CMAKE_DIR=${MY_DIR}/host-tools/CMake-3.30.0
CMAKE_BIN_DIR=${CMAKE_DIR}/build/bin
HOST_CC=gcc
BUILD_DIR=${MY_DIR}/build/${TARGET}
LIBWEBSOCKETS_BUILD_DIR=${BUILD_DIR}/libwebsockets
OPENSSL_BUILD_DIR=${BUILD_DIR}/openssl
CURL_BUILD_DIR=${BUILD_DIR}/curl

mkdir -p ${FRAMEWORK_DIR}
mkdir -p ${BUILD_DIR}
popd > /dev/null

# Download and set up the 'asprintf' library if not already present
pushd ${FRAMEWORK_DIR} > /dev/null
if [ -d "${ASPRINTF_DIR}" ]; then
    echo "Framework [asprintf] already exists"
else
    echo "wget asprintf in ${ASPRINTF_DIR}"
    # Pull fixed version 
    wget https://github.com/jwerle/asprintf.c/archive/refs/tags/${ASPRINTF_VERSION}.zip -P asprintf/. --no-check-certificate
    cd asprintf
    unzip ${ASPRINTF_VERSION}.zip
    mv asprintf.c-${ASPRINTF_VERSION} asprintf.c-master
    rm asprintf.c-master/test.c
    echo "Patching Framework [${PWD}]"
    cd asprintf.c-master
    cp ../../../../src/asprintf/patches/FixWarningsInAsprintf.patch  .
    patch -i FixWarningsInAsprintf.patch -p0
fi
popd > /dev/null

# Download and set up the 'libfyaml' library if not already present
pushd ${FRAMEWORK_DIR} > /dev/null
if [ -d "${LIBYAML_DIR}" ]; then
    echo "Framework [libfyaml] already exists"
else
    echo "wget libfyaml in ${LIBYAML_DIR}"
    # Pull fixed version
    wget https://github.com/pantoniou/libfyaml/archive/${LIBYAML_VERSION}.zip --no-check-certificate
    unzip ${LIBYAML_VERSION}.zip
    mv libfyaml-${LIBYAML_VERSION} libfyaml-master
    echo "Patching Framework [${PWD}]"
    # Copy the patch file from src directory
    cp ../../src/libyaml/patches/CorrectWarningsAndBuildIssuesInLibYaml.patch  .
    patch -i CorrectWarningsAndBuildIssuesInLibYaml.patch -p0
    echo "Patching Complete"
    #    ./bootstrap.sh
    #    ./configure --prefix=${LIBYAML_DIR}
    #    make
fi
popd > /dev/null

# Prefer system cmake if version is > 3.12
pushd ${MY_DIR}
if command -v cmake &> /dev/null && [ "$(cmake --version | head -n1 | awk '{print $3}')" \> "3.12" ]; then
    echo "CMake is installed and version is 3.13 or higher"
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

# Set up paths and search criteria based on the target architecture (linux or arm)
pushd "${FRAMEWORK_DIR}" > /dev/null

if [ "$TARGET" == "arm" ]; then
    TARGET=arm
    # Extract the sysroot value
    if [ "${CC}" == "" ]; then
        echo "CC is not set.. Exiting"
        exit 1
    fi
    SYSROOT=$(echo "$CC" | grep -oP '(?<=--sysroot=)[^ ]+')
    search_paths=$SYSROOT
else
    TARGET=linux
    search_paths="/usr/include /usr/local/include /usr/lib /usr/local/lib"
fi

OPENSSL_IS_SYSTEM_INSTALLED=0
# Output the path to a txt file
output_file="${MY_DIR}/file_path.txt"

# Function to search for a file and dump its path to a file if found
dump_library_path()
{
    local library_name="$1"
    local paths="$2"

    for file_path in $(find ${paths} -iname "${library_name}" 2>/dev/null); do
        # Check if the file is valid and accessible
        if file "${file_path}" &> /dev/null; then
            # Dump the key-value pair to the output file
            echo "${library_name}=${file_path}" >> "${output_file}"
            echo "${file_path}"    # Return the found path
            return 0               # Exit the function after finding the first match
        fi
    done

    return 1  # Return an error code if the file was not found
}

# Check package-config file and static library for OpenSSL
OPENSSL_PC=$(dump_library_path "openssl.pc" "${search_paths}") || echo "openssl.pc not found"
OPENSSL_STATIC_SSL_LIB=$(dump_library_path "libssl.a" "${search_paths}") || echo "libssl.a not found"
OPENSSL_STATIC_CRYPTO_LIB=$(dump_library_path "libcrypto.a" "${search_paths}") || echo "libcrypto.a not found"

if [[ -n "$OPENSSL_PC" && -n "$OPENSSL_STATIC_SSL_LIB" && -n "$OPENSSL_STATIC_CRYPTO_LIB" ]]; then
    echo "openssl.pc, libssl.a, and libcrypto.a are found for ${TARGET}."
    OPENSSL_IS_SYSTEM_INSTALLED=1
fi

# Check for curl
CURL_LIB=$(dump_library_path "libcurl.a" "${search_paths}") || echo "libcurl.a not found"
CURL_HEADER=$(dump_library_path "curl.h" "${search_paths}") || echo "curl.h not found"
LIBCURL_IS_SYSTEM_INSTALLED=0

if [[ -n "$CURL_HEADER" && -n "$CURL_LIB" ]]; then
    LIBCURL_IS_SYSTEM_INSTALLED=1
fi

popd > /dev/null # ${FRAMEWORK_DIR}

# Switch                             Description                                                 Effect on LWS Build
# -DLWS_WITH_SSL=OFF                 Disables Secure Sockets Layer (SSL) support in             Reduces library size, removes dependency on OpenSSL.
#                                    libwebsockets.
# -DLWS_WITH_ZIP_FOPS=OFF            Disables support for ZIP file operations in                Removes zlib dependency, reduces library size.
#                                    libwebsockets.
# -DLWS_WITH_ZLIB=OFF                Disables zlib compression support in libwebsockets.        Reduces library size, disables compression for WebSocket payloads.
# -DLWS_WITHOUT_BUILTIN_GETIFADDRS=ONDisables the built-in mechanism to obtain network          Relies on external libraries or system calls to get interface addresses.
#                                    interface addresses.
# -DLWS_WITHOUT_CLIENT=ON            Removes the WebSocket client functionality from the        Only server-side WebSocket functionality is included.
#                                    library.
# -DLWS_WITHOUT_EXTENSIONS=ON        Disables LWS extensions (e.g., permessage-deflate,         Reduces library size, potentially improves performance.
#                                    client_no_context_takeover).
# -DLWS_WITHOUT_TESTAPPS=ON          Excludes test applications from the build.                 Streamlines the build, focuses on core library components.
# -DLWS_WITH_SHARED=ON               Builds libwebsockets as a shared library (.so file).       Allows the library to be used by multiple applications.
# -DLWS_WITHOUT_TEST_SERVER=ON       Excludes the test server component from the build.         Streamlines the build.
# -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=ONDisables the external poll mechanism in the test server.  May affect testing capabilities of the library.
# -DLWS_WITH_MINIMAL_EXAMPLES=ON     Includes only the minimal set of example programs.         Reduces the number of example programs built.
# -DLWS_WITHOUT_DAEMONIZE=ON         Prevents the LWS test server from running as a daemon.     Useful for debugging and when you want to control the server process directly.
# -DCMAKE_C_FLAGS=-fPIC              Adds the -fPIC flag to the C compiler, enabling            Makes the generated code relocatable, necessary for shared libraries.
#                                    Position-Independent Code (PIC).
# -DLWS_WITH_NO_LOGS=ON              Disables logging in libwebsockets.                         Can improve performance but makes debugging more difficult.
# -DCMAKE_BUILD_TYPE=Release         Configures the build for release mode (optimization        Optimizes the library for performance.
#                                    enabled, debugging symbols stripped).
# -DCMAKE_POSITION_INDEPENDENT_CODE=ON Enables Position-Independent Code (PIC) for the          Makes all generated code relocatable.
#                                     entire project, including executables and shared libraries.


# Build and configure libwebsockets if not already built
pushd ${FRAMEWORK_DIR} > /dev/null

build_libwebsockets()
{
    cd ${LIBWEBSOCKETS_DIR}
    mkdir -p ${LIBWEBSOCKETS_BUILD_DIR}
    cd ${LIBWEBSOCKETS_BUILD_DIR}
    ${CMAKE_BIN} ${LIBWEBSOCKETS_DIR} -DLWS_WITH_SSL=OFF -DLWS_WITH_ZIP_FOPS=OFF -DLWS_WITH_ZLIB=OFF -DLWS_WITHOUT_BUILTIN_GETIFADDRS=ON \
    -DLWS_WITHOUT_CLIENT=ON -DLWS_WITHOUT_EXTENSIONS=ON -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITH_SHARED=ON \
    -DLWS_WITHOUT_TEST_SERVER=ON -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=ON -DLWS_WITH_MINIMAL_EXAMPLES=ON \
    -DLWS_WITHOUT_DAEMONIZE=ON -DCMAKE_C_FLAGS=-fPIC -DLWS_WITH_NO_LOGS=ON -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLWS_HAVE_LIBCAP=0
    make $@
    touch ${LIBWEBSOCKETS_BUILD_DIR}/.build_complete
}

if [ -d "${LIBWEBSOCKETS_DIR}" ]; then
    echo "Framework [libwebsockets] already exists"
    if [ -f "${LIBWEBSOCKETS_BUILD_DIR}/.build_complete" ]; then
        echo "Framework [libwebsockets] already built for ${TARGET}"
    else
        build_libwebsockets
    fi
else
    echo "Clone libwebsockets in ${LIBWEBSOCKETS_DIR}"
    wget https://github.com/warmcat/libwebsockets/archive/refs/tags/v4.3.3.zip --no-check-certificate
    unzip v4.3.3.zip
    build_libwebsockets
fi
popd > /dev/null


pushd ${FRAMEWORK_DIR} > /dev/null

# Build and configure OpenSSL if it's not already installed
build_openssl()
{
    cd ${OPENSSL_DIR}
    mkdir -p ${OPENSSL_BUILD_DIR}
    if [ "$TARGET" = "arm" ]; then
        # For arm
        CROSS_COMPILE=
        COMPILER_FLAGS=$(echo $CC | cut -d' ' -f2-)
        /usr/bin/perl ./Configure linux-armv4 shared --prefix=${OPENSSL_BUILD_DIR} --openssldir=${OPENSSL_BUILD_DIR} --cross-compile-prefix=${CROSS_COMPILE} $COMPILER_FLAGS
    else
        # For linux
        ./config --prefix=${OPENSSL_BUILD_DIR}
    fi
    make && make install
    touch ${OPENSSL_BUILD_DIR}/.build_complete
}

if [ "$OPENSSL_IS_SYSTEM_INSTALLED" -eq 0 ]; then
    if [ -d "${OPENSSL_DIR}" ]; then
        echo "Framework [openssl] already exists"
        if [ -f "${OPENSSL_BUILD_DIR}/.build_complete" ]; then
            echo "Framework [openssl] already built for ${TARGET}"
        else
            echo "Building Framework [openssl] for ${TARGET}"
            build_openssl
        fi
    else
        echo "Clone openssl in ${OPENSSL_DIR} for $TARGET"
        wget https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_1_1_1w.zip -P openssl --no-check-certificate
        cd openssl
        unzip OpenSSL_1_1_1w.zip
        build_openssl
    fi
fi
popd > /dev/null # ${FRAMEWORK_DIR}

pushd ${FRAMEWORK_DIR} > /dev/null

# Build and configure curl if it's not already installed
build_curl()
{
    cd ${CURL_DIR}
    mkdir -p ${CURL_BUILD_DIR}
    if [ "$TARGET" = "arm" ]; then
        # For arm
        ./configure CPPFLAGS="-I${OPENSSL_BUILD_DIR}/include" LDFLAGS="-L${OPENSSL_BUILD_DIR}/lib" --prefix=${CURL_BUILD_DIR} --host=arm-none-linux-gnu --with-ssl=${OPENSSL_BUILD_DIR} --enable-shared --enable-static --with-pic --without-libpsl --without-libidn2 --disable-docs --disable-libcurl-option --disable-alt-svc --disable-headers-api --disable-hsts --without-libgsasl --without-zlib
    else
        # For linux
        if [ "$OPENSSL_IS_SYSTEM_INSTALLED" -eq 1 ]; then
            ./configure --prefix=${CURL_BUILD_DIR} --with-ssl --without-zlib --without-libpsl --without-libidn2 --disable-docs --disable-libcurl-option --disable-alt-svc --disable-headers-api --disable-hsts --without-libgsasl  --enable-shared --enable-static
        else
            ./configure CPPFLAGS="-I${OPENSSL_BUILD_DIR}/include" LDFLAGS="-L${OPENSSL_BUILD_DIR}/lib" --prefix=${CURL_BUILD_DIR} --with-ssl=${OPENSSL_BUILD_DIR} --with-pic --without-zlib --without-libpsl --without-libidn2 --disable-docs --disable-libcurl-option --disable-alt-svc --disable-headers-api --disable-hsts --without-libgsasl --enable-shared --enable-static
        fi
    fi
    make $@; make $@ install
    touch ${CURL_BUILD_DIR}/.build_complete
}

if [ "${LIBCURL_IS_SYSTEM_INSTALLED}" -eq 0 ]; then
    if [ -d "${CURL_DIR}" ]; then
        echo "Framework [curl] already exists"
        if [ -f "${CURL_BUILD_DIR}/.build_complete" ]; then
            echo "Framework [curl] already built for ${TARGET}"
        else
            build_curl
        fi
    else
        echo "Clone curl in ${CURL_DIR} for $TARGET"
        wget https://curl.se/download/curl-8.8.0.zip -P curl --no-check-certificate
        cd curl
        unzip curl-8.8.0.zip
        build_curl
    fi
fi
popd > /dev/null # ${FRAMEWORK_DIR}
