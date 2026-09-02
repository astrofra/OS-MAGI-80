SHELL := /bin/bash

MAGI80_TOOLCHAIN_PREFIX ?= $(HOME)/.local/m68k-amigaos
MAGI80_PIPX_BIN ?= $(HOME)/.local/bin

TARGET_CC := $(MAGI80_TOOLCHAIN_PREFIX)/bin/m68k-amigaos-gcc
TARGET_SIZE := $(MAGI80_TOOLCHAIN_PREFIX)/bin/m68k-amigaos-size
TARGET_NM := $(MAGI80_TOOLCHAIN_PREFIX)/bin/m68k-amigaos-nm
TARGET_OBJDUMP := $(MAGI80_TOOLCHAIN_PREFIX)/bin/m68k-amigaos-objdump
TARGET_RUNTIME := -mcrt=nix20

AMIGA_BUILD_DIR := build/amiga
REPORT_DIR := build/reports
STAGING_DIR := build/staging
AMIGA_PROGRAM := $(AMIGA_BUILD_DIR)/magi80
AMIGA_MAP := $(AMIGA_BUILD_DIR)/magi80.map
STAGED_PROGRAM := $(STAGING_DIR)/magi80
SMOKE_BUILD_DIR := build/smoke
AGA_SCREEN_BUILD_DIR := $(SMOKE_BUILD_DIR)/aga-screen
AGA_SCREEN_SOURCE := tests/smoke/aga-screen/main.c
AGA_SCREEN_PROGRAM := $(AGA_SCREEN_BUILD_DIR)/program
AGA_SCREEN_MAP := $(AGA_SCREEN_BUILD_DIR)/program.map
AGA_SCREEN_EXPECTED := tests/smoke/aga-screen/expected.txt

TARGET_CFLAGS := \
	-std=c99 \
	-m68020 \
	-msoft-float \
	-Os \
	-Wall \
	-Wextra \
	-Werror \
	-fno-common \
	-ffunction-sections \
	-fdata-sections

.DELETE_ON_ERROR:

.PHONY: all amiga stage inspect vamos-test fs-uae-smoke aga-screen \
	aga-screen-inspect aga-screen-smoke runtime-compare check run clean

all: amiga

amiga: $(AMIGA_PROGRAM)

$(AMIGA_PROGRAM): src/main.c Makefile
	@mkdir -p $(AMIGA_BUILD_DIR)
	$(TARGET_CC) $(TARGET_CFLAGS) $< -Wl,-Map,$(AMIGA_MAP) -o $@ $(TARGET_RUNTIME)

stage: $(STAGED_PROGRAM)

$(STAGED_PROGRAM): $(AMIGA_PROGRAM)
	@mkdir -p $(STAGING_DIR)
	cp $< $@
	chmod 755 $@

inspect: $(AMIGA_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(TARGET_SIZE) $(AMIGA_PROGRAM) | tee $(REPORT_DIR)/magi80-size.txt
	$(TARGET_NM) --print-size --size-sort $(AMIGA_PROGRAM) >$(REPORT_DIR)/magi80-symbols.txt
	$(TARGET_OBJDUMP) -dr $(AMIGA_PROGRAM) >$(REPORT_DIR)/magi80-disassembly.txt

vamos-test: $(AMIGA_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	PATH="$(MAGI80_PIPX_BIN):$$PATH" vamos -C 20 $(AMIGA_PROGRAM) >$(REPORT_DIR)/magi80-vamos.txt
	diff -u tests/smoke/hosted-bootstrap/expected.txt $(REPORT_DIR)/magi80-vamos.txt

run: stage
	./scripts/run-fs-uae.sh a1200-pal-ks30-hd

fs-uae-smoke: stage
	./scripts/test-fs-uae-runtime.sh

aga-screen: $(AGA_SCREEN_PROGRAM)

$(AGA_SCREEN_PROGRAM): $(AGA_SCREEN_SOURCE) Makefile
	@mkdir -p $(AGA_SCREEN_BUILD_DIR)
	$(TARGET_CC) $(TARGET_CFLAGS) $< -Wl,-Map,$(AGA_SCREEN_MAP) -o $@ $(TARGET_RUNTIME)

aga-screen-inspect: $(AGA_SCREEN_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(TARGET_SIZE) $(AGA_SCREEN_PROGRAM) | tee $(REPORT_DIR)/aga-screen-size.txt
	$(TARGET_NM) --print-size --size-sort $(AGA_SCREEN_PROGRAM) >$(REPORT_DIR)/aga-screen-symbols.txt
	$(TARGET_OBJDUMP) -dr $(AGA_SCREEN_PROGRAM) >$(REPORT_DIR)/aga-screen-disassembly.txt

aga-screen-smoke: fs-uae-smoke aga-screen-inspect
	./scripts/test-fs-uae-runtime.sh $(AGA_SCREEN_PROGRAM) $(AGA_SCREEN_EXPECTED)

runtime-compare:
	./scripts/compare-c-runtimes.sh

check: inspect vamos-test aga-screen-smoke

clean:
	rm -rf $(AMIGA_BUILD_DIR) $(REPORT_DIR) $(SMOKE_BUILD_DIR) build/fs-uae-smoke build/runtime-comparison
	rm -f $(STAGED_PROGRAM) $(STAGING_DIR)/fs-uae-smoke.out
