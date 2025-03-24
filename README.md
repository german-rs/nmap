# Nmap - Es

## Importante ⚠️

Este repositorio contiene mi estudio y traducción del proyecto Nmap, una herramienta de código abierto para exploración de redes y auditoría de seguridad.

[Repositorio Original de Nmap](https://github.com/nmap/nmap)

### Objetivo del Proyecto

- Traducir el código fuente y documentación al español.
- Comprender en profundidad la arquitectura y funcionamiento de Nmap.
- Contribuir a la comunidad de software de código abierto.

### Aviso Legal

- Todos los derechos del código original pertenecen a los creadores de Nmap.
- Esta es una iniciativa de estudio y traducción sin fines comerciales.

## Introducción

Nmap se publica bajo una licencia personalizada, basada en (pero no compatible con) GPLv2. La licencia de Nmap permite el uso libre por parte de los usuarios finales, y también ofrecemos una licencia comercial para empresas que deseen redistribuir la tecnología de Nmap con sus productos. Consulte [Derechos de autor y licencia de Nmap](https://nmap.org/book/man-legal.html)
para más detalles.

La última versión de este software, así como los instaladores binarios para Windows, macOS y Linux (RPM) están disponibles en
[Nmap.org](https://nmap.org/download.html)

La documentación completa también está disponible
[en el sitio web Nmap.org](https://nmap.org/docs.html).

Las preguntas y sugerencias pueden enviarse a
[la lista de correo Nmap-dev](https://nmap.org/mailman/listinfo/dev).

## Instalación

Lo ideal sería poder teclear sin más:

    ./configure
    make
    make install

Para más información sobre la compilación, instalación y desinstalación, lea la
[Guía de instalación de Nmap](https://nmap.org/book/install.html) en Nmap.org.

## Uso de Nmap

Nmap tiene muchas características, pero empezar es tan fácil como ejecutar `nmap
scanme.nmap.org`. Ejecutar `nmap` sin ningún parámetro le dará una útil
lista de las opciones más comunes, que se discuten en profundidad en [la página man
page](https://nmap.org/book/man.html). Los usuarios que prefieran una interfaz gráfica
pueden utilizar la interfaz Zenmap (https://nmap.org/zenmap/).

## Contribución

Puede encontrar información sobre cómo enviar informes de errores y contribuir al proyecto Nmap
en los archivos [HACKING](HACKING) y [CONTRIBUTING.md](CONTRIBUTING.md)
