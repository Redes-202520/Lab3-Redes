# Laboratorio 3 - Redes

---

## Tabla de Contenidos

- [Integrantes](#integrantes)
- [Instrucciones de uso](#instrucciones-de-uso)
   - [Compilación automática con CMake](#compilación-automática-con-cmake)
   - [Compilación manual con CMake](#compilación-manual-con-cmake)
   - [Correr los ejecutables](#correr-los-ejecutables)
- [Descripción de los ejecutables](#descripción-de-los-ejecutables)
   - [Ejecutables TCP](#ejecutables-tcp)
   - [Ejecutables UDP](#ejecutables-udp)

---

## Integrantes

| Código    | Nombre           | Login                        |
|-----------|------------------|------------------------------|
| 202222737 | Adrian Velasquez | a.velasquezs@uniandes.edu.co |
| xxx       | xxxx             | xxx@uniandes.edu.co          |
| yyy       | yyy              | yyy@uniandes.edu.co          |

## Instrucciones de uso

Para compilar con CMake necesitas:
- CMake (en Unix/Linux/MacOS se puede instalar con un gestor de paquetes como Homebrew, en Windows se puede descargar desde la página oficial).
- Un compilador C/C++ (toolchain), ej. **MSVC** (Windows/Visual Studio Build Tools), **GCC** o **Clang** (en Unix/Linux/MacOS se pueden instalar con un gestor de paquetes como Homebrew, en Windows se pueden instalar con **MinGW**)
- Un sistema de construcción (generador), ej. **Ninja** (en Unix/Linux/MacOS se puede instalar con un gestor de paquetes como Homebrew, en Windows se puede descargar desde la página oficial).

**IMPORTANTE**: Si se presentan problemas al usar CMake, es posible compilar cada ejecutable por aparte utilizando otras herramientas, y el funcionamiento será equivalente.

### Compilación automática con CMake

1. Clonar el repositorio:
   ```bash
   git clone
    ```
   
2. Navegar al directorio de scripts del proyecto:
    ```bash
   cd Lab3-Redes/scripts
   ```
   
3. Ejecutar el script de instalación automática:
   - En Unix/Linux/MacOS:
     ```bash
     chmod +x cmake_build_all.sh
     ./cmake_build_all.sh
     ```
   - En Windows (PowerShell):
     ```powershell
     .\CMake_Build_All.ps1
     ```
   - **IMPORTANTE**: Los ejecutables quedarán en la carpeta `cmake-build-release`.

### Compilación manual con CMake

1. Clonar el repositorio:
   ```bash
   git clone
    ```
   
2. Navegar al directorio del proyecto:
    ```bash
   cd Lab3-Redes
   ```

3. Compilar el proyecto con CMake (opcional; se puede compilar de otras maneras):
    - En Unix/Linux/MacOS:
      ```bash
      mkdir cmake-build-debug
      cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B ./cmake-build-debug
      ```
    - En Windows (PowerShell):
      ```powershell
      mkdir cmake-build-release
      cmake -S ./ -B ./cmake-build-release
      ```
   
4. Compilar cada ejecutable con CMake (opcional; se puede compilar de otras maneras):
    ```bash
    cmake --build ./cmake-build-debug --parallel
    ```
   - **IMPORTANTE**: Los ejecutables están en la carpeta `cmake-build-release` si se usó CMake de acuerdo a las instrucciones. Si se compiló de otra manera, estarán en la carpeta donde se haya especificado.
   

### Correr los ejecutables:

1. Navegar a la carpeta donde están los ejecutables (si se usó CMake, es `cmake-build-release`):
    ```bash
    cd cmake-build-release
    ```
2. Correr el ejecutable:
    - En Unix/Linux/MacOS:
      ```bash
      ./<nombre_del_ejecutable>
      ```
    - En Windows (PowerShell):
      ```powershell
      .\<nombre_del_ejecutable>.exe
      ```

---

## Descripción de los ejecutables

En general, hay 2 tipos de ejecutables. Los TCP y los UDP. Los TCP tienen el sufijo `_tcp` y los UDP `_udp`.

### Ejecutables TCP
- `publisher_tcp`: Implementa un cliente TCP que se conecta a un servidor y envía mensajes. Puede ser ejecutado múltiples veces.
- `broker_tcp`: Implementa un servidor TCP que acepta conexiones de clientes y reenvía mensajes entre ellos. Solo debe ser ejecutado una vez.
- `subscriber_tcp`: Implementa un cliente TCP que se conecta a un servidor y recibe mensajes. Puede ser ejecutado múltiples veces.

### Ejecutables UDP
- `publisher_udp`: Implementa un cliente UDP que envía mensajes a un servidor. Puede ser ejecutado múltiples veces.
- `broker_udp`: Implementa un servidor UDP que recibe mensajes de clientes y los reenvía a otros clientes. Solo debe ser ejecutado una vez.
- `subscriber_udp`: Implementa un cliente UDP que recibe mensajes de un servidor. Puede ser ejecutado múltiples veces.
