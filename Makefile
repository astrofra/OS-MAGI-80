SHELL := /bin/bash

MAGI80_TOOLCHAIN_PREFIX ?= $(HOME)/.local/m68k-amigaos
MAGI80_PIPX_BIN ?= $(HOME)/.local/bin

TARGET_CC := $(MAGI80_TOOLCHAIN_PREFIX)/bin/m68k-amigaos-gcc
TARGET_SIZE := $(MAGI80_TOOLCHAIN_PREFIX)/bin/m68k-amigaos-size
TARGET_NM := $(MAGI80_TOOLCHAIN_PREFIX)/bin/m68k-amigaos-nm
TARGET_OBJDUMP := $(MAGI80_TOOLCHAIN_PREFIX)/bin/m68k-amigaos-objdump
TARGET_RUNTIME := -mcrt=nix20
HOST_CC ?= cc

PROJECT_CPPFLAGS := -Isrc

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
C2P_REFERENCE_SOURCE := src/graphics/c2p_reference.c
C2P_REFERENCE_LAYOUTS_SOURCE := src/graphics/c2p_reference_layouts.c
C2P_REFERENCE_SOURCES := $(C2P_REFERENCE_SOURCE) $(C2P_REFERENCE_LAYOUTS_SOURCE)
C2P_REFERENCE_HEADER := src/graphics/c2p_reference.h
HOST_BUILD_DIR := build/host
C2P_HOST_BUILD_DIR := $(HOST_BUILD_DIR)/c2p-reference
C2P_HOST_TEST_SOURCE := tests/host/c2p-reference/main.c
C2P_HOST_TEST_PROGRAM := $(C2P_HOST_BUILD_DIR)/test
C2P_HOST_TEST_EXPECTED := tests/host/c2p-reference/expected.txt
C2P_HOST_TEST_REPORT := $(REPORT_DIR)/c2p-reference-host.txt
BENCHMARK_BUILD_DIR := build/benchmark
C2P_BENCHMARK_BUILD_DIR := $(BENCHMARK_BUILD_DIR)/c2p-layouts
C2P_BENCHMARK_SOURCE := tests/benchmark/c2p-layouts/main.c
C2P_BENCHMARK_PROGRAM := $(C2P_BENCHMARK_BUILD_DIR)/program
C2P_BENCHMARK_MAP := $(C2P_BENCHMARK_BUILD_DIR)/program.map
C2P_BENCHMARK_REPORT := $(REPORT_DIR)/c2p-layouts-fs-uae.txt

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

HOST_CFLAGS := \
	-std=c99 \
	-O2 \
	-Wall \
	-Wextra \
	-Werror \
	-pedantic

C2P_BENCHMARK_CFLAGS = $(filter-out -Os,$(TARGET_CFLAGS)) -O2

.DELETE_ON_ERROR:

.PHONY: all amiga stage inspect vamos-test fs-uae-smoke c2p-test \
	aga-screen aga-screen-inspect aga-screen-smoke c2p-benchmark \
	c2p-benchmark-inspect c2p-benchmark-fs-uae runtime-compare check run clean

all: amiga

amiga: $(AMIGA_PROGRAM)

$(AMIGA_PROGRAM): src/main.c Makefile
	@mkdir -p $(AMIGA_BUILD_DIR)
	$(TARGET_CC) $(PROJECT_CPPFLAGS) $(TARGET_CFLAGS) $< -Wl,-Map,$(AMIGA_MAP) -o $@ $(TARGET_RUNTIME)

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

