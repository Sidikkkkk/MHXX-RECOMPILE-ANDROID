# ==============================================================================
# MHXX Android Native Port - codebin2elf (Converts decompressed code.bin to ELF)
# Target Architecture: ARMv6K / ARMv7-A 32-bit Little Endian
# ==============================================================================
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName
$DecompressedCodePath = Join-Path $ProjectRoot "extracted\exefs\code_decompressed.bin"
$OutputElfPath = Join-Path $ProjectRoot "extracted\exefs\mhxx.elf"

if (!(Test-Path $DecompressedCodePath)) {
    Write-Host "[!] File code_decompressed.bin tidak ditemukan di extracted\exefs!" -ForegroundColor Red
    exit 1
}

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "       MHXX ANDROID NATIVE PORT - CODE.BIN TO ELF BUILDER        " -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan

$codeBytes = [System.IO.File]::ReadAllBytes($DecompressedCodePath)

# ExHeader Memory Layout Parameters (Rabbit_R / MHXX)
$textVAddr = 0x00100000
$textSize  = 11425536     # 0x00AE4700

$rodataVAddr = 0x00BE6000
$rodataSize  = 1302300    # 0x0013DF1C

$dataVAddr = 0x00D24000
$dataSize  = 265184       # 0x00040C00

$bssVAddr = 0x00D65000
$bssSize  = 468324        # 0x00072564

$entryPoint = 0x00100000

# File offsets within code_decompressed.bin
$textOffsetInFile = 0
$rodataOffsetInFile = $textSize
# Pad rodata file offset to 4096 alignment
$rodataOffsetInFile = [int](([math]::Ceiling($textSize / 4096.0)) * 4096)
$dataOffsetInFile = [int](([math]::Ceiling(($rodataOffsetInFile + $rodataSize) / 4096.0)) * 4096)

Write-Host "[*] Memetakan segmen memori ELF:" -ForegroundColor Yellow
Write-Host "    .text   : VAddr 0x$([Convert]::ToString($textVAddr, 16).ToUpper()) | Size: $([math]::Round($textSize/1MB, 2)) MB (Flags: R-X)" -ForegroundColor Green
Write-Host "    .rodata : VAddr 0x$([Convert]::ToString($rodataVAddr, 16).ToUpper()) | Size: $([math]::Round($rodataSize/1MB, 2)) MB (Flags: R--)" -ForegroundColor Green
Write-Host "    .data   : VAddr 0x$([Convert]::ToString($dataVAddr, 16).ToUpper()) | Size: $([math]::Round($dataSize/1MB, 2)) MB (Flags: RW-)" -ForegroundColor Green
Write-Host "    .bss    : VAddr 0x$([Convert]::ToString($bssVAddr, 16).ToUpper()) | Size: $([math]::Round($bssSize/1MB, 2)) MB (Flags: RW-)" -ForegroundColor Green
Write-Host "    Entry   : 0x$([Convert]::ToString($entryPoint, 16).ToUpper())" -ForegroundColor Cyan

$stream = [System.IO.File]::Create($OutputElfPath)
$writer = New-Object System.IO.BinaryWriter($stream)

try {
    # -------------------------------------------------------------
    # ELF 32-bit Header (52 bytes)
    # -------------------------------------------------------------
    $writer.Write([byte[]]@(0x7F, 0x45, 0x4C, 0x46)) # e_ident[EI_MAG0..3]: \x7FELF
    $writer.Write([byte]1)                           # EI_CLASS: 1 = 32-bit
    $writer.Write([byte]1)                           # EI_DATA: 1 = Little Endian
    $writer.Write([byte]1)                           # EI_VERSION: 1 = Current
    $writer.Write([byte]0)                           # EI_OSABI: 0 = System V
    $writer.Write([byte]0)                           # EI_ABIVERSION: 0
    $writer.Write([byte[]](New-Object byte[] 7))     # EI_PAD (7 bytes zeros)

    $writer.Write([uint16]2)                         # e_type: 2 = ET_EXEC (Executable)
    $writer.Write([uint16]40)                        # e_machine: 40 = EM_ARM
    $writer.Write([uint32]1)                         # e_version: 1
    $writer.Write([uint32]$entryPoint)               # e_entry
    $writer.Write([uint32]52)                        # e_phoff: 52 (Program Header right after ELF Header)
    $writer.Write([uint32]0)                         # e_shoff: 0 (No Section Headers needed for raw execution)
    $writer.Write([uint32]0x05000000)                # e_flags: ARM EABI version 5
    $writer.Write([uint16]52)                        # e_ehsize: 52 bytes
    $writer.Write([uint16]32)                        # e_phentsize: 32 bytes per program header entry
    $writer.Write([uint16]4)                         # e_phnum: 4 program headers (.text, .rodata, .data, .bss)
    $writer.Write([uint16]0)                         # e_shentsize: 0
    $writer.Write([uint16]0)                         # e_shnum: 0
    $writer.Write([uint16]0)                         # e_shstrndx: 0

    # -------------------------------------------------------------
    # Program Headers (4 entries x 32 bytes = 128 bytes)
    # Total header size: 52 + 128 = 180 bytes -> pad to 4096 (0x1000)
    # -------------------------------------------------------------
    $headersEnd = 52 + (4 * 32)
    $fileBodyOffset = 0x1000

    function Write-ProgHeader($type, $offset, $vaddr, $filesz, $memsz, $flags, $align) {
        $writer.Write([uint32]$type)   # p_type: 1 = PT_LOAD
        $writer.Write([uint32]$offset) # p_offset
        $writer.Write([uint32]$vaddr)  # p_vaddr
        $writer.Write([uint32]$vaddr)  # p_paddr
        $writer.Write([uint32]$filesz) # p_filesz
        $writer.Write([uint32]$memsz)  # p_memsz
        $writer.Write([uint32]$flags)  # p_flags (1=X, 2=W, 4=R)
        $writer.Write([uint32]$align)  # p_align
    }

    $elfTextOffset = $fileBodyOffset
    $elfRodataOffset = $elfTextOffset + $textOffsetInFile + $textSize
    $elfDataOffset = $elfRodataOffset + $rodataSize

    # 1. .text (Flags: R | X = 4 | 1 = 5)
    Write-ProgHeader 1 $elfTextOffset $textVAddr $textSize $textSize 5 0x1000

    # 2. .rodata (Flags: R = 4)
    Write-ProgHeader 1 $elfRodataOffset $rodataVAddr $rodataSize $rodataSize 4 0x1000

    # 3. .data (Flags: R | W = 4 | 2 = 6)
    Write-ProgHeader 1 $elfDataOffset $dataVAddr $dataSize $dataSize 6 0x1000

    # 4. .bss (Flags: R | W = 4 | 2 = 6, filesz = 0, memsz = bssSize)
    Write-ProgHeader 1 $elfDataOffset $bssVAddr 0 $bssSize 6 0x1000

    # Pad until 0x1000
    $padLength = $fileBodyOffset - $headersEnd
    $writer.Write([byte[]](New-Object byte[] $padLength))

    # Write decompressed code body
    $writer.Write($codeBytes)

    Write-Host "[OK] File ELF mhxx.elf berhasil dibuat!" -ForegroundColor Green
    Write-Host "     Lokasi: $OutputElfPath ($([math]::Round($stream.Length / 1MB, 2)) MB)" -ForegroundColor Green
}
finally {
    $writer.Close()
    $stream.Close()
}

