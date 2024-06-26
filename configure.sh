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
popd > /dev/null # ${FRAMEWORK_DIR}

