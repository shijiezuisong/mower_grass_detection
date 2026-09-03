param(
    [string]$Python = "D:/Program Files/Python313/python.exe",
    [string]$Port = "",
    [int]$Baud = 921600,
    [string]$Prefix = "dense",
    [ValidateSet("jsonl", "csv", "xlsx")]
    [string]$OutputFormat = "jsonl",
    [Alias("Input")]
    [string]$InputFile = "",
    [string]$PointsOut = "",
    [switch]$Visualize,
    [ValidateSet("auto", "matplotlib", "open3d")]
    [string]$Viewer = "auto",
    [switch]$Loop,
    [double]$Speed = 1.0,
    [int]$MaxFrames = 0
)

$ErrorActionPreference = "Stop"

$outDir = Join-Path $PSScriptRoot "..\output"
$outDir = [System.IO.Path]::GetFullPath($outDir)
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$scriptPath = Join-Path $PSScriptRoot "tof_usart6_receiver.py"
$scriptPath = [System.IO.Path]::GetFullPath($scriptPath)

$cliArgs = @($scriptPath)
if ($InputFile -and $InputFile.Trim().Length -gt 0) {
    $inputPath = [System.IO.Path]::GetFullPath($InputFile)
    $cliArgs += @("--input", $inputPath, "--speed", "$Speed")
    if ($Loop) {
        $cliArgs += "--loop"
    }
} else {
    $ts = Get-Date -Format "yyyyMMdd_HHmmss"
    $outFile = Join-Path $outDir ("tof_frames_{0}_{1}.{2}" -f $Prefix, $ts, $OutputFormat)
    Write-Host "Output file: $outFile"
    $cliArgs += @("--baud", "$Baud", "--out", $outFile)
    if ($Port -and $Port.Trim().Length -gt 0) {
        $cliArgs += @("--port", $Port)
    }
}

if ($PointsOut -and $PointsOut.Trim().Length -gt 0) {
    $pointsPath = [System.IO.Path]::GetFullPath($PointsOut)
    $cliArgs += @("--points-out", $pointsPath)
}
if ($Visualize) {
    $cliArgs += @("--visualize", "--viewer", $Viewer)
}
if ($MaxFrames -gt 0) {
    $cliArgs += @("--max-frames", "$MaxFrames")
}

& $Python @cliArgs
