#* ******************************************************************************
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
#* ******************************************************************************
#*
#*   ** Project      : ut
#*   ** @addtogroup  : ut
#*   ** @file        : makefile
#*   ** @date        : 20/05/2022
#*   **
#*   ** @brief       : Makefile for UT
#*   **
#*
#* ******************************************************************************
ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BIN_DIR := $(ROOT_DIR)/build/bin
TOP_DIR := $(ROOT_DIR)

SRC_DIRS = $(ROOT_DIR)/src
INC_DIRS := $(ROOT_DIR)/../include
LIB_DIR = $(ROOT_DIR)/../build/${TARGET}/lib
BUILD_DIR = $(ROOT_DIR)/build/$(TARGET)/obj

ifeq ($(TARGET),)
$(info TARGET FORCED TO Linux)
TARGET=linux
endif

$(info TARGET [$(TARGET)])

ifeq ($(TARGET),arm)
HAL_LIB_DIR := $(ROOT_DIR)/libs
endif

YLDFLAGS += -L $(LIB_DIR) -lut_control -pthread

TARGET_EXEC = ut_control_test	
CFLAGS += -DNDEBUG
# CFLAGS += -DWEBSOCKET_SERVER

.PHONY: clean list all

export YLDFLAGS
export CFLAGS
export BIN_DIR
export SRC_DIRS
export INC_DIRS
export TARGET
export TOP_DIR
export HAL_LIB_DIR
export TARGET_EXEC
export BUILD_DIR
export LIB_DIR

.PHONY: clean list build skeleton

build:
	@/bin/echo UT [$@]
	$(ROOT_DIR)/build.sh
	make -C ./ut-core test
	@cp $(LIB_DIR)/lib* ${BIN_DIR}
	@cp ${ROOT_DIR}/src/*.sh ${BIN_DIR}
	@cp ${ROOT_DIR}/websocket-clients/* ${BIN_DIR}
	@/bin/echo -e ${GREEN}Copy Assets to [${BIN_DIR}/assets] ${NC}
	@mkdir -p ${BIN_DIR}/assets
	@cp -r ${ROOT_DIR}/src/assets/* ${BIN_DIR}/assets

list:
	@/bin/echo UT [$@]
	make -C ./ut-core list

clean:
	@/bin/echo UT [$@]
	@$(RM) -rf $(BIN_DIR)

cleanall:
	@/bin/echo UT [$@]
	@$(RM) -rf $(BIN_DIR)
	make -C ../ -f Makefile clean
	make -C ./ut-core clean
