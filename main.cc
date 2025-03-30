
/***************************************************************************
 * main.cc -- Contains the main() function of Nmap.  Note that main()      *
 * does very little except for calling nmap_main() (which is in nmap.cc)   *
 *                                                                         *
 ***********************IMPORTANT NMAP LICENSE TERMS************************
 *
 * The Nmap Security Scanner is (C) 1996-2025 Nmap Software LLC ("The Nmap
 * Project"). Nmap is also a registered trademark of the Nmap Project.
 *
 * This program is distributed under the terms of the Nmap Public Source
 * License (NPSL). The exact license text applying to a particular Nmap
 * release or source code control revision is contained in the LICENSE
 * file distributed with that version of Nmap or source code control
 * revision. More Nmap copyright/legal information is available from
 * https://nmap.org/book/man-legal.html, and further information on the
 * NPSL license itself can be found at https://nmap.org/npsl/ . This
 * header summarizes some key points from the Nmap license, but is no
 * substitute for the actual license text.
 *
 * Nmap is generally free for end users to download and use themselves,
 * including commercial use. It is available from https://nmap.org.
 *
 * The Nmap license generally prohibits companies from using and
 * redistributing Nmap in commercial products, but we sell a special Nmap
 * OEM Edition with a more permissive license and special features for
 * this purpose. See https://nmap.org/oem/
 *
 * If you have received a written Nmap license agreement or contract
 * stating terms other than these (such as an Nmap OEM license), you may
 * choose to use and redistribute Nmap under those terms instead.
 *
 * The official Nmap Windows builds include the Npcap software
 * (https://npcap.com) for packet capture and transmission. It is under
 * separate license terms which forbid redistribution without special
 * permission. So the official Nmap Windows builds may not be redistributed
 * without special permission (such as an Nmap OEM license).
 *
 * Source is provided to this software because we believe users have a
 * right to know exactly what a program is going to do before they run it.
 * This also allows you to audit the software for security holes.
 *
 * Source code also allows you to port Nmap to new platforms, fix bugs, and
 * add new features. You are highly encouraged to submit your changes as a
 * Github PR or by email to the dev@nmap.org mailing list for possible
 * incorporation into the main distribution. Unless you specify otherwise, it
 * is understood that you are offering us very broad rights to use your
 * submissions as described in the Nmap Public Source License Contributor
 * Agreement. This is important because we fund the project by selling licenses
 * with various terms, and also because the inability to relicense code has
 * caused devastating problems for other Free Software projects (such as KDE
 * and NASM).
 *
 * The free version of Nmap is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. Warranties,
 * indemnification and commercial support are all available through the
 * Npcap OEM program--see https://nmap.org/oem/
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
