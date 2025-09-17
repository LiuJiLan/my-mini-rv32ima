CC = gcc
CXX = g++
CFLAGS = -Wall -O2 -Iinclude -Ishell -Icore
CXXFLAGS = $(CFLAGS)   # 暂时相同

SRC_C = shell/shell.c core/old_core.c
SRC_CPP = core/my_core.cpp

OBJ_C = $(SRC_C:%.c=build/%.o)
OBJ_CPP = $(SRC_CPP:%.cpp=build/%.o)

OBJ = $(OBJ_C) $(OBJ_CPP)

TARGET = build/shell.elf

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) -lm

build/%.o: %.c | build
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.cpp | build
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build
