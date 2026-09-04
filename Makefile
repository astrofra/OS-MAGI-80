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
C2P4_REFERENCE_SOURCE := src/graphics/c2p4_reference.c
C2P4_LOOKUP_SOURCE := src/graphics/c2p4_lookup.c
C2P4_MASK32_SOURCE := src/graphics/c2p4_mask32.c
C2P4_M68K_SOURCE := src/graphics/c2p4_m68k.c
C2P4_M68K_ASM_SOURCE := src/graphics/c2p4_m68k.S
C2P4_SOURCES := $(C2P4_REFERENCE_SOURCE) $(C2P4_LOOKUP_SOURCE) \
	$(C2P4_MASK32_SOURCE) \
	$(C2P4_M68K_SOURCE)
C2P4_TARGET_SOURCES := $(C2P4_SOURCES) $(C2P4_M68K_ASM_SOURCE)
C2P4_HEADER := src/graphics/c2p4_reference.h
C2P4_HOST_BUILD_DIR := $(HOST_BUILD_DIR)/c2p4-reference
C2P4_HOST_TEST_SOURCE := tests/host/c2p4-reference/main.c
C2P4_HOST_TEST_PROGRAM := $(C2P4_HOST_BUILD_DIR)/test
C2P4_HOST_TEST_EXPECTED := tests/host/c2p4-reference/expected.txt
C2P4_HOST_TEST_REPORT := $(REPORT_DIR)/c2p4-reference-host.txt
GRAPHICS_REFERENCE_SOURCE := src/graphics/reference_compositor.c
GRAPHICS_REFERENCE_HEADER := src/graphics/reference_compositor.h
GRAPHICS_REFERENCE_BUILD_DIR := $(HOST_BUILD_DIR)/graphics-reference
GRAPHICS_REFERENCE_TEST_SOURCE := tests/host/graphics-reference/main.c
GRAPHICS_REFERENCE_TEST_PROGRAM := $(GRAPHICS_REFERENCE_BUILD_DIR)/test
GRAPHICS_REFERENCE_TEST_EXPECTED := tests/host/graphics-reference/expected.txt
GRAPHICS_REFERENCE_TEST_REPORT := $(REPORT_DIR)/graphics-reference-host.txt
AGA_REFERENCE_SOURCE := src/graphics/aga_reference_decoder.c
AGA_REFERENCE_HEADER := src/graphics/aga_reference_decoder.h
AGA_REFERENCE_BUILD_DIR := $(HOST_BUILD_DIR)/aga-reference-decoder
AGA_REFERENCE_TEST_SOURCE := tests/host/aga-reference-decoder/main.c
AGA_REFERENCE_TEST_PROGRAM := $(AGA_REFERENCE_BUILD_DIR)/test
AGA_REFERENCE_TEST_EXPECTED := tests/host/aga-reference-decoder/expected.txt
AGA_REFERENCE_TEST_REPORT := $(REPORT_DIR)/aga-reference-decoder-host.txt
GRAPHICS_REPORT_TEST_SOURCE := tests/host/graphics-benchmark-report/test.sh
GRAPHICS_REPORT_TEST_EXPECTED := tests/host/graphics-benchmark-report/expected.txt
GRAPHICS_REPORT_TEST_REPORT := $(REPORT_DIR)/graphics-benchmark-report-host.txt
GRAPHICS_REPORT_VALIDATOR := scripts/validate-graphics-benchmark-report.sh
BENCHMARK_BUILD_DIR := build/benchmark
C2P_BENCHMARK_BUILD_DIR := $(BENCHMARK_BUILD_DIR)/c2p-layouts
C2P_BENCHMARK_SOURCE := tests/benchmark/c2p-layouts/main.c
C2P_BENCHMARK_PROGRAM := $(C2P_BENCHMARK_BUILD_DIR)/program
C2P_BENCHMARK_MAP := $(C2P_BENCHMARK_BUILD_DIR)/program.map
C2P_BENCHMARK_REPORT := $(REPORT_DIR)/c2p-layouts-fs-uae.txt
C2P4_BENCHMARK_BUILD_DIR := $(BENCHMARK_BUILD_DIR)/c2p4
C2P4_BENCHMARK_SOURCE := tests/benchmark/c2p4/main.c
C2P4_BENCHMARK_PROGRAM := $(C2P4_BENCHMARK_BUILD_DIR)/program
C2P4_BENCHMARK_MAP := $(C2P4_BENCHMARK_BUILD_DIR)/program.map
C2P4_BENCHMARK_REPORT := $(REPORT_DIR)/c2p4-fs-uae.txt
CHIPRAM_BENCHMARK_BUILD_DIR := $(BENCHMARK_BUILD_DIR)/chipram
CHIPRAM_BENCHMARK_SOURCE := tests/benchmark/chipram/main.c
CHIPRAM_BENCHMARK_ASM_SOURCE := tests/benchmark/chipram/kernels.S
CHIPRAM_BENCHMARK_HEADER := tests/benchmark/chipram/kernels.h
CHIPRAM_BENCHMARK_PROGRAM := $(CHIPRAM_BENCHMARK_BUILD_DIR)/program
CHIPRAM_BENCHMARK_MAP := $(CHIPRAM_BENCHMARK_BUILD_DIR)/program.map
CHIPRAM_BENCHMARK_REPORT := $(REPORT_DIR)/chipram-fs-uae.txt
CHIPRAM_REPORT_VALIDATOR := scripts/validate-chipram-benchmark-report.sh
CHIPRAM_REPORT_TEST_SOURCE := tests/host/chipram-benchmark-report/test.sh
CHIPRAM_REPORT_TEST_EXPECTED := tests/host/chipram-benchmark-report/expected.txt
CHIPRAM_REPORT_TEST_REPORT := $(REPORT_DIR)/chipram-benchmark-report-host.txt
EXCLUSIVE_GRAPHICS_BUILD_DIR := $(BENCHMARK_BUILD_DIR)/exclusive-graphics
EXCLUSIVE_GRAPHICS_SOURCE := tests/benchmark/exclusive-graphics/main.c
EXCLUSIVE_GRAPHICS_PROGRAM := $(EXCLUSIVE_GRAPHICS_BUILD_DIR)/program
EXCLUSIVE_GRAPHICS_MAP := $(EXCLUSIVE_GRAPHICS_BUILD_DIR)/program.map
EXCLUSIVE_GRAPHICS_REPORT := $(REPORT_DIR)/exclusive-graphics-fs-uae.txt
EXCLUSIVE_GRAPHICS_REPORT_VALIDATOR := \
	scripts/validate-exclusive-graphics-benchmark-report.sh
