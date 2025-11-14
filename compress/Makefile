CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2 -I. -Isrc -Iutils
LDFLAGS = -lpthread
BUILDDIR = build
BINDIR = bin
TARGET = $(BINDIR)/huffman

# Archivos fuente
SOURCES = main.c \
          src/core/compress.c \
          src/core/decompress.c \
          src/huffman/huffman.c \
          src/archive/archive.c \
          utils/bitreader.c \
          utils/bitwriter.c \
          utils/decompress_utils.c \
          utils/file_utils.c \
          utils/threadpool.c

# Generar nombres de objetos en build/
OBJECTS = $(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(SOURCES)))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Regla para main.c
$(BUILDDIR)/main.o: main.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Reglas para archivos en src/core/
$(BUILDDIR)/compress.o: src/core/compress.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/decompress.o: src/core/decompress.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Reglas para archivos en src/huffman/
$(BUILDDIR)/huffman.o: src/huffman/huffman.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Reglas para archivos en src/archive/
$(BUILDDIR)/archive.o: src/archive/archive.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Reglas para archivos en utils/
$(BUILDDIR)/bitreader.o: utils/bitreader.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/bitwriter.o: utils/bitwriter.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/decompress_utils.o: utils/decompress_utils.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/file_utils.o: utils/file_utils.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/threadpool.o: utils/threadpool.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Crear directorios si no existen
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(BUILDDIR) $(BINDIR) utils/*.o

run: $(TARGET)
	$(TARGET)