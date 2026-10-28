$path = "extracted/exefs/code.bin"
$stream = [System.IO.File]::OpenRead($path)
$len = $stream.Length
Write-Host "File size: $len"
$stream.Seek($len - 32, [System.IO.SeekOrigin]::Begin) | Out-Null
$reader = New-Object System.IO.BinaryReader($stream)
$bytes = $reader.ReadBytes(32)
$reader.Close()
$stream.Close()

Write-Host "Hex: " ([BitConverter]::ToString($bytes))
for ($i = 0; $i -lt 8; $i++) {
    $val = [BitConverter]::ToUInt32($bytes, $i * 4)
    $hex = "0x{0:X8}" -f $val
    $off = $len - 32 + ($i * 4)
    Write-Host "Offset $off (End-$((32 - $i*4))): $hex ($val)"
}
