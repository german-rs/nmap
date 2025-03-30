/***************************************************************************
 * main.cc -- Contiene la función main() de Nmap. Nótese que main()        *
 * hace muy poco excepto llamar a nmap_main() (que está en nmap.cc)        *
 *                                                                         *
 ***********************TÉRMINOS IMPORTANTES DE LICENCIA DE NMAP*************
 *
 * El Escáner de Seguridad Nmap es (C) 1996-2025 Nmap Software LLC ("El Proyecto
 * Nmap"). Nmap es también una marca registrada del Proyecto Nmap.
 *
 * Este programa se distribuye bajo los términos de la Licencia Pública de
 * Código Fuente de Nmap (NPSL). El texto exacto de la licencia aplicable a una
 * versión particular de Nmap o revisión de control de código fuente está contenido
 * en el archivo LICENSE distribuido con esa versión de Nmap o revisión de control
 * de código fuente. Más información sobre derechos de autor/legales de Nmap está
 * disponible en https://nmap.org/book/man-legal.html, y más información sobre
 * la licencia NPSL puede encontrarse en https://nmap.org/npsl/ . Este
 * encabezado resume algunos puntos clave de la licencia de Nmap, pero no
 * sustituye al texto real de la licencia.
 *
 * Nmap es generalmente gratuito para que los usuarios finales lo descarguen y
 * lo usen, incluyendo uso comercial. Está disponible en https://nmap.org.
 *
 * La licencia de Nmap generalmente prohíbe a las empresas usar y
 * redistribuir Nmap en productos comerciales, pero vendemos una edición especial
 * Nmap OEM con una licencia más permisiva y características especiales para
 * este propósito. Vea https://nmap.org/oem/
 *
 * Si ha recibido un acuerdo de licencia escrito de Nmap o un contrato
 * que establece términos distintos a estos (como una licencia OEM de Nmap), puede
 * elegir usar y redistribuir Nmap bajo esos términos en su lugar.
 *
 * Las compilaciones oficiales de Nmap para Windows incluyen el software Npcap
 * (https://npcap.com) para la captura y transmisión de paquetes. Está bajo
 * términos de licencia separados que prohíben la redistribución sin permiso
 * especial. Por lo tanto, las compilaciones oficiales de Nmap para Windows no pueden
 * ser redistribuidas sin un permiso especial (como una licencia OEM de Nmap).
 *
 * Se proporciona el código fuente de este software porque creemos que los usuarios tienen
 * derecho a saber exactamente qué va a hacer un programa antes de ejecutarlo.
 * Esto también le permite auditar el software en busca de agujeros de seguridad.
 *
 * El código fuente también le permite portar Nmap a nuevas plataformas, corregir errores y
 * añadir nuevas características. Se le anima encarecidamente a enviar sus cambios como un
 * PR de Github o por correo electrónico a la lista de correo dev@nmap.org para su posible
 * incorporación a la distribución principal. A menos que especifique lo contrario, se
 * entiende que nos está ofreciendo derechos muy amplios para usar sus
 * contribuciones como se describe en el Acuerdo de Contribución de la Licencia Pública
 * de Código Fuente de Nmap. Esto es importante porque financiamos el proyecto vendiendo licencias
 * con varios términos, y también porque la incapacidad de relicenciar código ha
 * causado problemas devastadores para otros proyectos de Software Libre (como KDE
 * y NASM).
 *
 * La versión gratuita de Nmap se distribuye con la esperanza de que sea
 * útil, pero SIN NINGUNA GARANTÍA; ni siquiera la garantía implícita de
 * COMERCIABILIDAD o IDONEIDAD PARA UN PROPÓSITO PARTICULAR. Las garantías,
 * indemnizaciones y soporte comercial están disponibles a través del
 * programa OEM de Npcap--vea https://nmap.org/oem/
 *
 ***************************************************************************/

/* $Id$ */

/**
 * @file main.c
 * @brief Punto de entrada principal para la aplicación Nmap
 *
 * Este archivo contiene la función principal (main) que inicializa la aplicación Nmap.
 * Se encarga de procesar argumentos de línea de comandos, variables de entorno y
 * la funcionalidad de reanudación de escaneos.
 */

/**
 * Inclusión de cabeceras de sistema para manejo de señales y configuración regional
 */
#include <signal.h>
#include <locale.h>

/**
 * Inclusión de cabeceras propias de Nmap
 */
#include "nmap.h"       // Funcionalidad principal de Nmap
#include "NmapOps.h"    // Estructura de opciones de Nmap
#include "utils.h"      // Funciones de utilidad
#include "nmap_error.h" // Gestión de errores

/**
 * Inclusión condicional para depuración de memoria
 */
#ifdef MTRACE
#include "mcheck.h"
#endif

/**
 * Soporte específico para sistemas AmigaOS
 */
#ifdef __amigaos__
#include <proto/exec.h>   // Funciones del sistema ejecutivo de Amiga
#include <proto/dos.h>    // Funciones DOS de Amiga
#include "nmap_amigaos.h" // Funcionalidad específica de Nmap para AmigaOS

/**
 * Bases de bibliotecas necesarias para la pila TCP/IP en AmigaOS
 */
