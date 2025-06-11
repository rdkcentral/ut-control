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

TARGET_LIB = libut_control.so

RED:='\033[0;31m'
GREEN:='\033[0;32m'
YELLOW:='\033[0;33m'
NC:='\033[0m'
ECHOE = /bin/echo -e

$(info $(shell $(ECHOE) ${GREEN}Building ${YELLOW}[$(TARGET_LIB)]${NC}))

UT_CONTROL_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

SRC_DIR =
INC_DIR =

export PATH := $(shell pwd)/toolchain:$(PATH)

TOP_DIR = $(UT_CONTROL_DIR)

# TARGET must be supplied by the user
ifeq ($(TARGET),)
$(error "TARGET is not set, exiting")
endif

FRAMEWORK_SRC_DIR = $(TOP_DIR)/framework/$(TARGET)
FRAMEWORK_BUILD_DIR = $(TOP_DIR)/build/$(TARGET)
LIB_DIR := $(FRAMEWORK_BUILD_DIR)/lib
BUILD_DIR = $(FRAMEWORK_BUILD_DIR)/obj
BIN_DIR = $(TOP_DIR)/build/bin
BUILD_LIBS = yes

# Enable libyaml Requirements
LIBFYAML_DIR = $(FRAMEWORK_SRC_DIR)/libfyaml-master
ASPRINTF_DIR = $(FRAMEWORK_SRC_DIR)/asprintf/asprintf.c-master/
SRC_DIRS = $(LIBFYAML_DIR)/src/lib
SRC_DIRS += $(LIBFYAML_DIR)/src/thread
SRC_DIRS += $(LIBFYAML_DIR)/src/util
SRC_DIRS += $(LIBFYAML_DIR)/src/xxhash
SRC_DIRS += $(ASPRINTF_DIR)
INC_DIRS = $(LIBFYAML_DIR)/include
INC_DIRS += $(ASPRINTF_DIR)

# LIBFYAML Requirements
XLDFLAGS += -pthread

# LIBWEBSOCKETS Requirements
LIBWEBSOCKETS_DIR = $(FRAMEWORK_BUILD_DIR)/libwebsockets
INC_DIRS += $(LIBWEBSOCKETS_DIR)/include
XLDFLAGS += $(LIBWEBSOCKETS_DIR)/lib/libwebsockets.a

# CURL Requirements
CURL_DIR = $(FRAMEWORK_BUILD_DIR)/curl
ifneq ($(wildcard $(CURL_DIR)),)
INC_DIRS += $(CURL_DIR)/include
endif
XLDFLAGS += -ldl

# UT Control library Requirements
SRC_DIRS += ${TOP_DIR}/src
INC_DIRS += ${TOP_DIR}/include

XCFLAGS += -fPIC -Wall -shared   # Flags for compilation
XCFLAGS += -DNDEBUG
# CFLAGS += -DWEBSOCKET_SERVER

MKDIR_P ?= @mkdir -p
TARGET ?= linux
$(info TARGET [$(TARGET)])

OPENSSL_LIB_DIR = $(FRAMEWORK_BUILD_DIR)/openssl/lib/

# defaults for target arm
ifeq ($(TARGET),arm)
#CC := arm-rdk-linux-gnueabi-gcc -mthumb -mfpu=vfp -mcpu=cortex-a9 -mfloat-abi=soft -mabi=aapcs-linux -mno-thumb-interwork -ffixed-r8 -fomit-frame-pointer
# CFLAGS will be overriden by Caller as required
INC_DIRS += $(UT_DIR)/sysroot/usr/include
XLDFLAGS += -ldl
else
#linux case
# Check if the directory exists
ifneq ($(wildcard $(OPENSSL_LIB_DIR)),)
XLDFLAGS += -ldl
else
# Commands to run if the directory does not exist
XLDFLAGS += -lssl -lcrypto
endif
endif

# Defaults for target linux
ifeq ($(TARGET),linux)
CC := gcc -ggdb -o0 -Wall
endif

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)

#OBJS := $(SRCS:.c=.o)
OBJS := $(subst $(TOP_DIR),$(BUILD_DIR),$(SRCS:.c=.o))

INC_DIRS += $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
XCFLAGS += $(CFLAGS) $(INC_FLAGS)

# Final conversions
DEPS += $(OBJS:.o=.d)

.PHONY: clean list lib test all framework

all: framework

# Rule to create the shared library
lib : ${OBJS}
	@$(ECHOE) ${GREEN}Generating lib [${YELLOW}$(LIB_DIR)/$(TARGET_LIB)${GREEN}]${NC}
	@$(MKDIR_P) $(LIB_DIR)
	@$(CC) $(XCFLAGS) -o $(LIB_DIR)/$(TARGET_LIB) $^ $(XLDFLAGS)

# Make any c source
$(BUILD_DIR)/%.o: %.c
	@$(ECHOE) ${GREEN}Building [${YELLOW}$<${GREEN}]${NC}
	@$(MKDIR_P) $(dir $@)
	@$(CC) $(XCFLAGS) -c $< -o $@

# Ensure the framework is built
framework:
	@$(ECHOE) ${GREEN}"Ensure ut-control framework is present"${NC}
	@${UT_CONTROL_DIR}/configure.sh $(TARGET)
	make lib

list:
	@$(ECHOE) ${GREEN}List [$@]${NC}
	@$(ECHOE) ${YELLOW}INC_DIRS:${NC} ${INC_DIRS}
	@$(ECHOE) ${YELLOW}SRC_DIRS:${NC} ${SRC_DIRS}
	@$(ECHOE) ${YELLOW}CFLAGS:${NC} ${CFLAGS}
	@$(ECHOE) ${YELLOW}XLDFLAGS:${NC} ${XLDFLAGS}
	@$(ECHOE) ${YELLOW}XCFLAGS:${NC} ${XCFLAGS}
	@$(ECHOE) ${YELLOW}TARGET_LIB:${NC} ${TARGET_LIB}

clean:
	@$(ECHOE) ${GREEN}Performing Clean for $(TARGET)${NC}
	@$(ECHOE) ${GREEN}rm -rf [${YELLOW}$(BUILD_DIR)${GREEN}]${NC}
	@$(RM) -rf $(BUILD_DIR)
	@$(ECHOE) ${GREEN}rm -rf [${YELLOW}${TOP_DIR}/*.txt${GREEN}]${NC}
	@$(RM) -rf ${TOP_DIR}/*.txt
	@${ECHOE} ${GREEN}rm -fr [${YELLOW}$(LIB_DIR)/$(TARGET_LIB)${GREEN}]${NC}
	@$(RM) -fr $(LIB_DIR)/$(TARGET_LIB)
	@$(ECHOE) ${GREEN}Clean Completed${NC}

cleanall: clean
	@$(ECHOE) ${GREEN}Performing Clean on frameworks [$(TOP_DIR)/framework] and build [$(TOP_DIR)/build/]${NC}
	@${RM} -rf $(TOP_DIR)/framework
	@${RM} -rf $(TOP_DIR)/build/

cleanhost-tools:
	@$(ECHOE) ${GREEN}Performing Clean on host-tools [$(TOP_DIR)/host-tools]${NC}
	@${RM} -rf $(TOP_DIR)/host-tools

printenv:
	@$(ECHOE) "Environment variables: [UT]"
	@$(ECHOE) "---------------------------"
	@$(foreach v, $(.VARIABLES), \
		$(info $(v) = $($(v)))))
	@$(ECHOE) "---------------------------"
