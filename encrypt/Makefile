CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -Iinclude -I../compress/utils
LDFLAGS = -pthread
SRCS = main.c src/key.c src/aes.c src/file_ops.c
THREADPOOL_SRC = ../compress/utils/threadpool.c
THREADPOOL_OBJ = build/shared/threadpool.o
OBJS = $(SRCS:%.c=build/%.o) $(THREADPOOL_OBJ)
TARGET = aes128

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(THREADPOOL_OBJ): $(THREADPOOL_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)