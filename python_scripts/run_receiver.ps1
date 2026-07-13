param(
    [string]$Python = "D:/Program Files/Python313/python.exe",
    [string]$Port = "",
    [int]$Baud = 921600,
    [string]$Prefix = "dense"
)

$ErrorActionPreference = "Stop"

$outDir = Join-Path $PSScriptRoot "..\output"
$outDir = [System.IO.Path]::GetFullPath($outDir)
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$outFile = Join-Path $outDir ("tof_frames_{0}_{1}.jsonl" -f $Prefix, $ts)

Write-Host "Output file: $outFile"

$scriptPath = Join-Path $PSScriptRoot "tof_usart6_receiver.py"
$scriptPath = [System.IO.Path]::GetFullPath($scriptPath)

$args = @($scriptPath, "--baud", "$Baud", "--out", $outFile)
if ($Port -and $Port.Trim().Length -gt 0) {
    $args += @("--port", $Port)
}

& $Python @args