EXCLUSIVE_GRAPHICS_REPORT_TEST_SOURCE := \
	tests/host/exclusive-graphics-benchmark-report/test.sh
EXCLUSIVE_GRAPHICS_REPORT_TEST_EXPECTED := \
	tests/host/exclusive-graphics-benchmark-report/expected.txt
EXCLUSIVE_GRAPHICS_REPORT_TEST_REPORT := \
	$(REPORT_DIR)/exclusive-graphics-benchmark-report-host.txt
EXCLUSIVE_GRAPHICS_PHYSICAL_BUILD_DIR := \
	$(BENCHMARK_BUILD_DIR)/exclusive-graphics-physical
EXCLUSIVE_GRAPHICS_PHYSICAL_PROGRAM := \
	$(EXCLUSIVE_GRAPHICS_PHYSICAL_BUILD_DIR)/program
EXCLUSIVE_GRAPHICS_PHYSICAL_MAP := \
	$(EXCLUSIVE_GRAPHICS_PHYSICAL_BUILD_DIR)/program.map
EXCLUSIVE_GRAPHICS_PHYSICAL_STARTUP := \
	tests/benchmark/exclusive-graphics/physical-startup-sequence
EXCLUSIVE_GRAPHICS_PHYSICAL_README := \
	tests/benchmark/exclusive-graphics/physical-readme.txt
EXCLUSIVE_GRAPHICS_ADF_BUILDER := \
	scripts/build-exclusive-graphics-test-adf.sh
EXCLUSIVE_GRAPHICS_ADF_TESTER := \
	scripts/test-exclusive-graphics-adf-fs-uae.sh
DISTRIBUTION_DIR := build/distribution
EXCLUSIVE_GRAPHICS_TEST_ADF := \
	$(DISTRIBUTION_DIR)/magi80-exclusive-graphics-test.adf
