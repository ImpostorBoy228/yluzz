#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct {
    UINT32 efi_loader_signature;
    UINT32 efi_systab;
    UINT32 efi_memdesc_size;
    UINT32 efi_memdesc_version;
    UINT32 efi_memmap;
    UINT32 efi_memmap_size;
    UINT32 efi_systab_hi;
    UINT32 efi_memmap_hi;
} __attribute__((packed)) efi_info;

typedef struct {
    UINT8  setup_sects;
    UINT16 root_flags;
    UINT32 syssize;
    UINT16 ram_size;
    UINT16 vid_mode;
    UINT16 root_dev;
    UINT16 boot_flag;
    UINT16 jump;
    UINT32 header;
    UINT16 version;
    UINT32 realmode_swtch;
    UINT16 start_sys_seg;
    UINT16 kernel_version;
    UINT8  type_of_loader;
    UINT8  loadflags;
    UINT16 setup_move_size;
    UINT32 code32_start;
    UINT32 ramdisk_image;
    UINT32 ramdisk_size;
    UINT32 bootsect_kludge;
    UINT16 heap_end_ptr;
    UINT8  ext_loader_ver;
    UINT8  ext_loader_type;
    UINT32 cmd_line_ptr;
    UINT32 initrd_addr_max;
    UINT32 kernel_alignment;
    UINT8  relocatable_kernel;
    UINT8  min_alignment;
    UINT16 xloadflags;
    UINT32 cmdline_size;
    UINT32 hardware_subarch;
    UINT64 hardware_subarch_data;
    UINT32 payload_offset;
    UINT32 payload_length;
    UINT64 setup_data;
    UINT64 pref_address;
    UINT32 init_size;
    UINT32 handover_offset;
    UINT32 kernel_info_offset;
} __attribute__((packed)) linux_setup_header;

typedef struct {
    UINT64 addr;
    UINT64 size;
    UINT32 type;
} __attribute__((packed)) e820_entry;

typedef struct {
    UINT8          pad_screen[0x0c0];
    UINT32         ext_ramdisk_image;
    UINT32         ext_ramdisk_size;
    UINT32         ext_cmd_line_ptr;
    UINT8          pad4[0x1c0 - 0x0cc];
    efi_info       efi;
    UINT32         alt_mem_k;
    UINT32         scratch;
    UINT8          e820_entries;
    UINT8          eddbuf_entries;
    UINT8          edd_mbr_sig_buf_entries;
    UINT8          kbd_status;
    UINT8          secure_boot;
    UINT8          pad5[2];
    UINT8          sentinel;
    UINT8          pad6[1];
    linux_setup_header hdr;
    UINT8          pad7[0x290 - 0x1f1 - sizeof(linux_setup_header)];
    UINT32         edd_mbr_sig_buffer[16];
    e820_entry     e820_table[128];
} __attribute__((packed)) boot_params;
