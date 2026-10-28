# ==============================================================================
# MHXX Android Native Port - Game Asset & Binary Extraction Pipeline (PowerShell)
# ==============================================================================
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName
$InputsRomDir = Join-Path $ProjectRoot "inputs\rom"
$InputsUpdateDir = Join-Path $ProjectRoot "inputs\updates"
$ExtractedExeFSDir = Join-Path $ProjectRoot "extracted\exefs"
$ExtractedRomFSDir = Join-Path $ProjectRoot "extracted\romfs"

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "       MHXX ANDROID NATIVE PORT - ASSET & CODE EXTRACTOR         " -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "Project Directory: $ProjectRoot`n" -ForegroundColor DarkGray

Write-Host "[*] Memeriksa folder: $InputsRomDir" -ForegroundColor Yellow
$romFiles = Get-ChildItem -Path $InputsRomDir -Include *.3ds, *.cia, *.cxi -Recurse -File -ErrorAction SilentlyContinue

if ($romFiles.Count -eq 0) {
    Write-Host "    [!] PERINGATAN: Tidak ada file ROM (.3ds / .cia) ditemukan di 'inputs\rom\'!" -ForegroundColor Red
    Write-Host "    --> Silakan letakkan file ROM MHXX Anda di dalam folder tersebut.`n" -ForegroundColor White
} else {
    foreach ($f in $romFiles) {
        $sizeMB = [math]::Round($f.Length / 1MB, 2)
        Write-Host "    [+] Ditemukan ROM : $($f.Name) ($sizeMB MB)" -ForegroundColor Green
    }
}

Write-Host "`n[*] Memeriksa folder: $InputsUpdateDir" -ForegroundColor Yellow
$updateFiles = Get-ChildItem -Path $InputsUpdateDir -Include *.cia -Recurse -File -ErrorAction SilentlyContinue

if ($updateFiles.Count -eq 0) {
    Write-Host "    [-] Tidak ada file update ditemukan (Opsional, tapi disarankan v1.4.0)." -ForegroundColor Gray
} else {
    foreach ($f in $updateFiles) {
        $sizeMB = [math]::Round($f.Length / 1MB, 2)
        Write-Host "    [+] Ditemukan Update: $($f.Name) ($sizeMB MB)" -ForegroundColor Green
    }
}

if ($romFiles.Count -eq 0) {
    Write-Host "`n[PANDUAN LOKASI PENYIMPANAN FILE ANDA]" -ForegroundColor Cyan
    Write-Host "1. Salin file 'MHXX.3ds' atau 'MHXX.cia' Anda ke dalam folder:" -ForegroundColor White
    Write-Host "   -> $InputsRomDir" -ForegroundColor Green
    Write-Host "2. (Opsional) Salin file update 'v1.4.0.cia' ke:" -ForegroundColor White
    Write-Host "   -> $InputsUpdateDir" -ForegroundColor Green
    Write-Host "3. Jalankan kembali perintah ini:`n   powershell -ExecutionPolicy Bypass -File .\tools\extract_game.ps1`n" -ForegroundColor Yellow
    exit 0
}

# Extraction function for .3ds NCSD container
function Extract-NCSD([string]$filePath) {
    Write-Host "`n[+] Memproses ROM Cartridge 3DS: $([System.IO.Path]::GetFileName($filePath))" -ForegroundColor Cyan
    $stream = [System.IO.File]::OpenRead($filePath)
    $reader = New-Object System.IO.BinaryReader($stream)

    try {
        # Check NCSD magic at 0x100
        $stream.Seek(0x100, [System.IO.SeekOrigin]::Begin) | Out-Null
        $magic = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        if ($magic -ne "NCSD") {
            Write-Host "    [!] File ini bukan biner cartridge NCSD 3DS yang valid." -ForegroundColor Red
            return
        }

        # Partition 0 offset (Media Unit size = 0x200 bytes)
        $stream.Seek(0x120, [System.IO.SeekOrigin]::Begin) | Out-Null
        $part0OffsetMU = $reader.ReadUInt32()
        $part0Offset = [int64]$part0OffsetMU * 0x200

        Write-Host "    [+] Partisi Utama Game (NCCH 0) offset: 0x$([Convert]::ToString($part0Offset, 16).ToUpper())" -ForegroundColor Gray

        # Check NCCH magic at part0Offset + 0x100
        $stream.Seek($part0Offset + 0x100, [System.IO.SeekOrigin]::Begin) | Out-Null
        $ncchMagic = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        if ($ncchMagic -ne "NCCH") {
            Write-Host "    [!] Header NCCH partisi game tidak valid!" -ForegroundColor Red
            return
        }

        # Check encryption flag at part0Offset + 0x188
        $stream.Seek($part0Offset + 0x188, [System.IO.SeekOrigin]::Begin) | Out-Null
        $flags = $reader.ReadBytes(8)
        $isNoCrypto = ($flags[7] -band 0x04) -ne 0
        $isZeroKey = ($flags[7] -band 0x01) -ne 0

        if (-not ($isNoCrypto -or $isZeroKey)) {
            Write-Host "    [X] TERENKRIPSI! ROM memerlukan dekripsi menggunakan Decrypt9WIP / GodMode9 pada konsol 3DS." -ForegroundColor Red
            return
        }
        Write-Host "    [*] Status Enkripsi: ROM Didekripsi (Decrypted)" -ForegroundColor Green

        # ExeFS offset and size
        $stream.Seek($part0Offset + 0x1A0, [System.IO.SeekOrigin]::Begin) | Out-Null
        $exefsOffsetMU = $reader.ReadUInt32()
        $exefsOffset = $part0Offset + ([int64]$exefsOffsetMU * 0x200)

        Write-Host "    [+] Membaca ExeFS Header pada offset 0x$([Convert]::ToString($exefsOffset, 16).ToUpper())..." -ForegroundColor Gray

        if (!(Test-Path $ExtractedExeFSDir)) {
            New-Item -ItemType Directory -Path $ExtractedExeFSDir -Force | Out-Null
        }

        # Read 10 file entries in ExeFS header
        $stream.Seek($exefsOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $headerBytes = $reader.ReadBytes(0x200)

        for ($i = 0; $i -lt 10; $i++) {
            $offset = $i * 16
            $nameBytes = $headerBytes[$offset..($offset + 7)]
            $nameStr = [System.Text.Encoding]::ASCII.GetString($nameBytes).Trim([char]0)

            if ([string]::IsNullOrWhiteSpace($nameStr)) { continue }

            $fOffset = [System.BitConverter]::ToUInt32($headerBytes, $offset + 8)
            $fSize = [System.BitConverter]::ToUInt32($headerBytes, $offset + 12)

            $dataOffset = $exefsOffset + 0x200 + $fOffset
            $stream.Seek($dataOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
            $content = $reader.ReadBytes($fSize)

            $destPath = Join-Path $ExtractedExeFSDir "$nameStr.bin"
            [System.IO.File]::WriteAllBytes($destPath, $content)

            Write-Host "        -> Berhasil mengekstrak $nameStr.bin ($($content.Length) bytes)" -ForegroundColor Green
        }

        Write-Host "`n    [OK] Ekstraksi ExeFS berhasil! 'code.bin' siap di: $ExtractedExeFSDir\code.bin" -ForegroundColor Green
    }
    finally {
        $reader.Close()
        $stream.Close()
    }
}

foreach ($rom in $romFiles) {
    if ($rom.Extension -eq ".3ds") {
        Extract-NCSD $rom.FullName
    }
}

