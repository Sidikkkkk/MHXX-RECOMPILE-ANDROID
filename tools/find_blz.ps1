$lines = Get-Content 'C:\Users\LENOVO\.gemini\antigravity\brain\e1821814-0e7d-4b1f-964d-f7e5cccf3e19\.system_generated\logs\transcript_full.jsonl'
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match 'DecompressSize' -or $lines[$i] -match 'Decompressed Size: 12996608') {
        Write-Host "Found at line $i"
        # Print lines around it
        $start = [Math]::Max(0, $i - 5)
        $end = [Math]::Min($lines.Count - 1, $i + 2)
        for ($j = $start; $j -le $end; $j++) {
            $obj = $lines[$j] | ConvertFrom-Json
            if ($obj.tool_calls) {
                Write-Host "=== TOOL CALL at $j ==="
                Write-Host $obj.tool_calls[0].args.CommandLine
            }
        }
        break
    }
}

