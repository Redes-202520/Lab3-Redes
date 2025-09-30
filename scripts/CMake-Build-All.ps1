<#
Compila todos los targets en Release usando la carpeta ..\cmake-build-release
Uso:
  .\CMake-Build-All.ps1
#>

function Fail($msg) { Write-Error $msg; exit 1 }

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Fail "cmake no esta instalado o no esta en PATH." }

$BuildDir = "..\cmake-build-release"
$Config   = "Release"

# 1) Configurar (multi-config) desde el directorio del proyecto, o sea ..\
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
cmake -S ..\ -B $BuildDir | Out-Null

# 2) Compilar todos los targets en Release
cmake --build $BuildDir --config $Config --parallel

Write-Host ""
Write-Host "Compilacion Release completa en: $BuildDir"
Write-Host ""
