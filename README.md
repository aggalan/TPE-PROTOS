# SOCKS5d – Proxy SOCKS5 mínimo y suite de gestión

Un proxy **SOCKS v5** escrito en C que admite autenticación opcional (usuario / contraseña), configuración en caliente mediante un puerto de administración dedicado y un **cliente de administración** interactivo.

---

## Características

* Proxy **SOCKS v5** con soporte IPv4/IPv6 y DNS remoto (`*h`).
* Hasta **10 usuarios simultáneos** con autenticación individual.
* **Servicio de gestión** en un puerto UDP independiente

  * Métricas en tiempo real
  * Alta / baja de usuarios sin reiniciar el demonio
  * Activar o desactivar autenticación en caliente
  * Consulta y limpieza de registros de acceso
* **CLI de administración** para interactuar (autocompletado y ayuda integrada).
* Despliegue sencillo: **un único binario** (`socks5d`).
* Modo **debug** opcional con logging detallado.

---

## Compilación

```bash
# Clonar el repositorio y luego:
$ make                    # genera ./socks5d 
$ make admin_client       # genera ./admin_client
```

Dependencias:

* Sistema **POSIX** (probado en macOS y Linux)
* `gcc` / `clang` con soporte C99
* `make`, `pkg-config`
* (Opcional) librería [`check`](https://libcheck.github.io/check/) para tests unitarios

---

## Uso — `socks5d`

```text
Uso: socks5d [OPCIÓN]...

   -h               Imprime la ayuda y termina.
   -l <dir SOCKS>   Dirección donde escuchará el proxy SOCKS.
   -L <dir conf>    Dirección donde escuchará el servicio de gestión.
   -p <puerto S>    Puerto para conexiones SOCKS.
   -P <puerto C>    Puerto para conexiones de configuración.
   -u <user>:<pass> Usuario y contraseña autorizados (máx. 10).
   -v               Muestra la versión y termina.
```

### Inicio rápido (sin autenticación)

```bash
# Escucha en 0.0.0.0:1080 y publica la gestión en 127.0.0.1:8080
$ ./socks5d -l 0.0.0.0 -p 1080 -L 127.0.0.1 -P 8080
```

### Inicio rápido (con autenticación)

```bash
# Añade dos usuarios al arrancar
$ ./socks5d -u alice:s3cr3t -u bob:pwd1234
```

---

## Uso — `admin_client`

Con el demonio en ejecución, abre el cliente de administración apuntando al puerto de gestión(por defecto, sin colocar nada usamos 127.0.0.1 8080):

```bash
$ ./admin_client <host> <puerto>
```
login: > alice s3cr3t
```

### Comandos interactivos

| Comando           | Descripción                                                          |                                                     |
| ----------------- | -------------------------------------------------------------------- | --------------------------------------------------- |
| `stats`           | Muestra métricas del proxy (conexiones totales, bytes, concurrencia) |                                                     |
| `listusers`       | Lista los usuarios configurados                                      |                                                     |
| `adduser <u> <p>` | Agrega un usuario con contraseña                                     |                                                     |
| `deluser <u>`     | Elimina un usuario                                                   |                                                     |
| \`setauth enabled | disabled\`                                                           | Habilita o deshabilita la autenticación obligatoria |
| `login <u> <p>`   | Autentica la sesión de administración actual                         |                                                     |
| `dump [N]`        | Muestra las últimas *N* líneas del log (por defecto 10)              |                                                     |
| `searchlogs <u>`  | Filtra el log por nombre de usuario                                  |                                                     |
| `clearlogs`       | Borra todos los registros                                            |                                                     |
| `help`            | Muestra esta ayuda                                                   |                                                     |
| `exit`            | Cierra el cliente                                                    |                                                     |

---

## Registros de acceso y métricas

* Los accesos se guardan en `logs/access.log` con marca de tiempo, usuario, host destino y resultado.
* El archivo se borra al apagar el proxy.
* Las métricas se almacenan en memoria y se consultan con `stats`.

Ejemplo de salida `stats`:

```text
total_connections: 345
bytes_transferred: 12 834 991
concurrent_connections: 4
```

---


## Licencia

`MIT` – consulta el archivo `LICENSE` para el texto completo.
