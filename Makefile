CROSS_COMPILE ?=
KDIR ?=

KINC := -I$(KDIR)/usr/include
CC   := $(CROSS_COMPILE)gcc

CFLAGS += -O2 -Wall -fpic -I. $(KINC)
OBJS = media.o main.o options.o subdev.o

all: media-ctl

media-ctl: $(OBJS)
	$(CC) $(CFLAGS) -o media-ctl $(OBJS)
	$(CROSS_COMPILE)strip media-ctl
clean:
	rm -f $(OBJS) media-ctl

.PHONY: clean all
