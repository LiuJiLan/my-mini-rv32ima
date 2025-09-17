CC = gcc
CFLAGS = -Wall -O2 -Iinclude -Ishell -Icore

SRC = shell/shell.c core/old_core.c
OBJ = $(SRC:%.c=build/%.o)

TARGET = build/shell.elf

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lm

build/%.o: %.c | build
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build
