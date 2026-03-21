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
COMMON_FLAGS = -mcpu=cortex-a53 -ffreestanding -nostdlib -Wall

ASFLAGS  = $(COMMON_FLAGS)
CXXFLAGS = $(COMMON_FLAGS) -fno-exceptions -fno-rtti
LDFLAGS  = -nostdlib

# --- Files ---
LINKER   = linker.ld
TARGET   = kernel8.img
ELF      = kernel8.elf

# Collect all source files from the src/ directory
ASM_SRC  = $(wildcard src/*.S)
CXX_SRC  = $(wildcard src/*.cpp) $(wildcard src/*/*.cpp)

# Convert source paths to object file paths in a build/ directory
ASM_OBJ  = $(patsubst src/%.S, build/%.o, $(ASM_SRC))
CXX_OBJ  = $(patsubst src/%.cpp, build/%.o, $(CXX_SRC))
OBJ      = $(ASM_OBJ) $(CXX_OBJ)

# --- Targets ---
.PHONY: all run clean

all: $(TARGET)

# Link all object files into an ELF, then strip to a raw binary image
# The Pi's bootloader loads kernel8.img as a flat binary at 0x80000
$(ELF): $(OBJ) $(LINKER)
	$(LD) $(LDFLAGS) -T $(LINKER) -o $(ELF) $(OBJ)

$(TARGET): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(TARGET)

# Assemble .S files
build/%.o: src/%.S | build
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
	qemu-system-aarch64 -M raspi3b -kernel $(TARGET) -serial stdio -display none

clean:
	rm -rf build $(ELF) $(TARGET)
