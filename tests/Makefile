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
ECHOE = /bin/echo -e

SRC_DIRS = $(ROOT_DIR)/src
INC_DIRS := $(ROOT_DIR)/../include
LIB_DIR = $(ROOT_DIR)/../build/${TARGET}/lib
BUILD_DIR = $(ROOT_DIR)/build/$(TARGET)/obj

ifeq ($(TARGET),)
$(info TARGET FORCED TO Linux)
TARGET=linux
CFLAGS   += -g -O1 -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak
CXXFLAGS += $CFLAGS
YLDFLAGS  += -fsanitize=address -fsanitize=leak
endif

$(info TARGET [$(TARGET)])

ifeq ($(TARGET),arm)
HAL_LIB_DIR := $(ROOT_DIR)/libs
endif

YLDFLAGS += -L$(LIB_DIR) -lut_control -pthread

TARGET_EXEC = ut_control_test
CFLAGS += -DNDEBUG
# CFLAGS += -DWEBSOCKET_SERVER

# Optional compile-time log level override.
# Usage: make TARGET=linux UT_LOG_LEVEL=4   (or export UT_LOG_LEVEL=4 before calling make)
# Valid values: 0=NONE 1=ERROR 2=WARNING(default) 3=INFO 4=DEBUG
# NOTE: must be a numeric value; symbolic names (e.g. UT_LOG_LEVEL=DEBUG) are not accepted
#       because the preprocessor silently treats unknown tokens as 0.
ifneq ($(UT_LOG_LEVEL),)
$(if $(filter-out 0 1 2 3 4,$(UT_LOG_LEVEL)),\
    $(error UT_LOG_LEVEL must be a number 0-4: 0=NONE 1=ERROR 2=WARNING 3=INFO 4=DEBUG (got '$(UT_LOG_LEVEL)')))
CFLAGS += -DUT_LOG_LEVEL=$(UT_LOG_LEVEL)
endif

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
	@$(ECHOE) UT [$@]
	$(ROOT_DIR)/build.sh
	make -C ./ut-core test
	@cp $(LIB_DIR)/lib* ${BIN_DIR}
	@cp ${ROOT_DIR}/src/*.sh ${BIN_DIR}
	@cp ${ROOT_DIR}/websocket-clients/* ${BIN_DIR}
	@$(ECHOE) ${GREEN}Copy Assets to [${BIN_DIR}/assets] ${NC}
	@mkdir -p ${BIN_DIR}/assets
	@cp -r ${ROOT_DIR}/src/assets/* ${BIN_DIR}/assets

list:
	@$(ECHOE) UT [$@]
	make -C ./ut-core list

clean:
	@$(ECHOE) UT [$@]
	@$(RM) -rf $(BIN_DIR)

cleanall:
	@$(ECHOE) UT [$@]
	@$(RM) -rf $(BIN_DIR)
	make -C ../ -f Makefile clean
	make -C ./ut-core clean
