param(
    [ValidateSet("build", "flash_dap", "flash_jlink", "debug_ozone")]
    [string]$Task = "build",
    [string]$BuildDir = "cmake-build-debug",
    [string]$Generator = "Ninja"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$BuildPath = Join-Path $ProjectRoot $BuildDir

cmake -S $ProjectRoot -B $BuildPath -G $Generator

switch ($Task) {
    "build" {
        cmake --build $BuildPath
    }
    "flash_dap" {
        cmake --build $BuildPath --target flash_dap
    }
    "flash_jlink" {
        cmake --build $BuildPath --target flash_jlink
    }
    "debug_ozone" {
        cmake --build $BuildPath
        ozone "$ProjectRoot\debug_ozone.jdebug"
    }
}
