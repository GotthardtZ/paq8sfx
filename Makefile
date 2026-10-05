# paq8sfx Makefile

TARGET     = paq8sfx
STUB       = paq8sfx.stub
CC         = gcc
CXX        = g++
CFLAGS     = -O2 -Wall
CXXFLAGS   = -std=gnu++17 -fno-fast-math -ffp-contract=off -flto=auto -s -m64 -march=nocona -mtune=generic -Wl,--gc-sections -fno-exceptions -fno-rtti -DNDEBUG
LIBS       =
LD         = $(CXX)

PAQ8SFX_FLAGS := $(CXXFLAGS) -DFULL -O3
STUB_FLAGS    := $(CXXFLAGS) -DSFX -Os

SRCS := $(wildcard src/file/*.cpp src/model/*.cpp src/*.cpp)

TARGET_OBJS := $(SRCS:.cpp=.o)
STUB_OBJS := $(SRCS:.cpp=.o)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.cpp.o:
	$(CXX) $(CXXFLAGS) $(TARGET_FLAGS) -c -o $@ $<

$(TARGET_OBJS): %.o : %.cpp
	$(CXX) $(CXXFLAGS) $(PAQ8SFX_FLAGS) $< -c -o $@

$(STUB_OBJS): %.o : %.cpp
	$(CXX) $(CXXFLAGS) $(STUB_FLAGS) $< -c -o $@


all: $(TARGET)
	@ECHO Every target has to be made separately, with preceding clean up
	@ECHO Available targets: $(TARGET) $(STUB)

$(TARGET): $(TARGET_OBJS)
	$(LD) -o $@ $^ $(LIBS)

$(STUB): $(STUB_OBJS)
	$(LD) -o $@ $^ $(LIBS)


.PHONY: clean

clean:
	rm -rf $(TARGET) $(STUB) $(TARGET_OBJS) $(STUB_OBJS)

print-%:
	@echo '$*=$($*)'
