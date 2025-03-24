# Tabla de contenidos.

---

- [Introducción](#intro)
- [Repositorio de código](#repo)
- [Reporte de Bugs](#bug)
- [Pull Requests](#pr)
- [Redirecionador issues.nmap.org ](#issues)
- [El archivo HACKING](#hacking)

## <a name="intro"></a>Introducción

Este archivo sirve como complemento al archivo [HACKING file](HACKING). Contiene información específica sobre el uso de Github por parte de Nmap y cómo los colaboradores pueden utilizar los servicios de Github para participar en el desarrollo de Nmap.

## <a name="repo"></a>Repositorio de código

El repositorio de código autorizado sigue siendo el repositorio Subversion en [https://svn.nmap.org/nmap](https://svn.nmap.org/nmap). El repositorio de Github se sincroniza una vez por hora. Todos los commits se realizan directamente en Subversion, por lo que Github es una réplica de sólo lectura.

## <a name="bug"></a>Reporte de Bugs

Nmap utiliza Github Issues para hacer un seguimiento de los informes de errores. Por favor, asegúrese de incluir la versión de Nmap que está utilizando, los pasos para reproducir el error y una descripción de lo que espera que sea el comportamiento correcto.

## <a name="pr"></a>Pull Requests

Nmap agradece sus contribuciones en forma de Pull Request en Github. Dado que el repositorio de Github es actualmente de sólo lectura, no podemos fusionar directamente desde el PR. En su lugar, convertiremos su PR en un parche y lo aplicaremos al repositorio Subversion. Nos aseguraremos de acreditarle adecuadamente en el archivo CHANGELOG, y el mensaje de confirmación hará referencia al número de PR.

Dado que no todos los committers de Nmap utilizan Github a diario, es útil enviar un correo electrónico de notificación a [dev@nmap.org](mailto:dev@nmap.org) haciendo referencia al PR e incluyendo una breve descripción de la funcionalidad del parche.

El uso de pull requests tiene varias ventajas sobre los parches enviados por correo electrónico:

1. Permite que las pruebas de construcción de Travis CI se ejecuten y comprueben los problemas del código.

2. La interfaz de Github facilita la discusión de los cambios en el código.

3. Referenciar las contribuciones por el número de PR es más conveniente que rastrearlas por la URL del archivo de correo de
   [seclists.org](http://seclists.org/) especialmente cuando la discusión abarca más de un trimestre del año.

## <a name="issues"></a>Redirecionador issues.nmap.org

Para mayor comodidad, puede utilizar [issues.nmap.org](http://issues.nmap.org) para redirigir a los temas (informes de errores y pull requests) por número (por ejemplo, [http://issues.nmap.org/34](http://issues.nmap.org/34)) o para enlazar a la página del nuevo tema [http://issues.nmap.org/new](http://issues.nmap.org/new).

## <a name="hacking"></a>El archivo HACKING

En el archivo [HACKING file](HACKING) se puede encontrar información general sobre cómo hackear Nmap y participar en nuestra comunidad de desarrolladores y usuarios. Describe cómo empezar, licencias, guía de estilo y cómo usar la lista de correo dev.
