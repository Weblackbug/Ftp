# UPDOWNFTP

## Descripción
Este proyecto es un cliente FTP/SFTP escrito en C++ que permite subir y descargar directorios completos al servidor remoto. Incluye:

- Subida de archivos con exclusión de tipos no deseados (`.zip`, `.md`, `.bat`, `.exe`).
- Descarga de un directorio remoto como archivo ZIP.
- Interfaz gráfica básica con menús para abrir ZIP, descargar blog y configuración.

## Cómo compilar
```bash
# Generar los archivos de compilación con CMake
mkdir build && cd build
cmake ..
cmake --build .
```

## Uso rápido
```bash
# Ejecutar el programa (ejemplo)
./FtpUploader.exe
```

## Configuración
El archivo `config.json` contiene la información de conexión. **IMPORTANTE:** Los valores sensibles (`host`, `user`, `pass`) han sido reemplazados por marcadores de posición (`YOUR_HOST_HERE`, `YOUR_USER_HERE`, `YOUR_PASSWORD_HERE`).

## Contribuciones
1. Fork del repositorio.
2. Crea una rama (`git checkout -b feature-x`).
3. Haz commit de tus cambios y abre un Pull Request.

---
*Generated automatically.*
