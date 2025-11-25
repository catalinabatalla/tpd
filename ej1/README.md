# TP D: Desarrollo de aplicaciones distribuidas - Parte 1
## Protocolo de Transferencia de Archivos UDP (Stop & Wait)

**Materia:** Redes y Comunicaciones
**Grupo:** 21
**Fecha:** Noviembre 2025

---

### 1. Descripción
Este proyecto implementa un protocolo de transferencia de archivos fiable sobre **UDP** utilizando el mecanismo **Stop & Wait**. El sistema consta de un cliente y un servidor que se comunican a través de sockets BSD, garantizando la entrega ordenada y sin errores de archivos en redes con pérdida de paquetes y latencia variable.

El protocolo cumple estrictamente con las 4 fases especificadas por la cátedra:
1.  **HELLO:** Autenticación del cliente mediante credenciales.
2.  **WRQ:** Solicitud de escritura y validación de nombre de archivo.
3.  **DATA:** Transferencia de datos bloque a bloque con control de flujo (ACKs) y retransmisiones.
4.  **FIN:** Cierre ordenado de la sesión y liberación de recursos.

### 2. Estructura de Directorios

* **Makefile**: Script para la compilación automatizada del proyecto.
* **src/**: Código fuente y recursos.
    * `client.c`: Código del cliente (Manejo de argumentos, máquina de estados, timeouts).
    * `server.c`: Código del servidor (Multiplexación con `select`, manejo concurrente de clientes).
    * `protocol.h`: Definiciones compartidas (Estructura de PDU, Constantes, Tipos).
    * `g21.data`: Archivo oficial de prueba provisto para la validación.
    * `*.pcap`: Evidencias de tráfico capturadas para los distintos escenarios.

### 3. Instrucciones de Compilación

Para compilar ambos programas (cliente y servidor), ubíquese en la raíz de la carpeta `ej1` y ejecute:

```bash
make
