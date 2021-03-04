CROSS_COMPILE :=
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
LD = $(CROSS_COMPILE)ld
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

TARGET = videoServer

SRC_DIR := $(PWD)/src
DIRS = $(shell find $(SRC_DIR) -maxdepth 3 -type d)
SRC += $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJ = $(patsubst %.c, %.o, $(SRC))

CFLAG = -I./include -Wall -g
DFLAG = -lasound
$(TARGET):$(OBJ)
	$(CC) -o $@ $^ $(DFLAG)

%.o:%.c
	$(CC) $(CFLAG) -c -o $@ $<

.PHONY:
clean:
	rm -rf $(TARGET) $(OBJ)