#
## sample Makefile for boot-animation
#
#
TARGET=boot-animation boot-animation-player
OBJ1=boot_animation.o signal_proc.o
OBJ2=boot_animation_player.o media_player.o
OBJ_COM=signal_proc.o

CFLAGS = -Wall -Wextra -fpermissive
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs gstreamer-1.0)
LDFLAGS += -lpthread

# rules
all: boot-animation boot-animation-player

boot-animation: $(OBJ1) $(OBJ_COM)
	$(CC) $^ $(LDFLAGS) -o $@

boot-animation-player: $(OBJ2) $(OBJ_COM)
	$(CC) $^ $(LDFLAGS) -o $@

$(OBJ1):%.o:%.c
	$(CC) -c $(CFLAGS) $^ -o $@

$(OBJ2):%.o:%.c
	$(CC) -c $(CFLAGS) $^ -o $@

$(OBJ_COM):%.o:%.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -f $(OBJ1)
	rm -f $(OBJ2)

install:
	cp boot-animation $(DESTDIR)/bin/boot-animation
	cp boot-animation-player $(DESTDIR)/bin/boot-animation-player
