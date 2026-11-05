# Builds the Peltier custom chip to WebAssembly for the Wokwi simulator.
#
# Needs a wasi-sdk toolchain. Pin version 25, not the latest: wasi-sdk 34
# dropped the wasm32-wasi target alias that Wokwi's build line uses.
#
#   1. Download wasi-sdk-25.0-x86_64-windows.tar.gz from
#      https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-25
#   2. Extract it anywhere, e.g. C:\wasi-sdk-25.0
#   3. Point WASI_SDK_PATH at it, or pass -WasiSdk, and run this script.
#
# The resulting peltier.chip.wasm is committed, so the simulation runs without
# the toolchain installed; you only need this when the chip source changes.

param(
    [string]$WasiSdk = $env:WASI_SDK_PATH
)

$ErrorActionPreference = 'Stop'
$chipDir = $PSScriptRoot

if (-not $WasiSdk) {
    $WasiSdk = 'C:\wasi-sdk-25.0'
    Write-Host "WASI_SDK_PATH not set, trying $WasiSdk"
}

$clang = Join-Path $WasiSdk 'bin\clang.exe'
$sysroot = Join-Path $WasiSdk 'share\wasi-sysroot'

if (-not (Test-Path $clang)) {
    throw "clang not found at $clang. Set WASI_SDK_PATH or pass -WasiSdk <path>."
}
if (-not (Test-Path $sysroot)) {
    throw "wasi-sysroot not found at $sysroot."
}

$source = Join-Path $chipDir 'peltier.chip.c'
$output = Join-Path $chipDir 'peltier.chip.wasm'

& $clang `
    --target=wasm32-unknown-wasi `
    --sysroot $sysroot `
    -nostartfiles `
    -Wl,--import-memory `
    -Wl,--export-table `
    -Wl,--no-entry `
    -Werror `
    -O2 `
    -o $output `
    $source

if ($LASTEXITCODE -ne 0) {
    throw "clang failed with exit code $LASTEXITCODE"
}

$size = (Get-Item $output).Length
Write-Host "Built $output ($size bytes)"