EXCLUSIVE_GRAPHICS_TEST_ADF_MANIFEST := \
	$(DISTRIBUTION_DIR)/magi80-exclusive-graphics-test.manifest.txt

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
	c2p4-test graphics-reference-test aga-reference-test \
	graphics-report-test chipram-report-test \
	exclusive-graphics-report-test \
	aga-screen aga-screen-inspect aga-screen-smoke c2p-benchmark \
	c2p-benchmark-inspect c2p-benchmark-fs-uae c2p4-benchmark \
	c2p4-benchmark-inspect c2p4-benchmark-fs-uae chipram-benchmark \
	chipram-benchmark-inspect chipram-benchmark-fs-uae \
	exclusive-graphics-benchmark exclusive-graphics-benchmark-inspect \
	exclusive-graphics-benchmark-fs-uae exclusive-graphics-test-adf \
	exclusive-graphics-test-adf-inspect exclusive-graphics-test-adf-fs-uae \
	runtime-compare \
	check run clean

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

c2p4-test: $(C2P4_HOST_TEST_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(C2P4_HOST_TEST_PROGRAM) >$(C2P4_HOST_TEST_REPORT)
	diff -u $(C2P4_HOST_TEST_EXPECTED) $(C2P4_HOST_TEST_REPORT)

$(C2P4_HOST_TEST_PROGRAM): $(C2P4_HOST_TEST_SOURCE) $(C2P4_SOURCES) \
		$(C2P4_HEADER) $(AGA_REFERENCE_SOURCE) $(AGA_REFERENCE_HEADER) \
		$(GRAPHICS_REFERENCE_SOURCE) $(GRAPHICS_REFERENCE_HEADER) Makefile
	@mkdir -p $(C2P4_HOST_BUILD_DIR)
	$(HOST_CC) $(PROJECT_CPPFLAGS) $(HOST_CFLAGS) \
		$(C2P4_SOURCES) $(AGA_REFERENCE_SOURCE) \
		$(GRAPHICS_REFERENCE_SOURCE) $(C2P4_HOST_TEST_SOURCE) -o $@

graphics-reference-test: $(GRAPHICS_REFERENCE_TEST_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(GRAPHICS_REFERENCE_TEST_PROGRAM) >$(GRAPHICS_REFERENCE_TEST_REPORT)
	diff -u $(GRAPHICS_REFERENCE_TEST_EXPECTED) \
		$(GRAPHICS_REFERENCE_TEST_REPORT)

$(GRAPHICS_REFERENCE_TEST_PROGRAM): $(GRAPHICS_REFERENCE_TEST_SOURCE) \
		$(GRAPHICS_REFERENCE_SOURCE) $(GRAPHICS_REFERENCE_HEADER) Makefile
	@mkdir -p $(GRAPHICS_REFERENCE_BUILD_DIR)
	$(HOST_CC) $(PROJECT_CPPFLAGS) $(HOST_CFLAGS) \
		$(GRAPHICS_REFERENCE_SOURCE) $(GRAPHICS_REFERENCE_TEST_SOURCE) -o $@

aga-reference-test: $(AGA_REFERENCE_TEST_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(AGA_REFERENCE_TEST_PROGRAM) >$(AGA_REFERENCE_TEST_REPORT)
	diff -u $(AGA_REFERENCE_TEST_EXPECTED) $(AGA_REFERENCE_TEST_REPORT)

$(AGA_REFERENCE_TEST_PROGRAM): $(AGA_REFERENCE_TEST_SOURCE) \
		$(AGA_REFERENCE_SOURCE) $(AGA_REFERENCE_HEADER) \
		$(GRAPHICS_REFERENCE_SOURCE) $(GRAPHICS_REFERENCE_HEADER) \
		$(C2P_REFERENCE_SOURCES) $(C2P_REFERENCE_HEADER) Makefile
	@mkdir -p $(AGA_REFERENCE_BUILD_DIR)
	$(HOST_CC) $(PROJECT_CPPFLAGS) $(HOST_CFLAGS) \
		$(AGA_REFERENCE_SOURCE) $(GRAPHICS_REFERENCE_SOURCE) \
		$(C2P_REFERENCE_SOURCES) $(AGA_REFERENCE_TEST_SOURCE) -o $@

graphics-report-test: $(GRAPHICS_REPORT_TEST_SOURCE) \
		$(GRAPHICS_REPORT_TEST_EXPECTED) $(GRAPHICS_REPORT_VALIDATOR)
	@mkdir -p $(REPORT_DIR)
	LC_ALL=C LANG=C $(GRAPHICS_REPORT_TEST_SOURCE) \
		>$(GRAPHICS_REPORT_TEST_REPORT)
	diff -u $(GRAPHICS_REPORT_TEST_EXPECTED) \
		$(GRAPHICS_REPORT_TEST_REPORT)

chipram-report-test: $(CHIPRAM_REPORT_TEST_SOURCE) \
		$(CHIPRAM_REPORT_TEST_EXPECTED) $(CHIPRAM_REPORT_VALIDATOR)
	@mkdir -p $(REPORT_DIR)
	LC_ALL=C LANG=C $(CHIPRAM_REPORT_TEST_SOURCE) \
		>$(CHIPRAM_REPORT_TEST_REPORT)
	diff -u $(CHIPRAM_REPORT_TEST_EXPECTED) \
		$(CHIPRAM_REPORT_TEST_REPORT)

exclusive-graphics-report-test: $(EXCLUSIVE_GRAPHICS_REPORT_TEST_SOURCE) \
		$(EXCLUSIVE_GRAPHICS_REPORT_TEST_EXPECTED) \
		$(EXCLUSIVE_GRAPHICS_REPORT_VALIDATOR)
	@mkdir -p $(REPORT_DIR)
	LC_ALL=C LANG=C $(EXCLUSIVE_GRAPHICS_REPORT_TEST_SOURCE) \
		>$(EXCLUSIVE_GRAPHICS_REPORT_TEST_REPORT)
	diff -u $(EXCLUSIVE_GRAPHICS_REPORT_TEST_EXPECTED) \
		$(EXCLUSIVE_GRAPHICS_REPORT_TEST_REPORT)

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

c2p4-benchmark: $(C2P4_BENCHMARK_PROGRAM)

$(C2P4_BENCHMARK_PROGRAM): $(C2P4_BENCHMARK_SOURCE) \
		$(C2P4_TARGET_SOURCES) \
		$(C2P4_HEADER) $(AGA_REFERENCE_SOURCE) $(AGA_REFERENCE_HEADER) \
		$(GRAPHICS_REFERENCE_SOURCE) $(GRAPHICS_REFERENCE_HEADER) Makefile
	@mkdir -p $(C2P4_BENCHMARK_BUILD_DIR)
	$(TARGET_CC) $(PROJECT_CPPFLAGS) $(C2P_BENCHMARK_CFLAGS) \
		$(C2P4_BENCHMARK_SOURCE) $(C2P4_TARGET_SOURCES) \
		$(GRAPHICS_REFERENCE_SOURCE) $(AGA_REFERENCE_SOURCE) \
		-Wl,-Map,$(C2P4_BENCHMARK_MAP) -o $@ $(TARGET_RUNTIME)

c2p4-benchmark-inspect: $(C2P4_BENCHMARK_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(TARGET_SIZE) $(C2P4_BENCHMARK_PROGRAM) | \
		tee $(REPORT_DIR)/c2p4-size.txt
	$(TARGET_NM) --print-size --size-sort $(C2P4_BENCHMARK_PROGRAM) \
		>$(REPORT_DIR)/c2p4-symbols.txt
	$(TARGET_OBJDUMP) -dr $(C2P4_BENCHMARK_PROGRAM) \
		>$(REPORT_DIR)/c2p4-disassembly.txt

c2p4-benchmark-fs-uae: stage c2p4-benchmark-inspect
	MAGI80_FS_UAE_TIMEOUT_SECONDS=360 \
		./scripts/test-fs-uae-runtime.sh $(C2P4_BENCHMARK_PROGRAM) -
	@mkdir -p $(REPORT_DIR)
	cp $(STAGING_DIR)/fs-uae-smoke.out $(C2P4_BENCHMARK_REPORT)
	$(GRAPHICS_REPORT_VALIDATOR) $(C2P4_BENCHMARK_REPORT)

chipram-benchmark: $(CHIPRAM_BENCHMARK_PROGRAM)

$(CHIPRAM_BENCHMARK_PROGRAM): $(CHIPRAM_BENCHMARK_SOURCE) \
		$(CHIPRAM_BENCHMARK_ASM_SOURCE) $(CHIPRAM_BENCHMARK_HEADER) \
		Makefile
	@mkdir -p $(CHIPRAM_BENCHMARK_BUILD_DIR)
	$(TARGET_CC) $(PROJECT_CPPFLAGS) $(C2P_BENCHMARK_CFLAGS) \
		$(CHIPRAM_BENCHMARK_SOURCE) $(CHIPRAM_BENCHMARK_ASM_SOURCE) \
		-Wl,-Map,$(CHIPRAM_BENCHMARK_MAP) -o $@ $(TARGET_RUNTIME)

chipram-benchmark-inspect: $(CHIPRAM_BENCHMARK_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(TARGET_SIZE) $(CHIPRAM_BENCHMARK_PROGRAM) | \
		tee $(REPORT_DIR)/chipram-size.txt
	$(TARGET_NM) --print-size --size-sort $(CHIPRAM_BENCHMARK_PROGRAM) \
		>$(REPORT_DIR)/chipram-symbols.txt
	$(TARGET_OBJDUMP) -dr $(CHIPRAM_BENCHMARK_PROGRAM) \
		>$(REPORT_DIR)/chipram-disassembly.txt

chipram-benchmark-fs-uae: stage chipram-benchmark-inspect
	MAGI80_FS_UAE_TIMEOUT_SECONDS=60 \
		./scripts/test-fs-uae-runtime.sh $(CHIPRAM_BENCHMARK_PROGRAM) -
	@mkdir -p $(REPORT_DIR)
	cp $(STAGING_DIR)/fs-uae-smoke.out $(CHIPRAM_BENCHMARK_REPORT)
	$(CHIPRAM_REPORT_VALIDATOR) $(CHIPRAM_BENCHMARK_REPORT)

exclusive-graphics-benchmark: $(EXCLUSIVE_GRAPHICS_PROGRAM)

$(EXCLUSIVE_GRAPHICS_PROGRAM): $(EXCLUSIVE_GRAPHICS_SOURCE) \
		$(CHIPRAM_BENCHMARK_ASM_SOURCE) $(CHIPRAM_BENCHMARK_HEADER) \
		$(C2P4_TARGET_SOURCES) $(C2P4_HEADER) Makefile
	@mkdir -p $(EXCLUSIVE_GRAPHICS_BUILD_DIR)
	$(TARGET_CC) $(PROJECT_CPPFLAGS) $(C2P_BENCHMARK_CFLAGS) \
		$(EXCLUSIVE_GRAPHICS_SOURCE) $(CHIPRAM_BENCHMARK_ASM_SOURCE) \
		$(C2P4_TARGET_SOURCES) \
		-Wl,-Map,$(EXCLUSIVE_GRAPHICS_MAP) -o $@ $(TARGET_RUNTIME)

exclusive-graphics-benchmark-inspect: $(EXCLUSIVE_GRAPHICS_PROGRAM)
	@mkdir -p $(REPORT_DIR)
	$(TARGET_SIZE) $(EXCLUSIVE_GRAPHICS_PROGRAM) | \
		tee $(REPORT_DIR)/exclusive-graphics-size.txt
	$(TARGET_NM) --print-size --size-sort $(EXCLUSIVE_GRAPHICS_PROGRAM) \
		>$(REPORT_DIR)/exclusive-graphics-symbols.txt
	$(TARGET_OBJDUMP) -dr $(EXCLUSIVE_GRAPHICS_PROGRAM) \
		>$(REPORT_DIR)/exclusive-graphics-disassembly.txt

exclusive-graphics-benchmark-fs-uae: stage \
		exclusive-graphics-benchmark-inspect
	MAGI80_FS_UAE_TIMEOUT_SECONDS=240 \
		./scripts/test-fs-uae-runtime.sh $(EXCLUSIVE_GRAPHICS_PROGRAM) -
	@mkdir -p $(REPORT_DIR)
	cp $(STAGING_DIR)/fs-uae-smoke.out $(EXCLUSIVE_GRAPHICS_REPORT)
	$(EXCLUSIVE_GRAPHICS_REPORT_VALIDATOR) $(EXCLUSIVE_GRAPHICS_REPORT)

$(EXCLUSIVE_GRAPHICS_PHYSICAL_PROGRAM): $(EXCLUSIVE_GRAPHICS_SOURCE) \
		$(CHIPRAM_BENCHMARK_ASM_SOURCE) $(CHIPRAM_BENCHMARK_HEADER) \
		$(C2P4_TARGET_SOURCES) $(C2P4_HEADER) Makefile
	@mkdir -p $(EXCLUSIVE_GRAPHICS_PHYSICAL_BUILD_DIR)
	$(TARGET_CC) $(PROJECT_CPPFLAGS) $(C2P_BENCHMARK_CFLAGS) \
		-DMAGI80_BENCHMARK_ENVIRONMENT=\"physical_a1200_pal_candidate\" \
		-DMAGI80_BENCHMARK_AUTHORITY=\"real_hardware_candidate\" \
		-DMAGI80_BENCHMARK_REPORT_PATH=\"MAGI80BENCH:RESULT.TXT\" \
		$(EXCLUSIVE_GRAPHICS_SOURCE) $(CHIPRAM_BENCHMARK_ASM_SOURCE) \
		$(C2P4_TARGET_SOURCES) \
		-Wl,-Map,$(EXCLUSIVE_GRAPHICS_PHYSICAL_MAP) -o $@ \
		$(TARGET_RUNTIME)

exclusive-graphics-test-adf: $(EXCLUSIVE_GRAPHICS_TEST_ADF)

$(EXCLUSIVE_GRAPHICS_TEST_ADF): $(EXCLUSIVE_GRAPHICS_PHYSICAL_PROGRAM) \
		$(EXCLUSIVE_GRAPHICS_PHYSICAL_STARTUP) \
		$(EXCLUSIVE_GRAPHICS_PHYSICAL_README) \
		$(EXCLUSIVE_GRAPHICS_ADF_BUILDER)
	$(EXCLUSIVE_GRAPHICS_ADF_BUILDER) \
		$(EXCLUSIVE_GRAPHICS_PHYSICAL_PROGRAM) \
		$(EXCLUSIVE_GRAPHICS_PHYSICAL_STARTUP) \
		$(EXCLUSIVE_GRAPHICS_PHYSICAL_README) $@

exclusive-graphics-test-adf-inspect: $(EXCLUSIVE_GRAPHICS_TEST_ADF)
	xdfscan $(EXCLUSIVE_GRAPHICS_TEST_ADF)
	xdftool $(EXCLUSIVE_GRAPHICS_TEST_ADF) list
	@printf 'Manifest: %s\n' $(EXCLUSIVE_GRAPHICS_TEST_ADF_MANIFEST)

exclusive-graphics-test-adf-fs-uae: $(EXCLUSIVE_GRAPHICS_TEST_ADF) \
		$(EXCLUSIVE_GRAPHICS_ADF_TESTER) \
		$(EXCLUSIVE_GRAPHICS_REPORT_VALIDATOR)
	MAGI80_FS_UAE_TIMEOUT_SECONDS=360 \
		$(EXCLUSIVE_GRAPHICS_ADF_TESTER) $(EXCLUSIVE_GRAPHICS_TEST_ADF)

runtime-compare:
	./scripts/compare-c-runtimes.sh

check: c2p-test c2p4-test graphics-reference-test aga-reference-test \
	graphics-report-test chipram-report-test exclusive-graphics-report-test \
	chipram-benchmark exclusive-graphics-benchmark inspect vamos-test \
	aga-screen-smoke

clean:
	rm -rf $(AMIGA_BUILD_DIR) $(HOST_BUILD_DIR) $(REPORT_DIR) \
		$(SMOKE_BUILD_DIR) $(BENCHMARK_BUILD_DIR) $(DISTRIBUTION_DIR) \
		build/fs-uae-smoke build/fs-uae-physical-adf \
		build/runtime-comparison
	rm -f $(STAGED_PROGRAM) $(STAGING_DIR)/fs-uae-smoke.out
