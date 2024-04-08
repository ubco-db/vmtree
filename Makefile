CC=gcc
CFLAGS=-g
TARGET=test_vmtree
SRC_DIR=src
SRCS=$(SRC_DIR)/vmtree.c $(SRC_DIR)/dbbuffer.c $(SRC_DIR)/bitarr.c $(SRC_DIR)/filestorage.c $(SRC_DIR)/in_memory_sort.c $(SRC_DIR)/main_pc.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
