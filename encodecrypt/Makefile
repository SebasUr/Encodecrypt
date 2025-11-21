CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread \
         -I. \
         -I../compress \
         -I../compress/src \
         -I../compress/utils \
         -I../encrypt/include \
         -I../encrypt/src
LDFLAGS = -pthread
TARGET = encodecrypt

COMPRESS_SRCS = \
    ../compress/src/core/compress.c \
    ../compress/src/core/decompress.c \
    ../compress/src/huffman/huffman.c \
    ../compress/src/archive/archive.c \
    ../compress/utils/bitreader.c \
    ../compress/utils/bitwriter.c \
    ../compress/utils/decompress_utils.c \
    ../compress/utils/file_utils.c \
    ../compress/utils/threadpool.c

ENCRYPT_SRCS = \
    ../encrypt/src/key.c \
    ../encrypt/src/aes.c \
    ../encrypt/src/file_ops.c

SRCS = main.c $(COMPRESS_SRCS) $(ENCRYPT_SRCS)
OBJS = $(SRCS:%.c=build/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)
