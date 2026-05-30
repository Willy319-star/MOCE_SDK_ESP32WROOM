param(
    [string]$ProjectDir = "",
    [string]$Target = "esp32",
    [string]$Board = ""
)

$ErrorActionPreference = "Stop"
$SdkRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

if ([string]::IsNullOrWhiteSpace($ProjectDir)) {
    $ProjectDir = Join-Path $SdkRoot "examples/blink_minimal"
}
$ProjectDir = (Resolve-Path $ProjectDir).Path

if ([string]::IsNullOrWhiteSpace($Board)) {
    $Board = "my_board_$Target"
}

$SdkconfigDefaults = Join-Path $SdkRoot "boards/$Board/sdkconfig.defaults"
$ProjectSdkconfigDefaults = Join-Path $ProjectDir "sdkconfig.defaults"
if (Test-Path $ProjectSdkconfigDefaults) {
    $SdkconfigDefaults = "$ProjectSdkconfigDefaults;$SdkconfigDefaults"
}

. (Join-Path $SdkRoot "third_party/esp-idf/export.ps1")

Set-Location $ProjectDir

$BuildDir = Join-Path $ProjectDir "build"
if ((Test-Path $BuildDir) -and (!(Test-Path (Join-Path $BuildDir "CMakeCache.txt")) -or !(Test-Path (Join-Path $BuildDir "build.ninja")))) {
    $Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $BackupDir = Join-Path $ProjectDir "build.invalid.$Timestamp"
    Write-Output "Build directory exists but is not a valid CMake build directory."
    Write-Output "Moving it aside: $BuildDir -> $BackupDir"
    Move-Item $BuildDir $BackupDir
}

idf.py "-DMOCE_BOARD=$Board" "-DSDKCONFIG_DEFAULTS=$SdkconfigDefaults" set-target $Target
idf.py "-DMOCE_BOARD=$Board" "-DSDKCONFIG_DEFAULTS=$SdkconfigDefaults" build
