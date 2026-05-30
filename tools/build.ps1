param(
    [string]$ProjectDir = "",
    [string]$Target = "esp32",
    [string]$Board = ""
)

$ErrorActionPreference = "Stop"
$SdkRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path

function Invoke-Idf {
    & idf.py @args
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectDir)) {
    $ProjectDir = Join-Path $SdkRoot "examples/blink_minimal"
}
$ProjectDir = (Resolve-Path -LiteralPath $ProjectDir).Path

if ([string]::IsNullOrWhiteSpace($Board)) {
    $Board = "my_board_$Target"
}

$SdkconfigDefaults = Join-Path $SdkRoot "boards/$Board/sdkconfig.defaults"
$ProjectSdkconfigDefaults = Join-Path $ProjectDir "sdkconfig.defaults"
if (Test-Path -LiteralPath $ProjectSdkconfigDefaults) {
    $SdkconfigDefaults = "$ProjectSdkconfigDefaults;$SdkconfigDefaults"
}

. (Join-Path $SdkRoot "third_party/esp-idf/export.ps1")

Set-Location -LiteralPath $ProjectDir

$BuildDir = Join-Path $ProjectDir "build"
if ((Test-Path -LiteralPath $BuildDir) -and (!(Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt")) -or !(Test-Path -LiteralPath (Join-Path $BuildDir "build.ninja")))) {
    $Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $BackupDir = Join-Path $ProjectDir "build.invalid.$Timestamp"
    Write-Output "Build directory exists but is not a valid CMake build directory."
    Write-Output "Moving it aside: $BuildDir -> $BackupDir"
    Move-Item -LiteralPath $BuildDir -Destination $BackupDir
}

Invoke-Idf "-DMOCE_BOARD=$Board" "-DSDKCONFIG_DEFAULTS=$SdkconfigDefaults" set-target $Target
Invoke-Idf "-DMOCE_BOARD=$Board" "-DSDKCONFIG_DEFAULTS=$SdkconfigDefaults" build
