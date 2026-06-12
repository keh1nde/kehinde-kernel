# PiOS Makefile
# Target: AArch64 (Raspberry Pi 3 via QEMU)

# --- Toolchain ---
CC       = aarch64-elf-gcc
CXX      = aarch64-elf-g++
AS       = aarch64-elf-gcc
LD       = aarch64-elf-ld
OBJCOPY  = aarch64-elf-objcopy

# --- Flags ---
# -mcpu: target the Pi 3's Cortex-A53 cores
# -ffreestanding: no standard library or startup files assumed
# -nostdlib: don't link the C/C++ standard library
# -fno-exceptions -fno-rtti: required for bare-metal C++ (no runtime support)
# -Wall: enable all warnings — your best friend for catching mistakes early
# BOARD: Defines the target of the kernel.

BOARD ?= PI3b

ifeq ($(BOARD), PI3b)
	COMMON_FLAGS = -mcpu=cortex-a53 -ffreestanding -nostdlib -Wall
else ifeq ($(BOARD), PI5)
	COMMON_FLAGS = -mcpu=cortex-a76 -ffreestanding -nostdlib -Wall
endif

ASFLAGS  = $(COMMON_FLAGS) -DBOARD_$(BOARD)
CXXFLAGS = $(COMMON_FLAGS) -fno-exceptions -fno-rtti -DBOARD_$(BOARD)
LDFLAGS  = -nostdlib

# Per-board settings. BOARD_TAG is the lowercase suffix used to select
# board-specific source files (e.g. src/uart/uart_$(BOARD_TAG).cpp).
ifeq ($(BOARD), PI3b)
	QEMU_MACHINE = -M raspi3b
	BOARD_TAG    = pi3
else ifeq ($(BOARD), PI5)
	QEMU_MACHINE = -M virt
	BOARD_TAG    = pi5
endif

# --- Files ---
LINKER   = linker.ld
TARGET   = kernel8.img
ELF      = kernel8.elf

# Collect all source files from the src/ directory.
#
# Board-specific files are tagged with a board suffix (e.g. uart_pi3.cpp,
# interrupts_pi5.cpp). A file with NO board suffix is board-independent and is
# always compiled. A file tagged for the CURRENT board ($(BOARD_TAG)) is added;
# files tagged for any OTHER board are filtered out, so only one implementation
# of each board-specific symbol is ever linked.
#
# To add a new board: extend BOARD_TAGS and the per-board ifeq block above.
BOARD_TAGS = pi3 pi5

ALL_ASM  = $(wildcard src/*.S) $(wildcard src/*/*.S)
ALL_CXX  = $(wildcard src/*.cpp) $(wildcard src/*/*.cpp)

# Files tagged for some board (any tag in BOARD_TAGS).
TAGGED_ASM = $(foreach t,$(BOARD_TAGS),$(filter %_$(t).S,$(ALL_ASM)))
TAGGED_CXX = $(foreach t,$(BOARD_TAGS),$(filter %_$(t).cpp,$(ALL_CXX)))

# Board-independent files (no board tag) plus the current board's tagged files.
ASM_SRC  = $(filter-out $(TAGGED_ASM),$(ALL_ASM)) $(filter %_$(BOARD_TAG).S,$(ALL_ASM))
CXX_SRC  = $(filter-out $(TAGGED_CXX),$(ALL_CXX)) $(filter %_$(BOARD_TAG).cpp,$(ALL_CXX))

# Convert source paths to object file paths in a build/ directory
ASM_OBJ  = $(patsubst src/%.S, build/%.o, $(ASM_SRC))
CXX_OBJ  = $(patsubst src/%.cpp, build/%.o, $(CXX_SRC))
OBJ      = $(ASM_OBJ) $(CXX_OBJ)

# --- Targets ---
.PHONY: all run clean rebuild test test-debug

all: $(TARGET)

# Link all object files into an ELF, then strip to a raw binary image
# The Pi's bootloader loads kernel8.img as a flat binary at 0x80000
$(ELF): $(OBJ) $(LINKER)
	$(LD) $(LDFLAGS) -T $(LINKER) -o $(ELF) $(OBJ)

$(TARGET): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(TARGET)

# Assemble .S files
build/%.o: src/%.S | build
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# Compile .cpp files
build/%.o: src/%.cpp | build
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

# Create build directory if it doesn't exist
build:
	mkdir -p build

# Launch in QEMU — serial output goes to your terminal
# -M raspi3b: emulate Raspberry Pi 3 Model B
# -serial stdio: UART output appears in your terminal
# -display none: no graphical window (we only use serial)
run: $(TARGET)
	qemu-system-aarch64 $(QEMU_MACHINE) -kernel $(TARGET) -serial stdio -display none

debug: $(TARGET)
	qemu-system-aarch64 $(QEMU_MACHINE) -kernel $(TARGET) -serial stdio -display none -d int 2>qemu_log.txt

clean:
	rm -rf build $(ELF) $(TARGET)

rebuild:
	$(MAKE) clean
	$(MAKE) all

test:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) run

test-debug:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) debug