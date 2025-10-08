# Laboratorio 3 - Redes

## Tabla de Contenidos

- [Integrantes](#integrantes)
- [Instrucciones de uso](#instrucciones-de-uso)
   - [Compilación automática con CMake](#compilación-automática-con-cmake)
   - [Compilación manual con CMake](#compilación-manual-con-cmake)
   - [Correr los ejecutables](#correr-los-ejecutables)
- [Descripción de los ejecutables](#descripción-de-los-ejecutables)
   - [Ejecutables TCP](#ejecutables-tcp)
   - [Ejecutables UDP](#ejecutables-udp)
   - [Ejecutable "main"](#ejecutable-main)
- [Instrucciones detalladas de ejecución](#instrucciones-detalladas-de-ejecución)
   - [Ejecutar Brokers](#ejectuar-brokers)
   - [Ejecutar Subscribers](#ejecutar-subscribers)
   - [Ejecutar Publishers](#ejecutar-publishers)


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

### Ejecutable "main"
- `main`: Ejecutable utilizado para la verificación inicial del entorno de desarrollo. No está relacionado con la funcionalidad de los otros ejecutables.

## Instrucciones detalladas de ejecución

### Ejectuar Brokers
Para ejecutar los brokers, basta con escribir el siguiente comando en la terminal una vez compilado el archivo:  
- En Unix, Linux, MacOS:
```bash
   ./broker_<tcp_o_udp> <puerto_de_ejecucion>
```
- En Windows:
```powershell
   .\broker_<tcp_o_udp>.exe <puerto_de_ejecucion>
```
Una vez ejecutados desde la terminal, el broker quedará corriendo en el puerto asignado (por defecto 5555 para TCP y 5556 para UDP).

### Ejecutar Subscribers
Para ejecutar los subscribers, basta con escribir el siguiente comando en la terminal una vez compilado el archivo:  
- En Unix, Linux, MacOS:
```bash
   ./subscriber_<tcp_o_udp> <ip_del_broker> <puerto_del_broker> <lista_de_temas>
```
- En Windows:
```powershell
   .\subscriber_<tcp_o_udp>.exe <ip_del_broker> <puerto_del_broker> <lista_de_temas>
```
Donde la lista de temas es un listado del estilo ```tema1 tema2 tema3```, temas a los cuales estará suscrito el subscriptor (por defecto se inscribe a "test"). Por defecto, el IP del broker es 127.0.0.1 y el puerto es 5555 (TCP) o 5556 (UDP).

### Ejecutar Publishers
Para ejecutar los publishers, basta con escribir el siguiente comando en la terminal una vez compilado el archivo:  
- En Unix, Linux, MacOS:
```bash
   ./publisher_<tcp_o_udp> <ip_del_broker> <puerto_del_broker> <tema> <tiempo_de_publicacion>
```
- En Windows:
```powershell
   .\publisher_<tcp_o_udp>.exe <ip_del_broker> <puerto_del_broker> <tiempo_de_publicacion>
```
Donde el tema es el tema al cual el publisher va a enviar sus mensajes (por defecto es "test"), y el tiempo de publicación es un entero que representa los milisegundos entre cada publicación. El IP del broker es 127.0.0.1 por defecto, y el puerto es 5555 (TCP) o 5556 (UDP), mientras que el tiempo es de 1000ms.

## Librerías Utilizadas

A continuación se detallan las librerías estándar de C utilizadas en la implementación de los ejecutables TCP y UDP, junto con su propósito y ejemplos de uso en el código:

- arpa/inet.h
  - Proporciona funciones para manipulación y conversión de direcciones IP (por ejemplo, inet_addr, htons, htonl).
  - Uso: Conversión de direcciones y puertos entre formatos de red y host.

- netinet/in.h
  - Define estructuras y constantes para direcciones de dominio Internet, como struct sockaddr_in.
  - Uso: Creación y manejo de direcciones de sockets para TCP y UDP.

- netdb.h
  - Permite la resolución de nombres de host y servicios (getaddrinfo).
  - Uso: Resolución de IP y puerto del broker en los clientes (publisher y subscriber).

- stdio.h
  - Funciones de entrada/salida estándar (printf, fprintf, perror).
  - Uso: Mensajes informativos, errores y depuración en consola.

- stdlib.h
  - Utilidades generales como gestión de memoria (malloc, calloc, free), conversión de cadenas (atoi, strtol) y control de procesos (exit).
  - Uso: Asignación dinámica de memoria para suscripciones y manejo de argumentos.

- string.h
  - Funciones para manipulación de cadenas (strcpy, strncpy, strcmp, strlen, memcpy, memset).
  - Uso: Procesamiento de comandos, temas y payloads.

- sys/socket.h
  - Definiciones y funciones para la creación y manejo de sockets (socket, bind, connect, listen, accept, send, recv, sendto, recvfrom).
  - Uso: Comunicación entre procesos a través de TCP y UDP.

- sys/types.h
  - Tipos de datos usados en llamadas al sistema (size_t, ssize_t, socklen_t).
  - Uso: Definición de variables para tamaños y longitudes en operaciones de red.

- sys/select.h
  - Multiplexación de entrada/salida sobre múltiples descriptores de archivo (select).
  - Uso: Espera y manejo de múltiples conexiones simultáneas en el broker.

- unistd.h
  - Acceso a la API POSIX (close, read, write, sleep, nanosleep).
  - Uso: Cierre de sockets, lectura/escritura y temporización en los publishers.

- errno.h
  - Manejo de errores mediante la variable global errno.
  - Uso: Detección y reporte de errores en operaciones de red y sistema.

- signal.h
  - Manejo de señales del sistema (signal, SIGPIPE).
  - Uso: Ignorar SIGPIPE en el broker TCP para evitar terminación al escribir en sockets cerrados.

- time.h
  - Funciones relacionadas con el tiempo (time, nanosleep).
  - Uso: Generación de timestamps en los mensajes y control de intervalos de publicación.


Todas las funcionalidades de red, manejo de memoria y procesamiento de cadenas se implementan exclusivamente con librerías estándar de C, sin dependencias externas. Esto garantiza portabilidad y simplicidad en la compilación y ejecución en cualquier sistema compatible con C.