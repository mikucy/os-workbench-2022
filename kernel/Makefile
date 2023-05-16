NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       := include/ framework/

export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := x86_64-qemu
endif

include $(AM_HOME)/Makefile
include ../Makefile.lab
image: git

BUILD_FLAGS = -Iframework -Itest -DTEST -lpthread -g
VERBOSE = 0
ifeq ($(VERBOSE),1)
    BUILD_FLAGS += -DVERBOSE
endif

test: git
	@gcc src/pmm.c  \
	$(shell find test/ -name "*.c") \
	$(BUILD_FLAGS) \
	-o build/test

testall: test
	@build/test 0
	@build/test 1