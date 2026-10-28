# ==============================================================================
# MHXX Android Native Port - RomFS Extraction Tool
# Extracts the 2.21 GB RomFS partition from Monster Hunter Double Cross .3ds
# ==============================================================================
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName
$InputsRomDir = Join-Path $ProjectRoot "inputs\rom"
$ExtractedRomFSDir = Join-Path $ProjectRoot "extracted\romfs"
$OutputFile = Join-Path $ExtractedRomFSDir "romfs.bin"

$romFiles = Get-ChildItem -Path $InputsRomDir -Filter "*.3ds" -File
if ($romFiles.Count -eq 0) {
    Write-Host "[!] Tidak ada file .3ds ditemukan di $InputsRomDir" -ForegroundColor Red
    exit 1
}

$romPath = $romFiles[0].FullName
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "       MHXX ANDROID NATIVE PORT - ROMFS EXTRACTOR               " -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "[*] Membaca ROM: $([System.IO.Path]::GetFileName($romPath))" -ForegroundColor Yellow

$stream = [System.IO.File]::OpenRead($romPath)
$reader = New-Object System.IO.BinaryReader($stream)

try {
    # Partisi 0 (NCCH 0)
    $part0Offset = 0x4000
    $stream.Seek($part0Offset + 0x1B0, [System.IO.SeekOrigin]::Begin) | Out-Null
    $romfsOffsetMU = $reader.ReadUInt32()
    $romfsSizeMU = $reader.ReadUInt32()
    $romfsOffset = $part0Offset + ([int64]$romfsOffsetMU * 0x200)
    $romfsSize = [int64]$romfsSizeMU * 0x200

    Write-Host "[+] RomFS Offset : 0x$([Convert]::ToString($romfsOffset, 16).ToUpper()) ($romfsOffset bytes)" -ForegroundColor Green
    Write-Host "[+] RomFS Ukuran : $([math]::Round($romfsSize / 1MB, 2)) MB ($romfsSize bytes)" -ForegroundColor Green

    if (!(Test-Path $ExtractedRomFSDir)) {
        New-Item -ItemType Directory -Path $ExtractedRomFSDir -Force | Out-Null
    }

    Write-Host "[*] Mengekstrak RomFS ke: $OutputFile ..." -ForegroundColor Yellow
    $outStream = [System.IO.File]::Create($OutputFile)
    $stream.Seek($romfsOffset, [System.IO.SeekOrigin]::Begin) | Out-Null

    $bufferSize = [int64](16 * 1024 * 1024) # 16 MB Buffer
    $buffer = New-Object byte[] ([int]$bufferSize)
    $bytesRemaining = [int64]$romfsSize
    $totalWritten = [int64]0

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($bytesRemaining -gt 0) {
        $toRead = [int][Math]::Min($bufferSize, $bytesRemaining)
        $read = $stream.Read($buffer, 0, $toRead)
        if ($read -le 0) { break }
        $outStream.Write($buffer, 0, $read)
        $bytesRemaining -= $read
        $totalWritten += $read

        $percent = [math]::Round(($totalWritten / $romfsSize) * 100, 1)
        Write-Progress -Activity "Ekstraksi RomFS Game" -Status "$percent% ($([math]::Round($totalWritten / 1MB, 1)) MB / $([math]::Round($romfsSize / 1MB, 1)) MB)" -PercentComplete $percent
    }
    $sw.Stop()
    $outStream.Close()

    Write-Host "`n[OK] RomFS berhasil diekstrak dalam $($sw.Elapsed.TotalSeconds.ToString('F1')) detik!" -ForegroundColor Green
    Write-Host "     Ukuran file: $([math]::Round($totalWritten / 1MB, 2)) MB" -ForegroundColor Green
}
finally {
    $reader.Close()
    $stream.Close()
}
