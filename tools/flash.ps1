param(
    [string]$ProjectDir = "",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Rest = @()
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

$Target = ""
$Board = ""
$Port = ""

for ($i = 0; $i -lt $Rest.Count; $i++) {
    $Arg = $Rest[$i]
    switch -Regex ($Arg) {
        '^--target$' {
            $i++
            $Target = if ($i -lt $Rest.Count) { $Rest[$i] } else { "" }
            continue
        }
        '^--board$' {
            $i++
            $Board = if ($i -lt $Rest.Count) { $Rest[$i] } else { "" }
            continue
        }
        '^(--port|-p)$' {
            $i++
            $Port = if ($i -lt $Rest.Count) { $Rest[$i] } else { "" }
            continue
        }
        '^esp32' {
            $Target = $Arg
            continue
        }
        '^my_board_' {
            $Board = $Arg
            continue
        }
        '^(COM|com)\d+|/dev/' {
            $Port = $Arg
            continue
        }
        default {
            if ([string]::IsNullOrWhiteSpace($Port)) {
                $Port = $Arg
            } elseif ([string]::IsNullOrWhiteSpace($Board)) {
                $Board = $Arg
            } else {
                throw "Unknown argument: $Arg"
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Target)) {
    $Sdkconfig = Join-Path $ProjectDir "sdkconfig"
    if (Test-Path -LiteralPath $Sdkconfig) {
        $Match = Select-String -Path $Sdkconfig -Pattern '^CONFIG_IDF_TARGET="([^"]+)"' | Select-Object -First 1
        if ($Match) {
            $Target = $Match.Matches[0].Groups[1].Value
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Target)) {
    $Target = "esp32"
}
if ([string]::IsNullOrWhiteSpace($Board)) {
    $Board = "my_board_$Target"
}
if ([string]::IsNullOrWhiteSpace($Port)) {
    $Port = "COM3"
}

$SdkconfigDefaults = Join-Path $SdkRoot "boards/$Board/sdkconfig.defaults"
$ProjectSdkconfigDefaults = Join-Path $ProjectDir "sdkconfig.defaults"
if (Test-Path -LiteralPath $ProjectSdkconfigDefaults) {
    $SdkconfigDefaults = "$ProjectSdkconfigDefaults;$SdkconfigDefaults"
}

. (Join-Path $SdkRoot "third_party/esp-idf/export.ps1")

Set-Location -LiteralPath $ProjectDir

$CurrentTarget = ""
$Sdkconfig = Join-Path $ProjectDir "sdkconfig"
if (Test-Path -LiteralPath $Sdkconfig) {
    $Match = Select-String -Path $Sdkconfig -Pattern '^CONFIG_IDF_TARGET="([^"]+)"' | Select-Object -First 1
    if ($Match) {
        $CurrentTarget = $Match.Matches[0].Groups[1].Value
    }
}

if ($CurrentTarget -ne $Target) {
    Invoke-Idf "-DMOCE_BOARD=$Board" "-DSDKCONFIG_DEFAULTS=$SdkconfigDefaults" set-target $Target
}

Invoke-Idf "-DMOCE_BOARD=$Board" "-DSDKCONFIG_DEFAULTS=$SdkconfigDefaults" -p $Port flash
