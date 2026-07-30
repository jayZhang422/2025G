[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$releaseRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$manifestPath = Join-Path $releaseRoot 'SHA256SUMS.txt'

if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Missing release manifest: $manifestPath"
}

$failures = 0
$checked = 0
foreach ($line in Get-Content -LiteralPath $manifestPath -Encoding UTF8) {
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('#')) {
        continue
    }
    if ($line -notmatch '^([0-9A-Fa-f]{64})  (.+)$') {
        throw "Invalid SHA256SUMS line: $line"
    }

    $expected = $matches[1].ToUpperInvariant()
    $relativePath = $matches[2].Replace('/', [IO.Path]::DirectorySeparatorChar)
    $filePath = Join-Path $releaseRoot $relativePath
    if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
        Write-Host "MISSING  $relativePath" -ForegroundColor Red
        $failures++
        continue
    }

    $actual = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash
    if ($actual -ne $expected) {
        Write-Host "FAILED   $relativePath" -ForegroundColor Red
        Write-Host "  expected $expected"
        Write-Host "  actual   $actual"
        $failures++
    } else {
        Write-Host "OK       $relativePath"
    }
    $checked++
}

$bootImages = @(Get-ChildItem -LiteralPath $releaseRoot -Recurse -File |
    Where-Object { $_.Name -ieq 'BOOT.BIN' })
if ($bootImages.Count -ne 0) {
    $bootImages | ForEach-Object { Write-Host "UNEXPECTED $($_.FullName)" -ForegroundColor Red }
    $failures += $bootImages.Count
}

if ($failures -ne 0) {
    throw "Release verification failed: $failures problem(s), $checked manifest file(s) checked"
}

Write-Host "RELEASE_VERIFY_PASSED files=$checked BOOT_BIN=absent" -ForegroundColor Green
