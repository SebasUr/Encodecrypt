CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
SRCS = main.c src/key.c src/aes.c src/file_ops.c
OBJS = $(SRCS:%.c=build/%.o)
TARGET = aes128

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)