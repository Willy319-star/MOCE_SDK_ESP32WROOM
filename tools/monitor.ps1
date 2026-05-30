param(
    [string]$ProjectDir = "",
    [string]$Port = "COM3",
    [int]$Duration = 0
)

$ErrorActionPreference = "Stop"
$SdkRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

if ([string]::IsNullOrWhiteSpace($ProjectDir)) {
    $ProjectDir = Join-Path $SdkRoot "examples/blink_minimal"
}
$ProjectDir = (Resolve-Path $ProjectDir).Path

$IsRedirected = [Console]::IsInputRedirected
if (-not $IsRedirected -and $Duration -le 0) {
    . (Join-Path $SdkRoot "third_party/esp-idf/export.ps1")
    Set-Location $ProjectDir
    idf.py -p $Port monitor
} else {
    . (Join-Path $SdkRoot "third_party/esp-idf/export.ps1") *> $null
    python (Join-Path $SdkRoot "tools/monitor_capture.py") $ProjectDir $Port --duration $Duration
}
