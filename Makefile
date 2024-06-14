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

TARGET_LIB ?= libut_control.so

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

$(info $(shell echo -e ${GREEN}TARGET_LIB [$(TARGET_LIB)]${NC}))

UT_CONTROL_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
export PATH := $(shell pwd)/toolchain:$(PATH)
TOP_DIR ?= $(UT_CONTROL_DIR)
BUILD_DIR ?= $(TOP_DIR)/obj
BIN_DIR ?= $(TOP_DIR)/bin
LIB_DIR := $(TOP_DIR)/lib
BUILD_LIBS = yes

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

CFLAGS += -fPIC -Wall -shared   # Flags for compilation
CFLAGS += -DNDEBUG

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)

#OBJS := $(SRCS:.c=.o)
OBJS := $(subst $(TOP_DIR),$(BUILD_DIR),$(SRCS:.c=.o))

INC_DIRS += $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
XCFLAGS += $(CFLAGS) $(INC_FLAGS)

# Final conversions
DEPS += $(OBJS:.o=.d)

MKDIR_P ?= @mkdir -p
TARGET ?= linux
$(info TARGET [$(TARGET)])

# defaults for target arm
ifeq ($(TARGET),arm)
#CC := arm-rdk-linux-gnueabi-gcc -mthumb -mfpu=vfp -mcpu=cortex-a9 -mfloat-abi=soft -mabi=aapcs-linux -mno-thumb-interwork -ffixed-r8 -fomit-frame-pointer
# CFLAGS will be overriden by Caller as required
INC_DIRS += $(UT_DIR)/sysroot/usr/include
endif

# Defaults for target linux
ifeq ($(TARGET),linux)
CC := gcc -ggdb -o0 -Wall
endif

.PHONY: clean list lib test all

# Rule to create the shared and static library
lib : ${OBJS}
	@echo -e ${GREEN}Building static lib [${YELLOW}$(LIB_DIR)/$(TARGET_LIB)${GREEN}]${NC}
	@$(MKDIR_P) $(LIB_DIR)
	@$(CC) $(CFLAGS) -o $(LIB_DIR)/$(TARGET_LIB) $^ $(LDFLAGS)

# Make any c source
$(BUILD_DIR)/%.o: %.c
	@echo -e ${GREEN}Building [${YELLOW}$<${GREEN}]${NC}
	@$(MKDIR_P) $(dir $@)
	@$(CC) $(XCFLAGS) -c $< -o $@

.PHONY: clean list framework lib

all: framework lib

# Ensure the framework is built
framework:
	@echo -e ${GREEN}"Ensure framework is present"${NC}
	${UT_CONTROL_DIR}/configure.sh
	@echo -e ${GREEN}Completed${NC}

list:
	@echo ${GREEN}List [$@]${NC}
	@echo INC_DIRS: ${INC_DIRS}
	@echo SRC_DIRS: ${SRC_DIRS}
	@echo CFLAGS: ${CFLAGS}
	@echo LDFLAGS: ${LDFLAGS}
	@echo TARGET_LIB: ${TARGET_LIB}

clean:
	@echo -e ${GREEN}Performing Clean${NC}
	@$(RM) -rf $(BUILD_DIR)
	@echo -e ${GREEN}Clean Completed${NC}

cleanall: clean
	@echo -e ${GREEN}Performing Clean on frameworks [$(TOP_DIR)/framework]${NC}
	@${RM} -rf $(TOP_DIR)/framework
