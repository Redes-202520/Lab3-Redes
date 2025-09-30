#!/usr/bin/env bash
# Compila todos los targets en Release usando la carpeta ../cmake-build-release
# Uso:
#   ./cmake_build_all.sh

set -euo pipefail

command -v cmake >/dev/null 2>&1 || { echo "Error: cmake no esta instalado."; exit 1; }

BUILD_DIR="../cmake-build-release"

# 1) Configurar desde el directorio del proyecto, o sea ../
mkdir -p "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release -S ../ -B "$BUILD_DIR"

# 2) Compilar todos los targets
cmake --build "$BUILD_DIR" --parallel

echo ""
echo "Compilacion del laboratorio 3 completa en: $BUILD_DIR"
echo ""