c2p-test: $(C2P_HOST_TEST_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(C2P_HOST_TEST_PROGRAM) >$(C2P_HOST_TEST_REPORT)
	diff -u $(C2P_HOST_TEST_EXPECTED) $(C2P_HOST_TEST_REPORT)

$(C2P_HOST_TEST_PROGRAM): $(C2P_HOST_TEST_SOURCE) $(C2P_REFERENCE_SOURCES) \
		$(C2P_REFERENCE_HEADER) Makefile
	@mkdir -p $(C2P_HOST_BUILD_DIR)
	$(HOST_CC) $(PROJECT_CPPFLAGS) $(HOST_CFLAGS) \
		$(C2P_REFERENCE_SOURCES) $(C2P_HOST_TEST_SOURCE) -o $@

aga-screen: $(AGA_SCREEN_PROGRAM)

$(AGA_SCREEN_PROGRAM): $(AGA_SCREEN_SOURCE) $(C2P_REFERENCE_SOURCE) \
		$(C2P_REFERENCE_HEADER) Makefile
	@mkdir -p $(AGA_SCREEN_BUILD_DIR)
	$(TARGET_CC) $(PROJECT_CPPFLAGS) $(TARGET_CFLAGS) \
		$(AGA_SCREEN_SOURCE) $(C2P_REFERENCE_SOURCE) \
		-Wl,-Map,$(AGA_SCREEN_MAP) -o $@ $(TARGET_RUNTIME)

aga-screen-inspect: $(AGA_SCREEN_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(TARGET_SIZE) $(AGA_SCREEN_PROGRAM) | tee $(REPORT_DIR)/aga-screen-size.txt
	$(TARGET_NM) --print-size --size-sort $(AGA_SCREEN_PROGRAM) >$(REPORT_DIR)/aga-screen-symbols.txt
	$(TARGET_OBJDUMP) -dr $(AGA_SCREEN_PROGRAM) >$(REPORT_DIR)/aga-screen-disassembly.txt

aga-screen-smoke: fs-uae-smoke aga-screen-inspect
	./scripts/test-fs-uae-runtime.sh $(AGA_SCREEN_PROGRAM) $(AGA_SCREEN_EXPECTED)

c2p-benchmark: $(C2P_BENCHMARK_PROGRAM)

$(C2P_BENCHMARK_PROGRAM): $(C2P_BENCHMARK_SOURCE) \
		$(C2P_REFERENCE_SOURCES) $(C2P_REFERENCE_HEADER) Makefile
	@mkdir -p $(C2P_BENCHMARK_BUILD_DIR)
	$(TARGET_CC) $(PROJECT_CPPFLAGS) $(C2P_BENCHMARK_CFLAGS) \
		$(C2P_BENCHMARK_SOURCE) $(C2P_REFERENCE_SOURCES) \
		-Wl,-Map,$(C2P_BENCHMARK_MAP) -o $@ $(TARGET_RUNTIME)

c2p-benchmark-inspect: $(C2P_BENCHMARK_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(TARGET_SIZE) $(C2P_BENCHMARK_PROGRAM) | tee $(REPORT_DIR)/c2p-layouts-size.txt
	$(TARGET_NM) --print-size --size-sort $(C2P_BENCHMARK_PROGRAM) >$(REPORT_DIR)/c2p-layouts-symbols.txt
	$(TARGET_OBJDUMP) -dr $(C2P_BENCHMARK_PROGRAM) >$(REPORT_DIR)/c2p-layouts-disassembly.txt

c2p-benchmark-fs-uae: stage c2p-benchmark-inspect
	./scripts/test-fs-uae-runtime.sh $(C2P_BENCHMARK_PROGRAM) -
	@mkdir -p $(REPORT_DIR)
	cp $(STAGING_DIR)/fs-uae-smoke.out $(C2P_BENCHMARK_REPORT)
	./scripts/validate-c2p-benchmark-report.sh $(C2P_BENCHMARK_REPORT)

runtime-compare:
	./scripts/compare-c-runtimes.sh

check: c2p-test inspect vamos-test aga-screen-smoke

clean:
	rm -rf $(AMIGA_BUILD_DIR) $(HOST_BUILD_DIR) $(REPORT_DIR) $(SMOKE_BUILD_DIR) $(BENCHMARK_BUILD_DIR) build/fs-uae-smoke build/runtime-comparison
	rm -f $(STAGED_PROGRAM) $(STAGING_DIR)/fs-uae-smoke.out
