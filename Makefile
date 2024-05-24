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

TARGET_DYNAMIC_LIB ?= libutcontrollibrary.so
TARGET_STATIC_LIB ?= libutcontrollibrary.a

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

$(info $(shell echo -e ${GREEN}TARGET_DYNAMIC_LIB [$(TARGET_DYNAMIC_LIB)]${NC}))
$(info $(shell echo -e ${GREEN}TARGET_STATIC_LIB [$(TARGET_STATIC_LIB)]${NC}))

UT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
export PATH := $(shell pwd)/toolchain:$(PATH)
TOP_DIR ?= $(UT_DIR)
BUILD_DIR ?= $(TOP_DIR)/obj
BIN_DIR ?= $(TOP_DIR)/bin
XCFLAGS := $(KCFLAGS)
LIB_DIR := $(TOP_DIR)/lib
BUILD_LIBS = yes

CFLAGS += -fPIC -Wall    # Flags for compilation
LDFLAGS = -shared

# Enable libyaml Requirements
LIBFYAML_DIR = ${TOP_DIR}/framework/libfyaml-master
ASPRINTF_DIR = ${TOP_DIR}/framework/asprintf/asprintf.c-master/
SRC_DIRS += $(LIBFYAML_DIR)/src/lib
SRC_DIRS += $(LIBFYAML_DIR)/src/thread
SRC_DIRS += $(LIBFYAML_DIR)/src/util
SRC_DIRS += $(LIBFYAML_DIR)/src/xxhash
SRC_DIRS += $(ASPRINTF_DIR)
INC_DIRS += $(LIBFYAML_DIR)/include
INC_DIRS += $(ASPRINTF_DIR)

# LIBWEBSOCKETS Requirements
LIBWEBSOCKETS_DIR = $(TOP_DIR)/framework/libwebsockets-4.3.3
INC_DIRS += $(LIBWEBSOCKETS_DIR)/include
INC_DIRS += $(LIBWEBSOCKETS_DIR)/build
LDFLAGS += -L $(LIBWEBSOCKETS_DIR)/build/lib -l:libwebsockets.a

# UT Control library Requirements
SRC_DIRS += ${TOP_DIR}/src
INC_DIRS += ${TOP_DIR}/include
#CFLAGS += -DUT_LOG_ENABLED

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)

#OBJS := $(SRCS:.c=.o)
OBJS := $(subst $(TOP_DIR),$(BUILD_DIR),$(SRCS:.c=.o))

INC_DIRS += $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

#VERSION=$(shell git describe --tags | head -n1)

#$(info VERSION [$(VERSION)])

# Final conversions
DEPS += $(OBJS:.o=.d)

# Here is a list of exports from this makefile to the next
export BIN_DIR
export SRC_DIRS
export INC_DIRS
export BUILD_DIR
export LIB_DIR
#export TOP_DIR
export TARGET
export TARGET_EXEC
export TARGET_DYNAMIC_LIB
export TARGET_STATIC_LIB
export CFLAGS
export LDFLAGS
export BUILD_LIBS
export LDFLAGS

.PHONY: clean list build lib all

list:
	@echo ${GREEN}List [$@]${NC}
	@echo INC_DIRS: ${INC_DIRS}
	@echo SRC_DIRS: ${SRC_DIRS}
	@echo CFLAGS: ${CFLAGS}
	@echo LDFLAGS: ${LDFLAGS}
	@echo TARGET_DYNAMIC_LIB: ${TARGET_DYNAMIC_LIB}
	@echo TARGET_STATIC_LIB: ${TARGET_STATIC_LIB}
	@make -C ../../ list TARGET=linux

clean:
	@echo -e ${GREEN}Performing Clean${NC}
	@$(RM) -rf $(BUILD_DIR) $(LIB_DIR)
	@echo -e ${GREEN}Clean Completed${NC}

cleanall:
	@echo -e ${GREEN}Performing Clean on frameworks [$(TOP_DIR)/framework]${NC}
	@${RM} -rf $(TOP_DIR)/framework

lib:
	@mkdir -p $(LIB_DIR)
	@make -C ../../ lib TARGET=linux
