# userspace/libc/GNUmakefile

SRCS_C = $(wildcard src/*.c)
SRCS_A = $(wildcard src/*.asm)

OBJS = $(patsubst src/%.c, ../build/libc/%.o, $(SRCS_C)) \
       $(patsubst src/%.asm, ../build/libc/%.o, $(SRCS_A))

TARGET = ../build/libcnexus.a

.PHONY: all clean setup

all: setup $(TARGET)

setup:
	@mkdir -p ../build/libc

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

../build/libc/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

../build/libc/%.o: src/%.asm
	$(AS) -f elf64 $< -o $@

clean:
	rm -rf ../build/libc $(TARGET)
