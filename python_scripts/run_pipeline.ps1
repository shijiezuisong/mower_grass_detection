param(
    [string]$Python = "D:/Program Files/Python313/python.exe",
    [string]$Sparse = "output/tof_features_sparse.jsonl",
    [string]$Medium = "output/tof_features_medium.jsonl",
    [string]$DenseFrames = "output/tof_frames_dense.jsonl",
    [string]$DenseFeatures = "output/tof_features_dense.jsonl",
    [string]$ModelOut = "output/tof_grass_density_model.pkl",
    [string]$PredOut = "output/tof_density_pred_dense.jsonl"
)

$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [string]$Title,
        [string[]]$CommandArgs
    )

    Write-Host $Title
    & $Python @CommandArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Step failed: $Title"
    }
}

if (-not (Test-Path -LiteralPath $Python)) {
    throw "Python not found: $Python"
}

& $Python -c "import sklearn" | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Missing dependency: scikit-learn. Install with: `"$Python`" -m pip install scikit-learn"
}

if (-not (Test-Path -LiteralPath $DenseFrames)) {
    $denseCandidates = Get-ChildItem -Path "output" -Filter "tof_frames_dense_*.jsonl" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending
    if ($denseCandidates -and $denseCandidates.Count -gt 0) {
        $DenseFrames = $denseCandidates[0].FullName
        Write-Host "Dense input not found at default path, using latest capture: $DenseFrames"
    }
}

if (-not (Test-Path -LiteralPath $DenseFrames)) {
    throw "Input file not found: $DenseFrames. Please collect TOF frames first with tof_usart6_receiver.py or run_receiver.ps1"
}

$extractArgs = @(
    "python_scripts/tof_grass_density_features.py",
    "--in", $DenseFrames,
    "--out", $DenseFeatures,
    "--window-size", "15",
    "--label", "dense"
)
Invoke-Step -Title "[1/3] Extract dense features..." -CommandArgs $extractArgs

$trainInputs = @()
if (Test-Path -LiteralPath $Sparse) {
    $trainInputs += $Sparse
} else {
    Write-Host "Warning: optional training input missing: $Sparse"
}

if (Test-Path -LiteralPath $Medium) {
    $trainInputs += $Medium
} else {
    Write-Host "Warning: optional training input missing: $Medium"
}

$trainInputs += $DenseFeatures

$trainArgs = @(
    "python_scripts/tof_grass_density_train.py",
    "--inputs"
) + $trainInputs + @(
    "--model-out", $ModelOut
)
Invoke-Step -Title "[2/3] Train model..." -CommandArgs $trainArgs

$inferArgs = @(
    "python_scripts/tof_grass_density_infer.py",
    "--model", $ModelOut,
    "--in", $DenseFeatures,
    "--out", $PredOut
)
Invoke-Step -Title "[3/3] Run dense inference..." -CommandArgs $inferArgs

Write-Host "Done. Model: $ModelOut  Pred: $PredOut"
