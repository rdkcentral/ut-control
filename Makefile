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

UT_CONTROL_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
export PATH := $(shell pwd)/toolchain:$(PATH)
TOP_DIR ?= $(UT_CONTROL_DIR)
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
XCFLAGS += $(CFLAGS) $(INC_FLAGS)

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

MKDIR_P ?= @mkdir -p

TARGET = linux

$(info TARGET [$(TARGET)])

# defaults for target arm
ifeq ($(TARGET),arm)
CUNIT_VARIANT=arm-rdk-linux-gnueabi
#CC := arm-rdk-linux-gnueabi-gcc -mthumb -mfpu=vfp -mcpu=cortex-a9 -mfloat-abi=soft -mabi=aapcs-linux -mno-thumb-interwork -ffixed-r8 -fomit-frame-pointer
# CFLAGS will be overriden by Caller as required
INC_DIRS += $(UT_DIR)/sysroot/usr/include
endif

# Defaults for target linux
ifeq ($(TARGET),linux)
CUNIT_VARIANT=i686-pc-linux-gnu
CC := gcc -ggdb -o0 -Wall
AR := ar
endif

.PHONY: clean list build lib all

lib : static_lib dynamic_lib
# Rule to create the shared library
dynamic_lib: ${OBJS}
	@echo -e ${GREEN}Building dyanamic lib [${YELLOW}$(LIB_DIR)/$(TARGET_DYNAMIC_LIB)${GREEN}]${NC}
	@$(CC) $(CFLAGS) -o $(LIB_DIR)/$(TARGET_DYNAMIC_LIB) $^ $(LDFLAGS)

static_lib: $(OBJS)
	@echo -e ${GREEN}Building static lib [${YELLOW}$(LIB_DIR)/$(TARGET_STATIC_LIB)${GREEN}]${NC}
	@$(MKDIR_P) $(LIB_DIR)
	@$(AR) rcs $(LIB_DIR)/$(TARGET_STATIC_LIB) $^

# Make any c source
$(BUILD_DIR)/%.o: %.c
	@echo -e ${GREEN}Building [${YELLOW}$<${GREEN}]${NC}
	@$(MKDIR_P) $(dir $@)
	@$(CC) $(XCFLAGS) -c $< -o $@

.PHONY: clean list arm linux framework lib

all: framework linux

# Ensure the framework is built
framework: $(eval SHELL:=/usr/bin/env bash)
	@echo -e ${GREEN}"Ensure framework is present"${NC}
	${TOP_DIR}/install.sh
	@echo -e ${GREEN}Completed${NC}

arm:
	make TARGET=arm

linux: framework
	make TARGET=linux

list:
	@echo ${GREEN}List [$@]${NC}
	@echo INC_DIRS: ${INC_DIRS}
	@echo SRC_DIRS: ${SRC_DIRS}
	@echo CFLAGS: ${CFLAGS}
	@echo LDFLAGS: ${LDFLAGS}
	@echo TARGET_DYNAMIC_LIB: ${TARGET_DYNAMIC_LIB}
	@echo TARGET_STATIC_LIB: ${TARGET_STATIC_LIB}

clean:
	@echo -e ${GREEN}Performing Clean${NC}
	@$(RM) -rf $(BUILD_DIR) $(LIB_DIR)
	@echo -e ${GREEN}Clean Completed${NC}

cleanall:
	@echo -e ${GREEN}Performing Clean on frameworks [$(TOP_DIR)/framework]${NC}
	@${RM} -rf $(TOP_DIR)/framework
