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

TARGET_BIN?= ut-control-test

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

$(info $(shell echo -e ${GREEN}TARGET_DYNAMIC_LIB [$(TARGET_DYNAMIC_LIB)]${NC}))

UT_CONTROL_TEST_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
export PATH := $(shell pwd)/toolchain:$(PATH)
TOP_DIR ?= $(UT_CONTROL_TEST_DIR)
BIN_DIR ?= $(TOP_DIR)/bin
UT_DIR ?= $(UT_CONTROL_TEST_DIR)/ut-core

# Enable CUnit Requirements
CFLAGS += -DUT_CUNIT
CUNIT_DIR +=  $(UT_CONTROL_TEST_DIR)/ut-core/framework/CUnit-2.1-3/CUnit
CUNIT_SRC_DIRS += $(CUNIT_DIR)/Sources
INC_DIRS += $(CUNIT_DIR)/Headers
SRC_DIRS += $(CUNIT_SRC_DIRS)/Framework
SRC_DIRS += $(UT_DIR)/src
INC_DIRS += $(UT_DIR)/include
INC_DIRS += $(UT_DIR)/src
INC_DIRS += $(UT_CONTROL_TEST_DIR)/../include

INC_DIRS += $(UT_CONTROL_TEST_DIR)/include
INC_DIRS += $(UT_DIR)/include
SRC_DIRS += $(UT_CONTROL_TEST_DIR)/src

XLDFLAGS += -L $(UT_CONTROL_TEST_DIR)/lib -lutcontrollibrary -pthread
export LD_LIBRARY_PATH=$(UT_CONTROL_TEST_DIR)/lib/

VERSION=$(shell cd $(UT_DIR) && git describe --tags | head -n1)

$(info VERSION [$(VERSION)])

XCFLAGS += $(CFLAGS) $(INC_FLAGS) -D UT_VERSION=\"$(VERSION)\"

MKDIR_P ?= @mkdir -p

$(info TARGET [$(TARGET)])

export SRC_DIRS
export INC_DIRS
export LDFLAGS
export TARGET_BIN
export CFLAGS
export XCFLAGS
export XLDFLAGS

.PHONY: clean list build test all

test: build
	@make -C ../ test TARGET=linux

build:
	$(eval SHELL:=/usr/bin/env bash)
	@echo -e ${GREEN}"Ensure ut-core is present"${NC}
	cd ${UT_CONTROL_TEST_DIR}
	./build.sh
	@echo -e ${GREEN}Completed${NC}

list:
	@echo ${GREEN}List [$@]${NC}
	@echo INC_DIRS: ${INC_DIRS}
	@echo SRC_DIRS: ${SRC_DIRS}
	@echo CFLAGS: ${CFLAGS}
	@echo XLDFLAGS: ${XLDFLAGS}
	@echo TARGET_BIN: ${TARGET_BIN}
	@echo CFLAGS: ${CFLAGS}
	@echo XCFLAGS: ${XCFLAGS}
	@make -C ../ list TARGET=linux

clean:
	@echo ${GREEN}Clean [$@]${NC}
	@$(RM) -rf $(UT_CONTROL_TEST_DIR)/lib
	@make -C ../ clean TARGET=linux

cleanall:
	@echo ${GREEN}Clean [$@]${NC}
	@${RM} -rf $(UT_DIR)
	@make -C ../ cleanall TARGET=linux








