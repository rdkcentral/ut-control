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

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

$(info $(shell echo -e ${GREEN}Building [$(TARGET_LIB)]${NC}))

UT_CONTROL_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

SRC_DIR =
INC_DIR =

export PATH := $(shell pwd)/toolchain:$(PATH)

TOP_DIR = $(UT_CONTROL_DIR)

ifneq ($(TARGET),arm)
TARGET=linux
endif
export TARGET

LIB_DIR := $(TOP_DIR)/build/$(TARGET)/lib
BUILD_DIR = $(TOP_DIR)/build/$(TARGET)/obj
BIN_DIR = $(TOP_DIR)/build/bin
BUILD_LIBS = yes

# Enable libyaml Requirements
LIBFYAML_DIR = ${TOP_DIR}/framework/$(TARGET)/libfyaml-master
ASPRINTF_DIR = ${TOP_DIR}/framework/$(TARGET)/asprintf/asprintf.c-master/
SRC_DIRS = $(LIBFYAML_DIR)/src/lib
SRC_DIRS += $(LIBFYAML_DIR)/src/thread
SRC_DIRS += $(LIBFYAML_DIR)/src/util
SRC_DIRS += $(LIBFYAML_DIR)/src/xxhash
SRC_DIRS += $(ASPRINTF_DIR)
INC_DIRS = $(LIBFYAML_DIR)/include
INC_DIRS += $(ASPRINTF_DIR)

# LIBWEBSOCKETS Requirements
LIBWEBSOCKETS_DIR = $(TOP_DIR)/build/$(TARGET)/libwebsockets
INC_DIRS += $(LIBWEBSOCKETS_DIR)/include
XLDFLAGS += $(LIBWEBSOCKETS_DIR)/lib/libwebsockets.a

# CURL Requirements
CURL_DIR = $(TOP_DIR)/build/$(TARGET)/curl
ifneq ($(wildcard $(CURL_DIR)),)
INC_DIRS += $(CURL_DIR)/include
XLDFLAGS += $(CURL_DIR)/lib/libcurl.a
else
# Commands to run if the directory does not exist
XLDFLAGS += -lcurl
endif

# UT Control library Requirements
SRC_DIRS += ${TOP_DIR}/src
INC_DIRS += ${TOP_DIR}/include

CFLAGS += -fPIC -Wall -shared   # Flags for compilation
CFLAGS += -DNDEBUG
# CFLAGS += -DWEBSOCKET_SERVER

MKDIR_P ?= @mkdir -p
TARGET ?= linux
$(info TARGET [$(TARGET)])

OPENSSL_LIB_DIR = $(TOP_DIR)/build/$(TARGET)/openssl/lib/

# defaults for target arm
ifeq ($(TARGET),arm)
#CC := arm-rdk-linux-gnueabi-gcc -mthumb -mfpu=vfp -mcpu=cortex-a9 -mfloat-abi=soft -mabi=aapcs-linux -mno-thumb-interwork -ffixed-r8 -fomit-frame-pointer
# CFLAGS will be overriden by Caller as required
INC_DIRS += $(UT_DIR)/sysroot/usr/include
XLDFLAGS += $(OPENSSL_LIB_DIR)/libssl.a $(OPENSSL_LIB_DIR)/libcrypto.a -ldl
else
#linux case
# Check if the directory exists
ifneq ($(wildcard $(OPENSSL_LIB_DIR)),)
XLDFLAGS += $(OPENSSL_LIB_DIR)/libssl.a $(OPENSSL_LIB_DIR)/libcrypto.a -ldl
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
	@echo -e ${GREEN}Generating lib [${YELLOW}$(LIB_DIR)/$(TARGET_LIB)${GREEN}]${NC}
	@$(MKDIR_P) $(LIB_DIR)
	@$(CC) $(CFLAGS) -o $(LIB_DIR)/$(TARGET_LIB) $^ $(XLDFLAGS)

# Make any c source
$(BUILD_DIR)/%.o: %.c
	@echo -e ${GREEN}Building [${YELLOW}$<${GREEN}]${NC}
	@$(MKDIR_P) $(dir $@)
	@$(CC) $(XCFLAGS) -c $< -o $@

# Ensure the framework is built
framework:
	@echo -e ${GREEN}"Ensure ut-control framework is present"${NC}
	@${UT_CONTROL_DIR}/configure.sh TARGET=$(TARGET)
	make lib

list:
	@echo ${GREEN}List [$@]${NC}
	@echo -e ${YELLOW}INC_DIRS:${NC} ${INC_DIRS}
	@echo -e ${YELLOW}SRC_DIRS:${NC} ${SRC_DIRS}
	@echo -e ${YELLOW}CFLAGS:${NC} ${CFLAGS}
	@echo -e ${YELLOW}XLDFLAGS:${NC} ${XLDFLAGS}
	@echo -e ${YELLOW}TARGET_LIB:${NC} ${TARGET_LIB}

clean:
	@echo -e ${GREEN}Performing Clean for $(TARGET)${NC}
	@$(RM) -rf $(BUILD_DIR)
	@$(RM) -rf ${TOP_DIR}/*.txt
	@echo -e ${GREEN}Clean Completed${NC}

cleanall: clean
	@echo -e ${GREEN}Performing Clean on frameworks [$(TOP_DIR)/framework]${NC}
	@${RM} -rf $(TOP_DIR)/framework

cleanhost-tools:
	@echo -e ${GREEN}Performing Clean on host-tools [$(TOP_DIR)/host-tools]${NC}
	@${RM} -rf $(TOP_DIR)/host-tools