struct Library *SocketBase = NULL, *MiamiBase = NULL, *MiamiBPFBase = NULL, *MiamiPCapBase = NULL;
static const char ver[] = "$VER:" NMAP_NAME " v" NMAP_VERSION " [Amiga.sf]";

/**
 * @brief Cierra las bibliotecas utilizadas en AmigaOS
 *
 * Esta función se registra con atexit() para asegurar que las bibliotecas
 * se cierren adecuadamente al finalizar el programa.
 */
static void CloseLibs(void)
{
  if (MiamiPCapBase)
    CloseLibrary(MiamiPCapBase);
  if (MiamiBPFBase)
    CloseLibrary(MiamiBPFBase);
  if (SocketBase)
    CloseLibrary(SocketBase);
  if (MiamiBase)
    CloseLibrary(MiamiBase);
}

/**
 * @brief Abre las bibliotecas necesarias para la pila TCP/IP en AmigaOS
 *
 * @return BOOL TRUE si todas las bibliotecas se abrieron correctamente, FALSE en caso contrario
 */
static BOOL OpenLibs(void)
{
  if (!(MiamiBase = OpenLibrary(MIAMINAME, 21)))
    return FALSE;
  if (!(SocketBase = OpenLibrary("bsdsocket.library", 4)))
    return FALSE;
  if (!(MiamiBPFBase = OpenLibrary(MIAMIBPFNAME, 3)))
    return FALSE;
  if (!(MiamiPCapBase = OpenLibrary(MIAMIPCAPNAME, 5)))
    return FALSE;
  atexit(CloseLibs);
  return TRUE;
}
#endif

/**
 * Declaración de variables globales
 */
extern NmapOps o; /* Estructura de opciones global */

/**
 * Declaración de funciones externas
 */
extern void set_program_name(const char *name);

/**
 * @brief Función principal del programa
 *
 * Esta función realiza las siguientes tareas:
 * 1) Verifica la variable de entorno NMAP_ARGS
 * 2) Comprueba si Nmap fue llamado con la opción --resume
 * 3) Reanuda un escaneo previo o simplemente llama a nmap_main
 *
 * @param argc Número de argumentos de línea de comandos
 * @param argv Vector de argumentos de línea de comandos
 * @return int Código de salida del programa
 */
int main(int argc, char *argv[])
{
  char command[2048];   // Buffer para almacenar comandos
  int myargc;           // Contador de argumentos procesados
  char **myargv = NULL; // Vector de argumentos procesados
  char *cptr;           // Puntero auxiliar para procesamiento de cadenas
  int ret;              // Valor de retorno
  int i;                // Variable de iteración

  // Configura la localización regional
  o.locale = strdup(setlocale(LC_CTYPE, NULL));
  set_program_name(argv[0]);

#ifdef __amigaos__
  // Inicialización específica para AmigaOS
  if (!OpenLibs())
  {
    error("Couldn't open TCP/IP Stack Library(s)!");
    exit(20);
  }
  MiamiBPFInit((struct Library *)MiamiBase, (struct Library *)SocketBase);
  MiamiPCapInit((struct Library *)MiamiBase, (struct Library *)SocketBase);
#endif

#ifdef MTRACE
  // Esta extensión de glibc habilita el rastreo de memoria para detectar
  // fugas de memoria, liberaciones de memoria no asignada, etc.
  // Ver http://www.gnu.org/manual/glibc-2.2.5/html_node/Allocation-Debugging.html
  // Solo funciona si la variable de entorno MALLOC_TRACE está configurada a un archivo
  // donde se escribirá un registro de uso de memoria. Después de que el programa finalice,
  // se puede analizar el registro mediante el comando 'mtrace [ejecutable] [archivo_log]'
  // MTRACE solo debe definirse durante sesiones de depuración.
  mtrace();
#endif

  // Comprueba la variable de entorno NMAP_ARGS
  if ((cptr = getenv("NMAP_ARGS")))
  {
    // Construye la línea de comando completa
    if (Snprintf(command, sizeof(command), "nmap %s", cptr) >= (int)sizeof(command))
    {
      error("Advertencia: La variable NMAP_ARGS es demasiado larga, truncada");
    }

    // Añade el resto de argumentos de la línea de comandos
    for (i = 1; i < argc && strlen(command) + strlen(argv[i]) + 1 < sizeof(command); i++)
    {
      strcat(command, " ");
      strcat(command, argv[i]);
    }

    // Analiza la línea de comandos construida
    myargc = arg_parse(command, &myargv);
    if (myargc < 1)
    {
      fatal("No se ha podido analizar la variable NMAP_ARGS");
    }

    // Ejecuta nmap_main con los argumentos procesados
    ret = nmap_main(myargc, myargv);
    arg_parse_free(myargv);
    return ret;
  }

  // Manejo de la opción --resume
  if (argc == 3 && strcmp("--resume", argv[1]) == 0)
  {
    // Intenta reanudar un escaneo abortado utilizando el archivo de registro especificado
    if (gather_logfile_resumption_state(argv[2], &myargc, &myargv) == -1)
    {
      fatal("No se puede reanudar desde un (supuesto) archivo de registro %s", argv[2]);
    }
    o.resuming = true;
    return nmap_main(myargc, myargv);
  }

  // Si no hay opciones especiales, ejecuta nmap_main directamente
  return nmap_main(argc, argv);
}
