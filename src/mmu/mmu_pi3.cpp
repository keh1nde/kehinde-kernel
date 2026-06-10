/**
 * @file mmu_pi3.cpp
 * @brief Pi 3 MMU initialization — identity map for BCM2837 memory layout.
 *
 * Part of kehinde-kernel: a bare-metal AArch64 operating system for the
 * Raspberry Pi 3 Model B (Cortex-A53) and Pi 5 (Cortex-A76).
 *
 * Contains mmu_init for the Pi 3. Shared MMU functions (map, translate,
 * unmap) live in mmu.cpp and are compiled for all boards.
 *
 * @author Kehinde Adeoso
 * @copyright 2026 Kehinde Adeoso. SPDX-License-Identifier: MIT
 */
