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
CURL_DIR=${FRAMEWORK_DIR}/curl
OPENSSL_DIR=${FRAMEWORK_DIR}/openssl/openssl-OpenSSL_1_1_1w

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
    cmake .. -DLWS_WITH_SSL=OFF -DLWS_WITH_ZIP_FOPS=OFF -DLWS_WITH_ZLIB=OFF -DLWS_WITHOUT_BUILTIN_GETIFADDRS=ON \
    -DLWS_WITHOUT_CLIENT=ON -DLWS_WITHOUT_EXTENSIONS=ON -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITH_SHARED=ON \
    -DLWS_WITHOUT_TEST_SERVER=ON -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=ON -DLWS_WITH_MINIMAL_EXAMPLES=ON \
    -DLWS_WITHOUT_DAEMONIZE=ON -DCMAKE_C_FLAGS=-fPIC -DLWS_WITH_NO_LOGS=ON -DCMAKE_BUILD_TYPE=Release
    make $@
fi
popd > /dev/null

pushd ${FRAMEWORK_DIR} > /dev/null
if [ "$TARGET" = "arm" ]; then
    TARGET=arm
else
    TARGET=linux
fi

if [ "$TARGET" = "arm" ]; then
    search_paths=$SDKTARGETSYSROOT
else
    search_paths="/usr/include /usr/local/include /usr/lib /usr/local/lib"
fi

if find ${search_paths} -name 'curl.h' -print -quit | grep -q 'curl.h'; then
    echo "curl.h found for ${TARGET}"
    if find ${search_paths} -name 'libcurl.so*' -print -quit | grep -q 'libcurl.so*'; then
        echo "libcurl.so* found for ${TARGET}"
    else
        echo "libcurl.so* not found for ${TARGET}"
        LIBCURL_IS_INSTALLED=0
    fi
else
    echo "curl.h not found for ${TARGET}"
    LIBCURL_IS_INSTALLED=0
fi

# Search for libssl.so* files and check its version
result=""
for file_path in $(find ${search_paths} -iname "libssl.so.1*"); do
    # Check the file type and version
    if file "${file_path}" | grep -i -q "version 1"; then
        result="${file_path}"
        break
    fi
done

# Check if the result is empty or not
if [ -n "${result}" ]; then
    echo "Version 1 of libssl.so is found for ${TARGET}.Also assuming libcrypto is also available in the same path"
    OPENSSL_IS_INSTALLED=1
else
    echo "Version 1 of libssl.so is not found for ${TARGET}."
    OPENSSL_IS_INSTALLED=0
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
            CROSS_COMPILE=
            COMPILER_FLAGS=$(echo $CC | cut -d' ' -f2-)
            /usr/bin/perl ./Configure linux-armv4 shared --prefix=${OPENSSL_DIR}/build --openssldir=${OPENSSL_DIR} --cross-compile-prefix=${CROSS_COMPILE} $COMPILER_FLAGS

        else
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
            ./configure CPPFLAGS="-I${OPENSSL_DIR}/build/include" LDFLAGS="-L${OPENSSL_DIR}/build/lib" --prefix=$(pwd)/build --host=arm-rdk-linux-gnueabi --with-ssl=${OPENSSL_DIR}/build
        else
            ./configure --prefix=$(pwd)/build --with-ssl
        fi
        make $@; make $@ install
    fi
fi
popd > /dev/null # ${FRAMEWORK_DIR}
