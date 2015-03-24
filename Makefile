## @file Makefile
#
# Copyright(C) 2014 大前良介(OHMAE Ryosuke)
#
# This software is released under the MIT License.
# http://opensource.org/licenses/MIT
#
# @breaf
# @author <a href="mailto:ryo@mm2d.net">大前良介(OHMAE Ryosuke)</a>
# @date 2015/02/08

CC   = gcc
RM   = rm -rf
MAKE = make

CFLAGS = -Wall -g3 -O2
COPTS  = -D_DEBUG_ 
LDFLAGS = -lpng -ljpeg -lz

MODULE = image
OBJ_DIR = obj
DEPEND_FILE = $(MODULE).dpd
SRCS = $(wildcard *.c)
OBJS = $(addprefix $(OBJ_DIR)/, $(SRCS:%.c=%.o))

.PHONY: all clean
all: $(MODULE)

$(MODULE): $(OBJ_DIR) $(OBJS)
	$(CC) -o $(MODULE) $(OBJS) $(CFLAGS) $(LDFLAGS)

clean:
	$(RM) $(MODULE)
	$(RM) $(DEPEND_FILE)
	$(RM) $(OBJ_DIR)

$(OBJS): Makefile 

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $(COPTS) -c $< -o $@

.PHONY: depend
depend: $(DEPEND_FILE)
$(DEPEND_FILE):
	$(CC) -MM $(SRCS) | sed -e 's/^/$(OBJ_DIR)\//g' > $(DEPEND_FILE)

-include $(DEPEND_FILE)
