#
## sample Makefile for boot-animation
#
#
OBJ = boot-animation.o
CFLAGS = -Wall -Wextra -fpermissive -g
LDFLAGS = -L ./
TARGET=boot-animation

export PKG_CONFIG_PATH=$(TARGET_DIR)/../host/$(CROSSCOMPILE)/sysroot/usr/lib/pkgconfig
export PKG_CONFIG=$(TARGET_DIR)/../host/bin/pkg-config
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs gstreamer-1.0)
#LDFLAGS += -L$(TARGET_OUTPUT_DIR)/host/$(CROSSCOMPILE)/sysroot/usr/lib/gstreamer-1.0 -lgstamlvsink -lgstamlvdec

# rules
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $^ $(LDFLAGS) -o $@ 

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@ 

.PHONY: clean

clean:
	rm -f $(OBJ)

install:
	cp boot-animation $(DESTDIR)/bin/boot-animation
