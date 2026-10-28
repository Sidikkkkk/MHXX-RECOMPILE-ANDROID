$bytes = [System.IO.File]::ReadAllBytes('extracted/exefs/code_decompressed.bin')
Write-Host ("Total decompressed bytes: " + $bytes.Length)
for ($i = 0; $i -lt 32; $i++) {
    $offset = $i * 4
    $u32 = [System.BitConverter]::ToUInt32($bytes, $offset)
    $hex = '0x{0:X8}' -f $u32
    $addr = '0x{0:X8}' -f (0x00100000 + $offset)
    Write-Host "$addr : $hex"
}

