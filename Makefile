CC = gcc
JPEG_CFLAGS := $(shell pkg-config --cflags libjpeg 2>/dev/null)
JPEG_LIBS := $(shell pkg-config --libs libjpeg 2>/dev/null)

ifeq ($(strip $(JPEG_LIBS)),)
$(error libjpeg development files are required; install libjpeg-turbo)
endif

CPPFLAGS = -Iinclude $(JPEG_CFLAGS)

CFLAGS = -O2 -Wall -Wextra -Werror
DEBUG_CFLAGS = -g -O0 -Wall -Wextra -Werror

LDFLAGS =
LDLIBS = -lraylib -lm -ldl -lpthread -lrt $(JPEG_LIBS)

SRC = src/main.c src/app.c src/apply.c src/fs.c src/jpeg_loader.c src/wallpaper.c src/ui.c src/render.c
TARGET = wallpicker

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: clean $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

debug-install: debug
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET)

reinstall: clean uninstall install

.PHONY: all debug install debug-install uninstall clean reinstall
