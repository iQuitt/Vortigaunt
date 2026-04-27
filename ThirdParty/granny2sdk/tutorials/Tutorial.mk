# Simple makefile to build granny tutorials on linux.  Note that no real
# dependency scanning is done for these targets, and for tutorials, we assume
# that only debug mode will be used.

TUTORIALS_DIR ?= ..
CFLAGS         = -Wall -DLINUX -DRAD_NO_LOWERCASE_TYPES -DBUILDING_GRANNY_STATIC -fno-exceptions -D_GNU_SOURCE
INCLUDE_DIRS   = -I$(TUTORIALS_DIR)/../include -I$(TUTORIALS_DIR)/shared_code
DEBUG_CFLAGS   = -g -DDEBUG=1 -D_DEBUG
CFLAGS_FOR_32  = -m32
CFLAGS_FOR_64  = -m64

LINK_LIBS_32 += $(TUTORIALS_DIR)/../lib/linux/granny2_debug.a -lpthread
LINK_LIBS_64 += $(TUTORIALS_DIR)/../lib/linux/granny2_x64_debug.a -lpthread

GRANNY_SRC_CPP = $(wildcard *.cpp)
GRANNY_OBJS    = $(GRANNY_SRC_CPP:.cpp=.o)

DEBUG_OBJDIR     = objs/debug
DEBUG64_OBJDIR   = objs/debug64
DEBUG_OBJDIRS   = $(DEBUG_OBJDIR) $(DEBUG64_OBJDIR)

GRANNY_DBG_OBJS   = $(addprefix $(DEBUG_OBJDIR)/,$(GRANNY_OBJS))
GRANNY_DBG64_OBJS = $(addprefix $(DEBUG64_OBJDIR)/,$(GRANNY_OBJS))

CC  = gcc
C++ = g++
LIB = ar

.PHONY: clean debug release debug32 debug64

all: debug32

debug32: $(DEBUG_OBJDIRS) $(TARGET_NAME)_32

debug64: $(DEBUG_OBJDIRS) $(TARGET_NAME)_64

run32: debug32
	@cd .. && $(TARGET_NAME)/$(TARGET_NAME)_32

run64: debug64
	@cd .. && $(TARGET_NAME)/$(TARGET_NAME)_64

$(TARGET_NAME)_32: $(GRANNY_DBG_OBJS)
	@$(C++) $(CFLAGS_FOR_32) -o $@ $^ $(LINK_LIBS_32)
	@echo Built $@

$(TARGET_NAME)_64: $(GRANNY_DBG64_OBJS)
	@$(C++) $(CFLAGS_FOR_64) -o $@ $^ $(LINK_LIBS_64)
	@echo Built $@

clean:
	rm -rf objs
	rm -f $(TARGET_NAME)_32
	rm -f $(TARGET_NAME)_64

$(DEBUG_OBJDIRS):
	@mkdir -p $(DEBUG_OBJDIRS)

$(DEBUG_OBJDIR)/%.o: %.cpp
	@echo $< ... debug
	@$(C++) -c $(INCLUDE_DIRS) $(CFLAGS) $(CFLAGS_FOR_32) $(DEBUG_CFLAGS) $< -o $@

$(DEBUG64_OBJDIR)/%.o: %.cpp
	@echo $< ... debug64
	@$(C++) -c $(INCLUDE_DIRS) $(CFLAGS) $(CFLAGS_FOR_64) $(DEBUG_CFLAGS) $< -o $@

