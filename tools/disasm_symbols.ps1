$bytes = [System.IO.File]::ReadAllBytes('extracted/exefs/code_decompressed.bin')

function Dump-Range($startAddr, $count) {
    Write-Host "=== Disassembly at 0x$([Convert]::ToString($startAddr, 16).ToUpper()) ==="
    $fileOffset = $startAddr - 0x00100000
    for ($i = 0; $i -lt $count; $i++) {
        $off = $fileOffset + ($i * 4)
        if ($off + 4 -gt $bytes.Length) { break }
        $u32 = [BitConverter]::ToUInt32($bytes, $off)
        $hex = '0x{0:X8}' -f $u32
        $addr = '0x{0:X8}' -f ($startAddr + ($i * 4))
        Write-Host "$addr : $hex"
    }
}

Dump-Range 0x00127AE0 16
Dump-Range 0x002E5EE0 16
Dump-Range 0x00127D64 16
