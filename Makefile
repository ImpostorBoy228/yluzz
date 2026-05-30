TARGET = BOOTX64.EFI

CC = gcc
LD = ld

CFLAGS = -O2 -march=native -pipe \
         -ffreestanding -fno-stack-protector -mno-red-zone -fshort-wchar \
         -I/usr/include/efi -I/usr/include/efi/x86_64 \
         -DGNU_EFI_USE_MS_ABI

LDFLAGS = -nostdlib -znocombreloc -T /usr/lib/elf_x86_64_efi.lds \
          -shared -Bsymbolic \
          /usr/lib/crt0-efi-x86_64.o

LIBS = -L/usr/lib -lefi -lgnuefi

all: $(TARGET)

fuck/conf.h: fuck.conf
	printf '#pragma once\n' > $@
	printf '#define CFG_KERNEL_PATH L"%s"\n' $$(grep '^vmlinuz-path:' fuck.conf | sed 's/^vmlinuz-path: *//') >> $@
	printf '#define CFG_INITRD_PATH L"%s"\n' $$(grep '^initramfs-path:' fuck.conf | sed 's/^initramfs-path: *//') >> $@
	printf '#define CFG_CMDLINE_ROOT "%s"\n' $$(grep '^partition:' fuck.conf | sed 's/^partition: *//') >> $@

main.o: fuck/main.c fuck/conf.h
	$(CC) $(CFLAGS) -c fuck/main.c -o main.o

$(TARGET): main.o
	$(LD) $(LDFLAGS) main.o -o $@.elf $(LIBS)
	objcopy -I elf64-x86-64 -O efi-app-x86_64 \
	        -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	        -j .rel -j .rela -j .reloc $@.elf $@

clean:
	rm -f *.o *.EFI *.elf fuck/conf.h
