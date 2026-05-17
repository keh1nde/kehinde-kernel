# kehinde-kernel

**kehinde-kernel** is a bare-metal AArch64 operating system for the Raspberry Pi 3 Model B that I wrote from scratch in C++ and AArch64 assembly. It boots on real hardware and under QEMU brings up its own paging, manages its own memory, and drops the user into an interactive shell sitting on an in-memory filesystem — all without a standard library underneath.

<!--
  TODO: replace this block with an asciinema embed of a shell session.
  Suggested flow: boot, help, mkdir foo, cd foo, touch hello, write hello
  "hi there", cat hello, cd /, ls. Capture with:
    asciinema rec demo.cast
  then upload to asciinema.org and paste the player snippet here.
-->

[![asciicast](https://asciinema.org/a/GkIA1y66Ms6ZYH43.svg)](https://asciinema.org/a/GkIA1y66Ms6ZYH43)

## Quickstart

The fastest way to see it run is the prebuilt release. Building from source needs the AArch64 cross toolchain.

### Option A: run the release binary (no toolchain required)

You only need QEMU:

```bash
brew install qemu                                       # macOS
sudo apt-get install qemu-system-arm                    # Debian / Ubuntu

wget https://github.com/<USER>/<REPO>/releases/latest/download/kernel8.img
qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial stdio -display none
```

A shell prompt should appear within a second or so. Type `help` for the command list.

### Option B: build from source

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

## What's inside

Every feature in the bullet list below is implemented from scratch. There is no `libc`, no Raspberry Pi firmware stub, no third-party bootloader — just the linker script, the cross toolchain, and the code in this repo.

- **Boot sequence.** Parks secondary cores, drops from exception level 2 to exception level 1, sets up the stack, zeroes the BSS section, installs the exception vector table, unmasks interrupts, and hands off to the C++ kernel entry point. (`src/boot.S`)
- **Serial driver.** A driver for the Raspberry Pi's PL011 UART, configured for 115200 baud, 8N1. Used for boot logging, panic messages, and the interactive shell. (`src/uart/`)
- **Interrupts and exception handling.** A 16-entry AArch64 vector table with register-save/restore stubs in assembly, dispatching to C++ handlers. The synchronous-exception handler decodes the exception syndrome register and dumps the fault address, which made the rest of the kernel possible to debug. (`src/interrupts/`)
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

The bigger story — the design decisions I made and the bugs that taught me the most — lives in the project writeup.

<!-- TODO: replace with the actual writeup link once it's published. -->

> _Writeup link goes here._

A few highlights to expect there: chasing a translation fault down to a one-line linker-script fix (the kernel's page tables were quietly being overwritten by uninitialized C++17 inline variables); the multi-day MMU bring-up where the symptom was always "the CPU resets" until I built a fault decoder that could tell me which load instruction was hitting which unmapped address; and the moment I realized that `fs_write` to a fresh file was silently writing to physical address 8 because I had captured a block pointer one statement too early.

## Known limitations

What I plan on working on in the future are:

- **UART receive interrupt is disabled at the controller.** The driver is wired for it, but the interrupt controller mask is left clear to avoid a storm. The shell uses polling reads instead. Proper handling needs a read of the masked-interrupt-status register and an acknowledge to the interrupt-clear register.
- **No `kfree`.** The kernel heap is a pure bump allocator — memory is freed at reboot. The reservation strategy is designed so that a future slab or free-list allocator can sit on the same backing region.
- **No multi-core (SMP) support.** Cores 1–3 are parked in a wait-for-event loop at boot. Only core 0 runs the kernel.
- **No Raspberry Pi 5 port.** The kernel is currently specific to the Pi 3 Model B's BCM2835 peripheral layout and Cortex-A53 specifics.
- **Page tables are not write-execute separated.** Everything is identity-mapped as readable-writable-executable, and the linker emits a single load segment with all three permissions. The split is planned but not done.
- **`free_frame` does not validate its input.** A caller passing a misaligned or out-of-region address can corrupt the bitmap. There is a "bit currently set" guard but no upfront alignment or range check.
- **Intermediate level-2 and level-3 page tables are not reclaimed by `unmap`.** Only the terminal page descriptors are zeroed; the table frames that held them stay allocated.

## References

The PDFs themselves are not in this repository — they are large vendor documents that are freely available from the vendors.

- **Arm Architecture Reference Manual for A-profile architecture (ARM DDI 0487)** — exception levels, the virtual memory architecture (VMSAv8-64), and the generic timer. <https://developer.arm.com/documentation/ddi0487/latest/>
- **Arm Cortex-A53 Technical Reference Manual (ARM DDI 0500)** — the specific core on the Pi 3 Model B. <https://developer.arm.com/documentation/ddi0500/latest/>
- **BCM2835 ARM Peripherals** — the SoC's memory-mapped I/O layout: the PL011 UART, the legacy interrupt controller, the GPIO block. <https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf>

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.
