#include "jopa/boot.h"
#include "conf.h"

static void fill_e820(boot_params *bp, EFI_MEMORY_DESCRIPTOR *MemMap,
                       UINTN MapSize, UINTN DescSize)
{
    UINTN entries = MapSize / DescSize;
    if (entries > 128) entries = 128;

    for (UINTN i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR *d =
            (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + i * DescSize);
        bp->e820_table[i].addr = d->PhysicalStart;
        bp->e820_table[i].size = d->NumberOfPages * 4096;

        switch (d->Type) {
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiConventionalMemory:
            bp->e820_table[i].type = 1;
            break;
        case EfiACPIReclaimMemory:
            bp->e820_table[i].type = 3;
            break;
        case EfiACPIMemoryNVS:
            bp->e820_table[i].type = 4;
            break;
        default:
            bp->e820_table[i].type = 2;
            break;
        }
    }
    bp->e820_entries = (UINT8)entries;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle,
                           EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    EFI_STATUS Status;

    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
    EFI_FILE *RootDir;
    EFI_FILE *KernelFile;

    UINTN FileSize;
    VOID *KernelBuffer;

    Status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImage
    );

    if (EFI_ERROR(Status))
        goto naxyi;

    Status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&Fs
    );

    if (EFI_ERROR(Status))
        goto naxyi;

    Status = uefi_call_wrapper(
        Fs->OpenVolume,
        2,
        Fs,
        &RootDir
    );

    if (EFI_ERROR(Status))
        goto naxyi;

    Status = uefi_call_wrapper(
        RootDir->Open,
        5,
        RootDir,
        &KernelFile,
        CFG_KERNEL_PATH,
        EFI_FILE_MODE_READ,
        0
    );

    if (EFI_ERROR(Status))
        goto naxyi;

    EFI_FILE_INFO *FileInfo = NULL;
    UINTN InfoSize = 0;

    KernelFile->GetInfo(
        KernelFile,
        &gEfiFileInfoGuid,
        &InfoSize,
        NULL
    );

    FileInfo = AllocatePool(InfoSize);

    KernelFile->GetInfo(
        KernelFile,
        &gEfiFileInfoGuid,
        &InfoSize,
        FileInfo
    );

    FileSize = FileInfo->FileSize;

    FreePool(FileInfo);

    EFI_PHYSICAL_ADDRESS KernelAddress;
    EFI_PHYSICAL_ADDRESS candidates[] = {
        0x10000000, 0x8000000, 0x4000000, 0x2000000, 0x1000000
    };
    Status = EFI_NOT_FOUND;
    for (int i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        KernelAddress = candidates[i];
        Status = uefi_call_wrapper(
            SystemTable->BootServices->AllocatePages,
            4,
            AllocateAddress,
            EfiLoaderData,
            EFI_SIZE_TO_PAGES(FileSize),
            &KernelAddress
        );
        if (!EFI_ERROR(Status)) break;
    }
    if (EFI_ERROR(Status)) {
        KernelAddress = 0;
        Status = uefi_call_wrapper(
            SystemTable->BootServices->AllocatePages,
            4,
            AllocateAnyPages,
            EfiLoaderData,
            EFI_SIZE_TO_PAGES(FileSize),
            &KernelAddress
        );
    }
    if (EFI_ERROR(Status))
        goto naxyi;
    if (KernelAddress < 0x100000)
        goto naxyi;

    KernelBuffer = (VOID*)(UINTN)KernelAddress;

    Status = uefi_call_wrapper(
        KernelFile->Read,
        3,
        KernelFile,
        &FileSize,
        KernelBuffer
    );

    KernelFile->Close(KernelFile);

    if (EFI_ERROR(Status))
        goto naxyi;

    linux_setup_header *Setup =
        (linux_setup_header*)((UINT8*)KernelBuffer + 0x1f1);

    if (Setup->header != 0x53726448)
        goto naxyi;

    UINTN setup_sects = Setup->setup_sects;
    if (setup_sects == 0)
        setup_sects = 4;

    UINTN setup_size = (setup_sects + 1) * 512;

    EFI_PHYSICAL_ADDRESS BpAddr = 0xFFFFFFFF;

    Status = uefi_call_wrapper(
        SystemTable->BootServices->AllocatePages,
        4,
        AllocateMaxAddress,
        EfiLoaderData,
        2,
        &BpAddr
    );

    if (EFI_ERROR(Status)) {
        BpAddr = 0;
        Status = uefi_call_wrapper(
            SystemTable->BootServices->AllocatePages,
            4,
            AllocateAnyPages,
            EfiLoaderData,
            2,
            &BpAddr
        );
    }

    if (EFI_ERROR(Status))
        goto naxyi;

    boot_params *bp = (boot_params*)(UINTN)BpAddr;

    SetMem(bp, 0x2000, 0);

    CopyMem(&bp->hdr,
            Setup,
            sizeof(linux_setup_header));

    CHAR8 *cmdline =
        (CHAR8*)((UINT8*)bp + 0x1000);

    CHAR8 *src =
        (CHAR8*)"root=" CFG_CMDLINE_ROOT " rw console=tty0 loglevel=7";

    CHAR8 *dst = cmdline;

    while (*src)
        *dst++ = *src++;

    *dst = 0;

    bp->hdr.cmd_line_ptr =
        (UINT32)(UINTN)cmdline;

    bp->hdr.type_of_loader = 0xFF;
    bp->hdr.loadflags |= 0x80;

    EFI_FILE *InitrdFile = NULL;
    UINTN InitrdSize = 0;

    Status = uefi_call_wrapper(
        RootDir->Open,
        5,
        RootDir,
        &InitrdFile,
        CFG_INITRD_PATH,
        EFI_FILE_MODE_READ,
        0
    );

    if (!EFI_ERROR(Status)) {
        EFI_FILE_INFO *InitrdInfo = NULL;
        UINTN InitrdInfoSize = 0;

        InitrdFile->GetInfo(InitrdFile, &gEfiFileInfoGuid, &InitrdInfoSize, NULL);
        InitrdInfo = AllocatePool(InitrdInfoSize);
        InitrdFile->GetInfo(InitrdFile, &gEfiFileInfoGuid, &InitrdInfoSize, InitrdInfo);
        InitrdSize = InitrdInfo->FileSize;
        FreePool(InitrdInfo);

        EFI_PHYSICAL_ADDRESS InitrdAddr = 0xFFFFFFFF;
        Status = uefi_call_wrapper(
            SystemTable->BootServices->AllocatePages,
            4,
            AllocateMaxAddress,
            EfiLoaderData,
            EFI_SIZE_TO_PAGES(InitrdSize),
            &InitrdAddr
        );

        if (!EFI_ERROR(Status)) {
            uefi_call_wrapper(InitrdFile->Read, 3, InitrdFile, &InitrdSize, (VOID*)(UINTN)InitrdAddr);
            bp->hdr.ramdisk_image = (UINT32)InitrdAddr;
            bp->hdr.ramdisk_size = (UINT32)InitrdSize;
            bp->hdr.ext_ramdisk_image = (UINT32)((UINT64)InitrdAddr >> 32);
            bp->hdr.ext_ramdisk_size = (UINT32)((UINT64)InitrdSize >> 32);
            // Print(L"initrd ok\n\r");
        }

        InitrdFile->Close(InitrdFile);
    }

    UINTN MapKey;
    UINTN MapSize = 0;
    UINTN DescSize;
    UINT32 DescVer;

    SystemTable->BootServices->GetMemoryMap(
        &MapSize,
        NULL,
        &MapKey,
        &DescSize,
        &DescVer
    );

    MapSize += DescSize * 2;

    EFI_MEMORY_DESCRIPTOR *MemMap =
        AllocatePool(MapSize);

    Status = SystemTable->BootServices->GetMemoryMap(
        &MapSize,
        MemMap,
        &MapKey,
        &DescSize,
        &DescVer
    );

    if (EFI_ERROR(Status))
        goto naxyi;

    bp->efi.efi_loader_signature = 0x41544F4D;
    bp->efi.efi_systab = (UINT32)(UINTN)SystemTable;
    bp->efi.efi_systab_hi = (UINT32)((UINT64)(UINTN)SystemTable >> 32);
    bp->efi.efi_memdesc_size = DescSize;
    bp->efi.efi_memdesc_version = DescVer;
    bp->efi.efi_memmap = (UINT32)(UINTN)MemMap;
    bp->efi.efi_memmap_hi = (UINT32)((UINT64)(UINTN)MemMap >> 32);
    bp->efi.efi_memmap_size = MapSize;

    UINT64 entry;
    int handover = (Setup->version >= 0x20d && Setup->handover_offset != 0);

    if (handover) {
        entry = (UINT64)KernelBuffer + Setup->handover_offset;
    } else {
        entry = (UINT64)KernelBuffer + setup_size;

        fill_e820(bp, MemMap, MapSize, DescSize);

        while (1)
        {
            Status = SystemTable->BootServices->ExitBootServices(
                ImageHandle,
                MapKey
            );

            if (!EFI_ERROR(Status))
                break;

            Status = SystemTable->BootServices->GetMemoryMap(
                &MapSize,
                MemMap,
                &MapKey,
                &DescSize,
                &DescVer
            );

            if (EFI_ERROR(Status))
                goto naxyi;

            fill_e820(bp, MemMap, MapSize, DescSize);
        }
    }

    if (!bp || !entry || entry < 0x100000)
        goto naxyi;

    __asm__ volatile (
        "jmp *%0\n"
        :
        : "r"(entry), "S"(bp), "d"(SystemTable)
        : "memory"
    );

naxyi:

    while (1)
        __asm__("hlt");

    return EFI_SUCCESS;
}
