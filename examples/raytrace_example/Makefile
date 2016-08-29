# Makefile template from https://spin.atomicobject.com/2016/08/26/makefile-c-projects/

TARGET_EXEC ?= raytrace_example

BUILD_DIR ?= ./build
SRC_DIRS ?= .

SRCS := X11SWWindow.cpp main.cpp imgui_impl_raytrace.cpp ../../imgui.cpp ../../imgui_draw.cpp
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS)) -I../../

CPPFLAGS ?= $(INC_FLAGS) -std=c++11 -O3 -MMD -MP

LDFLAGS += -L/usr/X11/lib -lX11

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
		$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# assembly
$(BUILD_DIR)/%.s.o: %.s
		$(MKDIR_P) $(dir $@)
			$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
		$(MKDIR_P) $(dir $@)
			$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
		$(MKDIR_P) $(dir $@)
			$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


.PHONY: clean

clean:
		$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p

