# yluzz

UEFI chainloader for Linux bzImage. Two boot protocols: EFI handover (CONFIG_EFI_STUB=y) or classic 32-bit with E820 fallback.

### Requirements

- gcc
- gnu-efi
- binutils (ld, objcopy)

```
# Arch
pacman -S gnu-efi
```

### Configure

Edit `fuck.conf`:

```
vmlinuz-path: \\vmlinuz-linux-cachyos
initramfs-path: \\initramfs-linux-cachyos.img
partition: UUID=your-uuid
```

`partition` lands in `root=` on the kernel cmdline. Use UUID not `/dev/sdX`.

### Build

```
make
```

Outputs `BOOTX64.EFI`.

### Install

```
sudo cp BOOTX64.EFI /boot/EFI/BOOT/BOOTX64.EFI
```

Kernel and initrd must be on the same ESP volume as the loader.
