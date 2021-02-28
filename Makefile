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
SRC += $(wildcard $(SRC_DIR)/*.c) 
SRC += $(wildcard $(SRC_DIR)/protocol/*.c) 

SRC += $(wildcard $(SRC_DIR)/socket/*.c) 
SRC += $(wildcard $(SRC_DIR)/socket/TCP/*.c) 
SRC += $(wildcard $(SRC_DIR)/socket/UDP/*.c) 

SRC += $(wildcard $(SRC_DIR)/dataSource/video/*.c) 
SRC += $(wildcard $(SRC_DIR)/dataSource/encoder/*.c) 
SRC += $(wildcard $(SRC_DIR)/dataSource/audio/*.c) 

OBJ = $(patsubst %.c, %.o, $(SRC))

CFLAG = -I./include -Wall -g
DFLAG = 
$(TARGET):$(OBJ)
	$(CC) -o $@ $^

%.o:%.c
	$(CC) $(CFLAG) -c -o $@ $<

.PHONY:
clean:
	rm -rf $(TARGET) $(OBJ)