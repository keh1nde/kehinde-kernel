# kehinde-kernel

**kehinde-kernel** is a bare-metal AArch64 operating system for the Raspberry Pi 3 Model B and Pi 5 that I wrote from scratch in C++ and AArch64 assembly. It boots on real hardware and under QEMU brings up its own paging, manages its own memory, and drops the user into an interactive shell sitting on an in-memory filesystem.

[![asciicast](https://asciinema.org/a/GkIA1y66Ms6ZYH43.svg)](https://asciinema.org/a/GkIA1y66Ms6ZYH43)

## Quickstart

This process differs slightly when using the kernel on the Pi 3b or the Pi 5.

## Using kehinde-kernel on the Raspberry Pi 3 Model B
The easiest way to use kehinde-kernel is via the latest release. Building from source needs the AArch64 cross toolchain.

#### Option A: run the release binary (no toolchain required)

You only need QEMU:

```bash
brew install qemu                                       # macOS
sudo apt-get install qemu-system-arm                    # Debian / Ubuntu

wget https://github.com/keh1nde/kehinde-kernel/releases/latest/download/kernel8.img
qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial stdio -display none
```

A shell prompt should appear within a second or so. Type `help` for the command list.

#### Option B: build from source

Prerequisites:

- `aarch64-elf-gcc` (cross compiler — `brew install aarch64-elf-gcc` on macOS, or build from `gcc-arm-none-eabi`-style toolchains on Linux)
- `qemu-system-aarch64`
- GNU `make`

Then:

```bash
make run        # build + boot in QEMU, serial to your terminal
make debug      # same, plus interrupt logging to qemu_log.txt
make clean
```

## Using kehinde-kernel on the Raspberry Pi 5

Prerequisites:
1. A Raspberry Pi 5
2. A microSD card
3. A USB to TTL serial adapter

**Step 1 — Prepare the SD card**

Download [Raspberry Pi Imager](https://www.raspberrypi.com/software/) and flash **Raspberry Pi OS Lite (64-bit)** onto the microSD card. This populates the boot partition with the firmware files the Pi 5 bootloader requires.

**Step 2 — Replace the kernel**

Eject and reinsert the card. Open the boot partition (mounted as `bootfs` on macOS, `/boot/firmware` on most Linux distributions), then run:

```bash
wget https://github.com/keh1nde/kehinde-kernel/releases/latest/download/kernel8.img

# The Pi 5 bootloader requires the image size to be a multiple of 512 bytes.
truncate -s $(((($(wc -c < kernel8.img) + 511) / 512) * 512)) kernel8.img

cp kernel8.img /Volumes/bootfs/kernel8.img        # macOS
# cp kernel8.img /boot/firmware/kernel8.img        # Linux
```

**Step 3 — Edit config.txt**

Open `config.txt` on the boot partition and append:

```
kernel=kernel8.img
enable_rp1_uart=1
pciex4_reset=0
```

**Step 4 — Wire the TTL adapter**

Connect the adapter to the Pi 5's 40-pin header:

| Adapter pin | Pi 5 header pin           |
|-------------|---------------------------|
| RX          | Pin 8  (GPIO 14, UART TX) |
| TX          | Pin 10 (GPIO 15, UART RX) |
| GND         | Pin 6  (GND)              |

Do not connect the adapter's VCC to the Pi.

**Step 5 — Boot and connect**

Insert the SD card and plug the adapter into your machine. Find the adapter's device node:

```bash
# macOS
ls /dev/tty.usb*

# Linux
ls /dev/ttyUSB*
```

Each command lists every USB serial device currently attached. Plug the adapter in, run the command, then unplug and run it again — the entry that disappears is the adapter's node.

Open a serial terminal at 115200 baud before powering the Pi:

```bash
screen /dev/tty.usbserial-XXXX 115200    # macOS — replace with the node from above
screen /dev/ttyUSB0 115200               # Linux — replace with the node from above
```

Power the Pi. A shell prompt should appear within a second or two. Type `help` for the command list.


## What's inside

The kernel includes the following features:

- **Boot sequence.** Parks secondary cores, drops from exception level 2 to exception level 1, sets up the stack, zeroes the BSS section, installs the exception vector table, unmasks interrupts, and hands off to the C++ kernel entry point. (`src/boot.S`)
- **Serial driver.** A driver for the Raspberry Pi's PL011 UART, configured for 115200 baud, 8N1. Used for boot logging, panic messages, and the interactive shell. (`src/uart/`)
- **Interrupts and exception handling.** A 16-entry AArch64 vector table with register-save/restore stubs in assembly, dispatching to C++ handlers. The synchronous-exception handler decodes the exception syndrome register and dumps the fault address, which made the rest of the kernel possible to debug. Currently, only the entries for synchronous exceptions and IRQs for the current Exception Level are implemented. (`src/interrupts/`)
- **Generic timer.** The Cortex-A53's physical timer programmed to fire roughly every 100 milliseconds (10 Hz). Tick handling lives in the interrupt path. (`src/timer/`)
- **Physical memory manager.** A flat-bitmap frame allocator over the Pi's RAM, with 4 KiB frames and a bitmap placed at the page-aligned end of the kernel image. (`src/pmm/`)
- **Memory management unit (paging / virtual memory).** Three-level page tables (level 1 → level 2 → level 3) with a 4 KiB granule and a 39-bit virtual address space. The kernel runs identity-mapped: I bring up the tables, flip the MMU on, and keep going. Includes a software page-table walker, on-demand table allocation, an `unmap` path, and the proper barrier / TLB-invalidate sequence around every page-table edit. (`src/mmu/`)
- **Kernel heap.** A bump allocator with lazy physical-page backing — physical frames are pulled from the physical memory manager and mapped into the heap's virtual range only as the bump pointer crosses page boundaries. (`src/heap_alloc/`)
- **In-memory filesystem.** A small filesystem with files and directories, supporting create, read, write, unlink, readdir, and stat. Directories grow by chaining 4 KiB blocks; the unlink path uses a swap-with-last trick for O(1) removal. (`src/filesystem/`)
- **Interactive shell.** Eleven commands — `help`, `ls`, `pwd`, `cd`, `mkdir`, `touch`, `write`, `cat`, `rm`, `clear`, `kernel`, `shutdown` — that talk directly to the filesystem API. No file-descriptor layer; the shell just resolves paths to inode numbers. (`src/shell/`)

## Architecture overview

| Module                                  | Role                                                                          | Entry file                          |
| --------------------------------------- | ----------------------------------------------------------------------------- | ----------------------------------- |
| Boot                                    | Drop to exception level 1, set up stack and BSS, hand off to C++              | `src/boot.S`                        |
| Kernel entry                            | Initialize subsystems in order, launch the shell                              | `src/kernel.cpp`                    |
| Serial driver                           | PL011 UART — boot logging, shell I/O                                          | `src/uart/uart.cpp`                 |
| Interrupts                              | Vector table, exception-syndrome decoding, interrupt controller bring-up      | `src/interrupts/`                   |
| Generic timer                           | Cortex-A53 physical timer, 10 Hz tick                                         | `src/timer/timer.cpp`               |
| Physical memory manager                 | Bitmap frame allocator over RAM                                               | `src/pmm/pmm.cpp`                   |
| Memory management unit                  | Three-level paging, identity map, software walker                             | `src/mmu/mmu.cpp`                   |
| Kernel heap                             | Bump allocator with lazy page-backed virtual memory                           | `src/heap_alloc/heap_alloc.cpp`     |
| In-memory filesystem                    | Inode list, block-chained directories and files                               | `src/filesystem/filesystem.cpp`     |
| Shell                                   | Interactive line editor and command dispatcher                                | `src/shell/shell.cpp`               |
| Linker script                           | Memory layout, BSS / rodata / data section globbing, `__kernel_end` symbol    | `linker.ld`                         |

Initialization order in `kernel_main` is load-bearing: paging comes up first (which internally brings up the physical memory manager), then the heap (which depends on paging for lazy mapping), then the filesystem (which depends on the heap), then the interrupt controller, then the shell.

## Project writeup

I've written at length everything that went into this project. This includes design, implementation, hitches, and more. Note that the writeup only covers details and features that originated prior to v1.0.0

<!-- TODO: replace with the actual writeup link once it's published. -->
[View the writeup here](https://cuny907-my.sharepoint.com/:b:/g/personal/kehinde_adeoso77_login_cuny_edu/IQDQ8UB1CopFS7LEZFEokoCJAd-rrPZ4r6--N73vv5tU2Xw?e=tpBXf2)


## References

I used the following documents as reference during project implementation

<!-- Make sure to list the Dinosaur Book and the OSDev wiki -->

- **Arm Architecture Reference Manual for A-profile architecture (ARM DDI 0487)** — exception levels, the virtual memory architecture (VMSAv8-64), and the generic timer. <https://developer.arm.com/documentation/ddi0487/latest/>
- **Arm Cortex-A53 Technical Reference Manual (ARM DDI 0500)** — the specific core on the Pi 3 Model B. <https://developer.arm.com/documentation/ddi0500/latest/>
- **BCM2835 ARM Peripherals** — the SoC's memory-mapped I/O layout: the PL011 UART, the legacy interrupt controller, the GPIO block. <https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf>
- **Operating Systems Concepts by Avi Silberschatz, Peter Baer Galvin, and Greg Gagne** — a textbook covering all fundamental operating system structures, conventions, and others. <https://os-book.com/OS10/index.html>

## License

I've released this project under the MIT License. See [LICENSE](LICENSE) for details.
