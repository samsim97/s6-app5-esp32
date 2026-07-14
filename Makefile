# Makefile wrapper around idf.py for build / flash / debug
# Usage examples:
#   make build
#   make flash PORT=/dev/ttyUSB0
#   make monitor PORT=/dev/ttyUSB0
#   make debug

PORT     ?= /dev/ttyUSB0
BAUD     ?= 460800
TARGET   ?= esp32
BUILDDIR ?= build

IDF      ?= idf.py
IDF_ARGS ?= -B $(BUILDDIR)

.PHONY: all build app bootloader partition-table clean fullclean \
        flash app-flash bootloader-flash erase monitor \
        debug openocd gdb gdbgui set-target menuconfig size help

all: build

build:
	$(IDF) $(IDF_ARGS) build

app:
	$(IDF) $(IDF_ARGS) app

bootloader:
	$(IDF) $(IDF_ARGS) bootloader

partition-table:
	$(IDF) $(IDF_ARGS) partition-table

set-target:
	$(IDF) $(IDF_ARGS) set-target $(TARGET)

menuconfig:
	$(IDF) $(IDF_ARGS) menuconfig

size:
	$(IDF) $(IDF_ARGS) size

clean:
	$(IDF) $(IDF_ARGS) clean

fullclean:
	$(IDF) $(IDF_ARGS) fullclean

flash:
	$(IDF) $(IDF_ARGS) -p $(PORT) -b $(BAUD) flash

app-flash:
	$(IDF) $(IDF_ARGS) -p $(PORT) -b $(BAUD) app-flash

bootloader-flash:
	$(IDF) $(IDF_ARGS) -p $(PORT) -b $(BAUD) bootloader-flash

erase:
	$(IDF) $(IDF_ARGS) -p $(PORT) erase-flash

monitor:
	$(IDF) $(IDF_ARGS) -p $(PORT) monitor

flash-monitor:
	$(IDF) $(IDF_ARGS) -p $(PORT) -b $(BAUD) flash monitor

openocd:
	$(IDF) $(IDF_ARGS) openocd

gdb:
	$(IDF) $(IDF_ARGS) gdb

gdbgui:
	$(IDF) $(IDF_ARGS) gdbgui

debug:
	$(IDF) $(IDF_ARGS) openocd gdbgui

help:
	@echo "Targets:"
	@echo "  build              - Compile project (idf.py build)"
	@echo "  app/bootloader/partition-table - Build single component"
	@echo "  set-target         - Set chip target (TARGET=esp32|esp32s3|...)"
	@echo "  menuconfig         - Open project configuration"
	@echo "  size               - Print firmware size summary"
	@echo "  clean / fullclean  - Clean build outputs"
	@echo "  flash              - Flash full image (PORT=, BAUD=)"
	@echo "  app-flash          - Flash app partition only"
	@echo "  bootloader-flash   - Flash bootloader only"
	@echo "  erase              - Erase entire flash"
	@echo "  monitor            - Open serial monitor"
	@echo "  flash-monitor      - Flash then start monitor"
	@echo "  openocd            - Start OpenOCD JTAG server"
	@echo "  gdb                - Start GDB connected to target"
	@echo "  gdbgui             - Start GDB with web UI"
	@echo "  debug              - Run OpenOCD + gdbgui together"
