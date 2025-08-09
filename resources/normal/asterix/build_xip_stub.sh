arm-none-eabi-gcc -fPIC -mcpu=cortex-m4 -Os -ffreestanding -nostdlib -c xip_stub.c
arm-none-eabi-ld -T xip_stub.lds -o xip_stub.elf xip_stub.o
arm-none-eabi-nm xip_stub.elf
arm-none-eabi-objcopy -Obinary xip_stub.elf xip_stub.bin
