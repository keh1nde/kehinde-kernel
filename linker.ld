ENTRY(_start)

/* We define two segments to eliminate the RWX vulnerablility,
establish W^X boundaries for future MMU enforcement. */
PHDRS {
  text_seg PT_LOAD FLAGS(5);
  data_seg PT_LOAD FLAGS(6);
}


SECTIONS
{
    /* Starts at LOADER_ADDR. */
    . = 0x80000;
    __start = .;
    __text_start = .;
    .text :
    {
        KEEP(*(.text.boot))
        *(.text)
    }
    : text_seg
    . = ALIGN(4096); /* align to page size */
    __text_end = .;

    __rodata_start = .;
    .rodata :
    {
        *(.rodata .rodata.*)
    }
    : text_seg
    . = ALIGN(4096); /* align to page size */
    __rodata_end = .;

    __data_start = .;
    .data :
    {
        *(.data .data.*)
    }
    : data_seg
    . = ALIGN(4096); /* align to page size */
    __data_end = .;

    __bss_start = .;
    .bss :
    {
        bss = .;
        *(.bss .bss.*)
        *(COMMON)
    }
    : data_seg
    . = ALIGN(4096); /* align to page size */
    __bss_end = .;
    __end = .;
    __kernel_end = .;
}