# userspace/shell/GNUmakefile

SRCS = shell.c builtins.c
OBJS = $(patsubst %.c, ../build/shell_%.o, $(SRCS))
TARGET = ../build/nsh

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) ../build/libc/crt0.o ../build/libcnexus.a
	$(LD) $(LDFLAGS) -Ttext=0x400000 ../build/libc/crt0.o $(OBJS) ../build/libcnexus.a -o $@

../build/shell_%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f ../build/shell_*.o $(TARGET)
