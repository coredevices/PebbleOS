
ENTRY(main)

MEMORY
{
  APP (rwx) : ORIGIN = 0, LENGTH = 24K
}

SECTIONS
{
    .header :
    {
        KEEP(*(.pbl_header))
    } > APP

    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
    } > APP

    .data :
    {
        KEEP(*(.data))
        *(.data.*)
    } > APP

    .bss :
    {
        *(.bss)
        *(.bss.*)
    } > APP

    DISCARD :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
        *(.eh_frame)
    }
}

