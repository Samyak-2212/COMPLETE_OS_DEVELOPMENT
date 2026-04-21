# userspace/init/GNUmakefile

SRCS = init.c
OBJS = $(patsubst %.c, ../build/init_%.o, $(SRCS))
TARGET = ../build/init

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) ../build/libc/crt0.o ../build/libcnexus.a
	$(LD) $(LDFLAGS) -Ttext=0x400000 ../build/libc/crt0.o $(OBJS) ../build/libcnexus.a -o $@

../build/init_%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f ../build/init_*.o $(TARGET)
