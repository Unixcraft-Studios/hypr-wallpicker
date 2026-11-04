CC ?= cc
CMAKE ?= cmake

TARGET := wallpicker

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

RAYLIB_DIR := vendor/raylib
RAYLIB_BUILD := $(RAYLIB_DIR)/build
RAYLIB_LIB := $(RAYLIB_BUILD)/raylib/libraylib.a

SRC := \
	src/main.c \
	src/app.c \
	src/apply.c \
	src/fs.c \
	src/wallpaper.c \
	src/ui.c \
	src/render.c

HEADERS := $(wildcard include/*.h)
OBJ := $(SRC:.c=.o)

CPPFLAGS := -Iinclude -I$(RAYLIB_DIR)/src
CFLAGS := -O2 -Wall -Wextra -Werror
DEBUG_CFLAGS := -O0 -g3 -Wall -Wextra -Werror

LDFLAGS :=
LDLIBS := -lm -ldl -lpthread -lrt -lX11

.PHONY: all debug clean clean-all install uninstall reinstall raylib

all: $(TARGET)

$(RAYLIB_LIB):
	$(CMAKE) -S $(RAYLIB_DIR) -B $(RAYLIB_BUILD) \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.10 \
		-DBUILD_EXAMPLES=OFF \
		-DCUSTOMIZE_BUILD=ON \
		-DSUPPORT_FILEFORMAT_JPG=ON \
		-DSUPPORT_MODULE_RAUDIO=ON \
		-DSUPPORT_FILEFORMAT_MOD=OFF \
		-DSUPPORT_FILEFORMAT_XM=OFF
	$(CMAKE) --build $(RAYLIB_BUILD) --parallel

raylib: $(RAYLIB_LIB)

%.o: %.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ) $(RAYLIB_LIB)
	$(CC) $(OBJ) $(RAYLIB_LIB) -o $@ $(LDFLAGS) $(LDLIBS)

debug: CFLAGS := $(DEBUG_CFLAGS)
debug: clean all

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

reinstall: uninstall install

clean:
	rm -f $(OBJ) $(TARGET)

clean-all: clean
	rm -rf $(RAYLIB_BUILD)

