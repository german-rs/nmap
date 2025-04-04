/***************************************************************************
 * nmap.cc -- Actualmente maneja algunas de las características de escaneo  *
 * de puertos de Nmap, así como la interfaz de línea de comandos para el   *
 * usuario. Nótese que la función main() real está en main.cc              *
 *                                                                         *
 *****************TÉRMINOS DE LICENCIA IMPORTANTES DE NMAP*******************
 *
 * El Escáner de Seguridad Nmap es (C) 1996-2025 Nmap Software LLC ("El Proyecto
 * Nmap"). Nmap es también una marca registrada del Proyecto Nmap.
 *
 * Este programa es distribuido bajo los términos de la Licencia de Código Fuente
 * Público de Nmap (NPSL). El texto exacto de la licencia que aplica a una versión
 * particular de Nmap o revisión de control de código fuente está contenido en el
 * archivo LICENSE distribuido con esa versión de Nmap o revisión de control de
 * código fuente. Más información sobre derechos de autor/legal de Nmap está
 * disponible en https://nmap.org/book/man-legal.html, y más información sobre
 * la licencia NPSL en sí puede encontrarse en https://nmap.org/npsl/ . Este
 * encabezado resume algunos puntos clave de la licencia de Nmap, pero no
 * sustituye al texto real de la licencia.
 *
 * Nmap es generalmente gratuito para que los usuarios finales lo descarguen y
 * lo usen, incluyendo uso comercial. Está disponible en https://nmap.org.
 *
 * La licencia de Nmap generalmente prohíbe a las empresas usar y redistribuir
 * Nmap en productos comerciales, pero vendemos una Edición OEM de Nmap especial
 * con una licencia más permisiva y características especiales para este propósito.
 * Véase https://nmap.org/oem/
 *
 * Si ha recibido un acuerdo de licencia de Nmap por escrito o un contrato que
 * establece términos diferentes a estos (como una licencia OEM de Nmap), puede
 * elegir usar y redistribuir Nmap bajo esos términos en su lugar.
 *
 * Las compilaciones oficiales de Nmap para Windows incluyen el software Npcap
 * (https://npcap.com) para la captura y transmisión de paquetes. Está bajo
 * términos de licencia separados que prohíben la redistribución sin permiso
 * especial. Por lo tanto, las compilaciones oficiales de Nmap para Windows no
 * pueden ser redistribuidas sin permiso especial (como una licencia OEM de Nmap).
 *
 * Se proporciona el código fuente de este software porque creemos que los usuarios
 * tienen derecho a saber exactamente qué va a hacer un programa antes de ejecutarlo.
 * Esto también le permite auditar el software en busca de agujeros de seguridad.
 *
 * El código fuente también le permite portar Nmap a nuevas plataformas, corregir
 * errores y añadir nuevas características. Se le anima encarecidamente a enviar
 * sus cambios como un PR de Github o por correo electrónico a la lista de correo
 * dev@nmap.org para su posible incorporación a la distribución principal. A menos
 * que especifique lo contrario, se entiende que nos está ofreciendo derechos muy
 * amplios para usar sus contribuciones como se describe en el Acuerdo de Contribución
 * de la Licencia de Código Fuente Público de Nmap. Esto es importante porque
 * financiamos el proyecto vendiendo licencias con varios términos, y también porque
 * la incapacidad de relicenciar código ha causado problemas devastadores para otros
 * proyectos de Software Libre (como KDE y NASM).
 *
 * La versión gratuita de Nmap se distribuye con la esperanza de que sea útil,
 * pero SIN NINGUNA GARANTÍA; ni siquiera la garantía implícita de COMERCIABILIDAD
 * o IDONEIDAD PARA UN PROPÓSITO PARTICULAR. Las garantías, indemnización y soporte
 * comercial están disponibles a través del programa OEM de Npcap--véase
 * https://nmap.org/oem/
 *
 ***************************************************************************/

/* $Id$ */
/**
 * @file nmap.cc
 * @brief Sección de código con inclusiones y funciones utilitarias para Nmap
 */

/**
 * Definiciones específicas para sistemas Windows
 */
#ifdef WIN32
#include "winfix.h"
/* Este nombre colisiona en la siguiente inclusión. */
#undef PS_NONE
#include <shlobj.h>
#endif

/**
 * Inclusiones de cabeceras principales de Nmap
 */
#include "nmap.h"
#include "osscan.h"       /* Escaneo de sistemas operativos */
#include "scan_engine.h"  /* Motor principal de escaneo */
#include "FPEngine.h"     /* Motor de huellas digitales */
#include "idle_scan.h"    /* Implementación de escaneo idle */
#include "NmapOps.h"      /* Opciones y configuración de Nmap */
#include "MACLookup.h"    /* Búsqueda de direcciones MAC */
#include "traceroute.h"   /* Implementación de traceroute */
#include "nmap_tty.h"     /* Manejo de terminal */
#include "nmap_ftp.h"     /* Funcionalidad FTP para escaneo rebote */
#include "services.h"     /* Base de datos de servicios */
#include "targets.h"      /* Manejo de objetivos de escaneo */
#include "tcpip.h"        /* Funciones de nivel bajo TCP/IP */
#include "NewTargets.h"   /* Administración dinámica de objetivos */
#include "Target.h"       /* Clase Target */
#include "service_scan.h" /* Escaneo de servicios */
#include "charpool.h"     /* Pool de caracteres para optimización de memoria */
#include "nmap_error.h"   /* Manejo de errores */
#include "utils.h"        /* Funciones utilitarias */
#include "xml.h"          /* Generación de salida XML */
#include "scan_lists.h"   /* Listas de puertos y protocolos a escanear */
#include "payload.h"      /* Manejo de cargas útiles para paquetes */

/**
 * Soporte para scripts NSE (Nmap Scripting Engine)
 */
#ifndef NOLUA
#include "nse_main.h" /* Motor de scripting Lua */
#endif

/**
 * Soporte para manejo de señales
 */
#ifdef HAVE_SIGNAL
#include <signal.h>
#endif

#include <fcntl.h>

/**
 * Soporte para información de usuario en sistemas Unix
 */
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

/**
 * Definición de IPPROTO_SCTP si no está disponible
 */
#ifndef IPPROTO_SCTP
#include "libnetutil/netutil.h"
#endif

/**
 * Soporte para OpenSSL si está disponible
 */
#if HAVE_OPENSSL
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#endif

/**
 * Soporte para libssh2 si está disponible (para escaneos SSH)
 */
#if HAVE_LIBSSH2
#include <libssh2.h>
#endif

/**
 * Soporte para compresión zlib si está disponible
 */
#if HAVE_LIBZ
#include <zlib.h>
#endif

/**
 * Inclusión de libdnet para obtener el número de versión
 */
#ifdef WIN32
#include "libdnet-stripped/include/dnet_winconfig.h"
#else
#include "libdnet-stripped/include/config.h"
#endif
#define DNET_VERSION VERSION

/**
 * Detección de Windows Subsystem for Linux (WSL) en sistemas Linux
 */
#ifdef LINUX
#include <sys/utsname.h>
#endif

/**
 * Inclusiones de la biblioteca estándar de C++
 */
#include <string>
#include <sstream>
#include <vector>

/**
 * Variables globales para el procesamiento de opciones
 */
extern char *optarg; /* Argumento de la opción actual */
extern int optind;   /* Índice del siguiente argumento a procesar */
extern NmapOps o;    /* Estructura de opciones global */

/**
 * @brief Muestra la información de versión de Nmap
 */
static void display_nmap_version();

/**
 * @brief Mecanismo para guardar argv[0] para código que lo requiera
 */
static const char *program_name = NULL;

/**
 * @brief Establece el nombre del programa
 * @param name Nombre del programa a establecer
 */
void set_program_name(const char *name)
{
  program_name = name;
}

/**
 * @brief Obtiene el nombre del programa
 * @return Puntero al nombre del programa
 */
static const char *get_program_name(void)
{
  return program_name;
}

/**
 * @brief Analiza el argumento --scanflags
 *
 * Esta función analiza el argumento proporcionado como flags de escaneo TCP.
 * El argumento puede ser un número mayor o igual a 0, o una cadena que contenga
 * nombres de flags TCP, como "URGPSHFIN".
 *
 * @param arg Cadena con los flags a analizar
 * @return Valor de los flags o -1 si el argumento es inválido
 */
static int parse_scanflags(char *arg)
{
  int flagval = 0;  // Inicializa la variable de retorno
  char *end = NULL; // Puntero para verificar el final de la conversión

  // Comprueba si el argumento es un número
  if (isdigit((int)(unsigned char)arg[0]))
  {
    flagval = strtol(arg, &end, 0); // Convierte la cadena a un número entero
    // Verifica si la conversión falló o el valor está fuera del rango permitido
    if (*end || flagval < 0 || flagval > 255)
      return -1; // Retorna -1 si el argumento es inválido
  }
  else
  {
    // Verifica y asigna los valores correspondientes a cada flag TCP
    if (strcasestr(arg, "FIN"))
      flagval |= TH_FIN;
    if (strcasestr(arg, "SYN"))
      flagval |= TH_SYN;
    if (strcasestr(arg, "RST") || strcasestr(arg, "RESET"))
      flagval |= TH_RST;
    if (strcasestr(arg, "PSH") || strcasestr(arg, "PUSH"))
      flagval |= TH_PUSH;
    if (strcasestr(arg, "ACK"))
      flagval |= TH_ACK;
    if (strcasestr(arg, "URG"))
      flagval |= TH_URG;
    if (strcasestr(arg, "ECE"))
      flagval |= TH_ECE;
    if (strcasestr(arg, "CWR"))
      flagval |= TH_CWR;
    if (strcasestr(arg, "ALL"))
      flagval = 255; // Asigna todos los flags si se especifica "ALL"
    if (strcasestr(arg, "NONE"))
      flagval = 0; // No asigna ningún flag si se especifica "NONE"
  }
  return flagval; // Retorna el valor de los flags
}

static void printusage()
{

  printf("%s %s ( %s )\n"
         "Utilización: nmap [Tipo(s) de escaneado] [Opciones] {especificación del objetivo}\n"
         "ESPECIFICACIÓN DEL OBJETIVO:\n"
         "  Puedes pasar nombres de host, direcciones IP, redes, etc..\n"
         "  Ejemplo: scanme.nmap.org, microsoft.com/24, 192.168.0.1; 10.0.0-255.1-254\n"
         "  -iL <inputfilename>: Entrada de la lista de hosts/redes\n"
         "  -iR <num hosts>: Elegir objetivos al azar\n"
         "  --exclude <host1[,host2][,host3],...>: Excluir hosts/redes\n"
         "  --excludefile <exclude_file>: Lista de exclusión del archivo\n"
         "DESCUBRIMIENTO DEL HOST:\n"
         "  -sL: Escaneo de Listado - simplemente enumera los objetivos a escanear\n"
         "  -sn: Escaneo Ping - desactivar el escaneo de puertos\n"
         "  -Pn: Tratar todos los hosts como en línea -- omitir la detección de hosts\n"
         "  -PS/PA/PU/PY[portlist]: Descubrimiento TCP SYN, TCP ACK, UDP o SCTP en los puertos especificados\n"
         "  -PE/PP/PM: Sondeos de descubrimiento ICMP echo, timestamp y netmask request\n"
         "  -PO[protocol list]: Ping de Protocolo IP\n"
         "  -n/-R: Nunca hacer resolución DNS/Siempre resolver [predeterminado: a veces]\n"
         "  --dns-servers <serv1[,serv2],...>: Especificar servidores DNS personalizados\n"
         "  --system-dns: Usar el resolvedor DNS del sistema operativo\n"
         "  --traceroute: Rastrear ruta de saltos a cada host\n"
         "TÉCNICAS DE ESCANEO:\n"
         "  -sS/sT/sA/sW/sM: Escaneos TCP SYN/Connect()/ACK/Window/Maimon\n"
         "  -sU: Escaneo UDP\n"
         "  -sN/sF/sX: Escaneos TCP Null, FIN y Xmas\n"
         "  --scanflags <flags>: Personalizar banderas de escaneo TCP\n"
         "  -sI <zombie host[:probeport]>: Escaneo Idle\n"
         "  -sY/sZ: Escaneos SCTP INIT/COOKIE-ECHO\n"
         "  -sO: Escaneo de protocolo IP\n"
         "  -b <FTP relay host>: Escaneo rebote FTP\n"
         "ESPECIFICACIÓN DE PUERTOS Y ORDEN DE ESCANEO:\n"
         "  -p <rangos de puertos>: Escanear solo los puertos especificados\n"
         "    Ej: -p22; -p1-65535; -p U:53,111,137,T:21-25,80,139,8080,S:9\n"
         "  --exclude-ports <rangos de puertos>: Excluir los puertos especificados del escaneo\n"
         "  -F: Modo rápido - Escanear menos puertos que el escaneo predeterminado\n"
         "  -r: Escanear puertos secuencialmente - no aleatorizar\n"
         "  --top-ports <número>: Escanear los <número> puertos más comunes\n"
         "  --port-ratio <ratio>: Escanear puertos más comunes que <ratio>\n"
         "DETECCIÓN DE SERVICIO/VERSIÓN:\n"
         "  -sV: Sondear puertos abiertos para determinar info de servicio/versión\n"
         "  --version-intensity <nivel>: Establecer desde 0 (ligero) hasta 9 (probar todos los sondeos)\n"
         "  --version-light: Limitar a los sondeos más probables (intensidad 2)\n"
         "  --version-all: Probar cada sondeo individual (intensidad 9)\n"
         "  --version-trace: Mostrar actividad detallada de escaneo de versión (para depuración)\n"
#ifndef NOLUA
         "ESCANEO DE SCRIPTS:\n"
         "  -sC: equivalente a --script=default\n"
         "  --script=<scripts Lua>: <scripts Lua> es una lista separada por comas de\n"
         "           directorios, archivos de script o categorías de script\n"
         "  --script-args=<n1=v1,[n2=v2,...]>: proporcionar argumentos a los scripts\n"
         "  --script-args-file=filename: proporcionar argumentos de script NSE en un archivo\n"
         "  --script-trace: Mostrar todos los datos enviados y recibidos\n"
         "  --script-updatedb: Actualizar la base de datos de scripts\n"
         "  --script-help=<scripts Lua>: Mostrar ayuda sobre scripts.\n"
         "           <scripts Lua> es una lista separada por comas de archivos de script o\n"
         "           categorías de script.\n"
#endif
         "DETECCIÓN DE SISTEMA OPERATIVO:\n"
         "  -O: Habilitar detección de sistema operativo\n"
         "  --osscan-limit: Limitar la detección de SO a objetivos prometedores\n"
         "  --osscan-guess: Adivinar SO de manera más agresiva\n"
         "TEMPORIZACIÓN Y RENDIMIENTO:\n"
         "  Las opciones que toman <tiempo> están en segundos, o añadir 'ms' (milisegundos),\n"
         "  's' (segundos), 'm' (minutos), o 'h' (horas) al valor (ej. 30m).\n"
         "  -T<0-5>: Establecer plantilla de temporización (mayor es más rápido)\n"
         "  --min-hostgroup/max-hostgroup <tamaño>: Tamaños de grupos de escaneo de host en paralelo\n"
         "  --min-parallelism/max-parallelism <numprobes>: Paralelización de sondeos\n"
         "  --min-rtt-timeout/max-rtt-timeout/initial-rtt-timeout <tiempo>: Especifica\n"
         "      el tiempo de ida y vuelta del sondeo.\n"
         "  --max-retries <intentos>: Limita número de retransmisiones de sondeo de escaneo de puertos.\n"
         "  --host-timeout <tiempo>: Abandonar el objetivo después de este tiempo\n"
         "  --scan-delay/--max-scan-delay <tiempo>: Ajustar retraso entre sondeos\n"
         "  --min-rate <número>: Enviar paquetes no más lento que <número> por segundo\n"
         "  --max-rate <número>: Enviar paquetes no más rápido que <número> por segundo\n"
         "EVASIÓN DE FIREWALL/IDS Y SUPLANTACIÓN:\n"
         "  -f; --mtu <val>: fragmentar paquetes (opcionalmente con MTU dado)\n"
         "  -D <señuelo1,señuelo2[,YO],...>: Ocultar un escaneo con señuelos\n"
         "  -S <Dirección_IP>: Suplantar dirección de origen\n"
         "  -e <interfaz>: Usar interfaz especificada\n"
         "  -g/--source-port <número de puerto>: Usar número de puerto dado\n"
         "  --proxies <url1,[url2],...>: Transmitir conexiones a través de proxies HTTP/SOCKS4\n"
         "  --data <cadena hexadecimal>: Añadir una carga útil personalizada a los paquetes enviados\n"
         "  --data-string <cadena>: Añadir una cadena ASCII personalizada a los paquetes enviados\n"
         "  --data-length <num>: Añadir datos aleatorios a los paquetes enviados\n"
         "  --ip-options <opciones>: Enviar paquetes con opciones ip especificadas\n"
         "  --ttl <val>: Establecer campo IP time-to-live\n"
         "  --spoof-mac <dirección mac/prefijo/nombre de vendedor>: Suplantar tu dirección MAC\n"
         "  --badsum: Enviar paquetes con un checksum TCP/UDP/SCTP falso\n"
         "SALIDA:\n"
         "  -oN/-oX/-oS/-oG <archivo>: Salida de escaneo en formato normal, XML, s|<rIpt kIddi3,\n"
         "     y Grepable, respectivamente, al nombre de archivo dado.\n"
         "  -oA <nombre base>: Salida en los tres formatos principales a la vez\n"
         "  -v: Aumentar nivel de verbosidad (usar -vv o más para mayor efecto)\n"
         "  -d: Aumentar nivel de depuración (usar -dd o más para mayor efecto)\n"
         "  --reason: Mostrar la razón por la que un puerto está en un estado particular\n"
         "  --open: Mostrar solo puertos abiertos (o posiblemente abiertos)\n"
         "  --packet-trace: Mostrar todos los paquetes enviados y recibidos\n"
         "  --iflist: Imprimir interfaces y rutas del host (para depuración)\n"
         "  --append-output: Añadir en lugar de sobreescribir los archivos de salida especificados\n"
         "  --resume <nombre de archivo>: Reanudar un escaneo abortado\n"
         "  --noninteractive: Desactivar interacciones en tiempo de ejecución vía teclado\n"
         "  --stylesheet <ruta/URL>: Hoja de estilo XSL para transformar salida XML a HTML\n"
         "  --webxml: Referenciar hoja de estilo desde Nmap.Org para XML más portable\n"
         "  --no-stylesheet: Evitar la asociación de hoja de estilo XSL con salida XML\n"
         "MISCELÁNEA:\n"
         "  -6: Habilitar escaneo IPv6\n"
         "  -A: Habilitar detección de SO, detección de versión, escaneo de scripts y traceroute\n"
         "  --datadir <nombre de directorio>: Especificar ubicación personalizada del archivo de datos Nmap\n"
         "  --send-eth/--send-ip: Enviar usando tramas ethernet brutas o paquetes IP\n"
         "  --privileged: Asumir que el usuario tiene privilegios completos\n"
         "  --unprivileged: Asumir que el usuario carece de privilegios de socket bruto\n"
         "  -V: Imprimir número de versión\n"
         "  -h: Imprimir esta página de resumen de ayuda.\n"
         "EJEMPLOS:\n"
         "  nmap -v -A scanme.nmap.org\n"
         "  nmap -v -sn 192.168.0.0/16 10.0.0.0/8\n"
         "  nmap -v -iR 10000 -Pn -p 80\n"
         "CONSULTA LA PÁGINA DEL MANUAL (https://nmap.org/book/man.html) PARA MÁS OPCIONES Y EJEMPLOS\n",
         NMAP_NAME, NMAP_VERSION, NMAP_URL);
}

#ifdef WIN32

/*
 * Función auxiliar para verificar privilegios en Windows
 * En Windows esta función está vacía ya que la verificación
 * se hace de otra manera
 */
static void check_setugid(void)
{
}
#else

/*
 * Muestra una advertencia cuando se ejecuta con setuid o setgid
 * ya que esto permite la ejecución de código (por ejemplo scripts NSE)
 * como el propietario/grupo.
 */
static void check_setugid(void)
{
  if (getuid() != geteuid())
    error("ADVERTENCIA: Ejecutar Nmap con setuid, como está haciendo, representa un alto riesgo de seguridad.\n");
  if (getgid() != getegid())
    error("ADVERTENCIA: Ejecutar Nmap con setgid, como está haciendo, representa un alto riesgo de seguridad.\n");
}
#endif

/*
 * Inserta un puerto en la lista de fusión si no existe ya
 * Parámetros:
 *   mlist - Lista de puertos objetivo
 *   merged_port_count - Contador de puertos fusionados
 *   p - Puerto a insertar
 */
static void insert_port_into_merge_list(unsigned short *mlist,
                                        int *merged_port_count,
                                        unsigned short p)
{
  int i;
  // Verifica que el puerto no esté ya en la lista
  for (i = 0; i < *merged_port_count; i++)
  {
    if (mlist[i] == p)
    {
      return;
    }
  }
  mlist[*merged_port_count] = p;
  (*merged_port_count)++;
}

/*
 * Fusiona dos listas de puertos eliminando duplicados
 * Parámetros:
 *   port_list1, count1 - Primera lista y su tamaño
 *   port_list2, count2 - Segunda lista y su tamaño
 *   merged_port_count - Puntero donde se almacenará el tamaño de la lista fusionada
 * Retorna:
 *   Lista fusionada de puertos sin duplicados
 */
static unsigned short *merge_port_lists(unsigned short *port_list1, int count1,
                                        unsigned short *port_list2, int count2,
                                        int *merged_port_count)
{
  int i;
  unsigned short *merged_port_list = NULL;

  *merged_port_count = 0;

  // Asigna memoria para la lista fusionada
  merged_port_list =
      (unsigned short *)safe_zalloc((count1 + count2) * sizeof(unsigned short));

  // Inserta puertos de la primera lista
  for (i = 0; i < count1; i++)
  {
    insert_port_into_merge_list(merged_port_list,
                                merged_port_count,
                                port_list1[i]);
  }

  // Inserta puertos de la segunda lista
  for (i = 0; i < count2; i++)
  {
    insert_port_into_merge_list(merged_port_list,
                                merged_port_count,
                                port_list2[i]);
  }

  // Reajusta el tamaño de memoria si hubo duplicados
  if (*merged_port_count < (count1 + count2))
  {
    merged_port_list = (unsigned short *)
        safe_realloc(merged_port_list,
                     (*merged_port_count) * sizeof(unsigned short));
  }

  return merged_port_list;
}

/*
 * Valida las listas de puertos y configuraciones de escaneo
 * Parámetros:
 *   vports - Estructura con las listas de puertos
 *   vo - Opciones de Nmap
 */
void validate_scan_lists(scan_lists &vports, NmapOps &vo)
{
  // Configura el tipo de ping si no está definido
  if (vo.pingtype == PINGTYPE_UNKNOWN)
  {
    if (vo.isr00t)
    {
      // Para root usa configuración por defecto según IPv4/IPv6
      if (vo.pf() == PF_INET)
      {
        vo.pingtype = DEFAULT_IPV4_PING_TYPES;
      }
      else
      {
        vo.pingtype = DEFAULT_IPV6_PING_TYPES;
      }

      // Configura puertos para ping ACK y SYN
      getpts_simple(DEFAULT_PING_ACK_PORT_SPEC, SCAN_TCP_PORT,
                    &vports.ack_ping_ports, &vports.ack_ping_count);
      getpts_simple(DEFAULT_PING_SYN_PORT_SPEC, SCAN_TCP_PORT,
                    &vports.syn_ping_ports, &vports.syn_ping_count);
    }
    else
    {
      // Para no-root usa ping TCP
      vo.pingtype = PINGTYPE_TCP;
      getpts_simple(DEFAULT_PING_CONNECT_PORT_SPEC, SCAN_TCP_PORT,
                    &vports.syn_ping_ports, &vports.syn_ping_count);
    }
  }

  // Ajusta configuraciones si no es root
  if (!vo.isr00t)
  {
    if (vo.pingtype & (PINGTYPE_ICMP_PING | PINGTYPE_ICMP_MASK | PINGTYPE_ICMP_TS))
    {
#ifdef WIN32
      error("Advertencia: Npcap no detectado -- usando ping TCP en lugar de ICMP");
#else
      error("Advertencia: No es root -- usando ping TCP en lugar de ICMP");
#endif
      // Deshabilita pings ICMP y habilita TCP
      vo.pingtype &= ~(PINGTYPE_ICMP_PING | PINGTYPE_ICMP_MASK | PINGTYPE_ICMP_TS);
      vo.pingtype |= PINGTYPE_TCP;

      // Configura puertos TCP si no hay definidos
      if (vports.syn_ping_count == 0)
      {
        getpts_simple(DEFAULT_TCP_PROBE_PORT_SPEC, SCAN_TCP_PORT, &vports.syn_ping_ports, &vports.syn_ping_count);
        assert(vports.syn_ping_count > 0);
      }
    }
  }

  // Ajusta el tipo de ping TCP para no-root
  if ((vo.pingtype & PINGTYPE_TCP) && (!vo.isr00t))
  {
    // Se debe usar ping estilo connect()
    // Simular que se querían sondeos SYN todo el tiempo
    if (vports.ack_ping_count > 0)
    {
      // Combina puertos ACK y SYN ya que ambos se reducen a sondeos SYN
      unsigned short *merged_port_list;
      int merged_port_count;

      merged_port_list = merge_port_lists(
          vports.syn_ping_ports, vports.syn_ping_count,
          vports.ack_ping_ports, vports.ack_ping_count,
          &merged_port_count);

      // Limpia memoria
      free(vports.syn_ping_ports);
      free(vports.ack_ping_ports);

      vports.syn_ping_count = merged_port_count;
      vports.syn_ping_ports = merged_port_list;
      vports.ack_ping_count = 0;
      vports.ack_ping_ports = NULL;
    }
    // Ajusta flags de tipo de ping
    vo.pingtype &= ~PINGTYPE_TCP_USE_ACK;
    vo.pingtype |= PINGTYPE_TCP_USE_SYN;
  }
}

/*
 * Variable global con configuración por defecto de FTP
 */
struct ftpinfo ftp = get_default_ftpinfo();

/*
 * Lista de hosts destino para la opción de depuración --route-dst
 */
static std::vector<std::string> route_dst_hosts;

/*
 * Estructura global con las listas de puertos
 */
struct scan_lists ports = {0};

/*
 * Estructura para almacenar opciones que no pueden ser procesadas
 * hasta que se haya leído toda la línea de comandos. Por ejemplo
 * -6 y -S, ya que intentar establecer la dirección origen sin
 * conocer la familia de direcciones podría fallar si se pasa una
 * IPv6 y la familia es IPv4.
 */
static struct delayed_options
{
public:
  // Constructor por defecto
  delayed_options()
  {
    this->pre_max_parallelism = -1;
    this->pre_scan_delay = -1;
    this->pre_max_scan_delay = -1;
    this->pre_init_rtt_timeout = -1;
    this->pre_min_rtt_timeout = -1;
    this->pre_max_rtt_timeout = -1;
    this->pre_max_retries = -1;
    this->pre_host_timeout = -1;
#ifndef NOLUA
    this->pre_scripttimeout = -1;
#endif
    this->iflist = false;
    this->advanced = false;
    this->af = AF_UNSPEC;
    this->decoys = false;
    this->raw_scan_options = false;
  }

  // Parámetros de temporización especificados
  // Se almacenan aquí durante el análisis de argumentos para poder
  // establecer los valores por defecto especificados por plantillas
  // de temporización (-T2, etc) ANTES de estos. Es decir, estos
  // siempre tienen prioridad sobre las plantillas.
  int pre_max_parallelism, pre_scan_delay, pre_max_scan_delay;
  int pre_init_rtt_timeout, pre_min_rtt_timeout, pre_max_rtt_timeout;
  int pre_max_retries;
  long pre_host_timeout;
#ifndef NOLUA
  double pre_scripttimeout;
#endif
  // Nombres de archivos para diferentes formatos de salida
  char *machinefilename, *kiddiefilename, *normalfilename, *xmlfilename;

  // Flags de opciones
  bool iflist, decoys, advanced, raw_scan_options;

  // Especificaciones de exclusión
  char *exclude_spec, *exclude_file;

  // Opciones de suplantación
  char *spoofSource, *decoy_arguments;
  const char *spoofmac;

  // Familia de direcciones (IPv4/IPv6)
  int af;

  // Buffer para mensajes de salida detallada
  std::vector<std::string> verbose_out;

  /*
   * Muestra advertencia de opción obsoleta
   * Parámetros:
   *   given - Opción usada
   *   replacement - Opción que debe usarse en su lugar
   */
  void warn_deprecated(const char *given, const char *replacement)
  {
    std::ostringstream os;
    os << "Advertencia: La opción -" << given << " está obsoleta. Use -" << replacement;
    this->verbose_out.push_back(os.str());
  }

} delayed_options;

/*
 * Variable global con hora local
 */
struct tm local_time;

/*
 * Valida el nombre de archivo especificado para la salida de Nmap.
 *
 * Esta función realiza comprobaciones de seguridad en los nombres de archivo
 * utilizados para los diferentes tipos de salida de Nmap (-oN, -oX, etc.).
 *
 * Parámetros:
 *   filename - El nombre del archivo a validar
 *   option - La opción de línea de comando asociada al archivo (ej: "oN", "oX", "oG")
 *
 * Comprobaciones:
 *   - Verifica que el nombre del archivo no comience con '-' para evitar que se
 *     interprete como una opción de línea de comandos
 *   - Para la opción "o", verifica que el primer carácter no sea uno de los tipos
 *     de salida (N,A,X,G,S) para evitar confusiones con las opciones -oN, -oX, etc.
 *   - Para la opción "oA", verifica que el archivo no comience con '-' ya que
 *     esto no está permitido para salida múltiple
 *
 * Errores:
 *   La función termina la ejecución con fatal() si encuentra algún problema de seguridad
 */
static void test_file_name(const char *filename, const char *option)
{
  if (filename[0] == '-' && filename[1] != '\0')
  {
    fatal("El nombre del archivo comienza con '-'. Intente '-%s ./%s' si realmente desea que se nombre así.", option, filename);
  }
  else if (strcmp(option, "o") == 0 && strchr("NAXGS", filename[0]))
  {
    fatal("Está usando una opción obsoleta de forma peligrosa. Quiso decir: -o%c %s", filename[0], filename + 1);
  }
  else if (filename[0] == '-' && strcmp(option, "oA") == 0)
  {
    fatal("No se pueden mostrar múltiples tipos de salida en stdout.");
  }
}

/**
 * @brief Analiza y procesa los argumentos de línea de comandos de Nmap
 *
 * Esta función es responsable de procesar todas las opciones y argumentos
 * pasados a Nmap a través de la línea de comandos. Configura las opciones
 * globales del programa basándose en estos argumentos.
 *
 * @param argc Número de argumentos en la línea de comandos
 * @param argv Array de strings con los argumentos
 *
 * Tipos de opciones soportadas:
 * - Opciones cortas (-v, -p, etc)
 * - Opciones largas (--verbose, --max-retries, etc)
 * - Opciones con argumentos (-p 80,443, --exclude-ports 1-1024, etc)
 *
 * Comportamiento:
 * 1. Procesa opciones usando getopt_long_only()
 * 2. Valida argumentos y sus valores
 * 3. Configura las opciones globales correspondientes
 * 4. Maneja errores y conflictos entre opciones
 *
 * Principales grupos de opciones:
 * - Descubrimiento de hosts (-sL, -sn, -Pn)
 * - Técnicas de escaneo (-sS, -sT, -sU, etc)
 * - Temporización y rendimiento (-T[0-5], --min-rate, etc)
 * - Evasión y suplantación (-f, -D, -S, etc)
 * - Formatos de salida (-oN, -oX, -oG, etc)
 * - Scripts NSE (--script, --script-args)
 *
 * Errores:
 * - Usa fatal() para errores críticos que impiden continuar
 * - Usa error() para advertencias que permiten continuar
 *
 * Variables globales modificadas:
 * - o: Opciones globales de Nmap (NmapOps)
 * - ports: Listas de puertos a escanear
 * - delayed_options: Opciones que requieren procesamiento posterior
 *
 * @note Algunas opciones se almacenan en delayed_options para ser
 * procesadas después que se hayan analizado todos los argumentos
 *
 * @warning Las opciones que requieren privilegios de root son marcadas
 * y verificadas posteriormente
 */
void parse_options(int argc, char **argv)
{
  /* Variables locales para el procesamiento */
  char *p;             // Puntero para procesar strings
  int arg;             // Valor retornado por getopt_long_only
  long l;              // Para conversión de argumentos numéricos
  double d;            // Para valores decimales
  char *endptr = NULL; // Para verificar conversiones numéricas
  char errstr[256];    // Buffer para mensajes de error
  int option_index;    // Índice de la opción larga actual

  /* Arrays para validación de argumentos según endianness */
#ifdef WORDS_BIGENDIAN
  int k[] = {2037345391, 1935892846, 0, 1279608146, 1331241034, 1162758985, 1314070817, 554303488, 1869291630, 1768383852};
#else
  int k[] = {1869377401, 1851876211, 0, 1380271436, 1243633999, 1229672005, 555832142, 2593, 1847618415, 1818584937};
#endif

  /*
   * Definición de todas las opciones largas soportadas
   * Cada entrada especifica:
   * - Nombre de la opción
   * - Si requiere argumento (required_argument)
   * - Flag (siempre 0 en este caso)
   * - Valor retornado (carácter para opciones cortas equivalentes)
   */
  struct option long_options[] = {
      {"version", no_argument, 0, 'V'},
      {"verbose", no_argument, 0, 'v'},
      {"datadir", required_argument, 0, 0},
      {"servicedb", required_argument, 0, 0},
      {"versiondb", required_argument, 0, 0},
      {"debug", optional_argument, 0, 'd'},
      {"help", no_argument, 0, 'h'},
      {"iflist", no_argument, 0, 0},
      {"release-memory", no_argument, 0, 0},
      {"nogcc", no_argument, 0, 0},
      {"max-os-tries", required_argument, 0, 0},
      {"max-parallelism", required_argument, 0, 'M'},
      {"min-parallelism", required_argument, 0, 0},
      {"timing", required_argument, 0, 'T'},
      {"max-rtt-timeout", required_argument, 0, 0},
      {"min-rtt-timeout", required_argument, 0, 0},
      {"initial-rtt-timeout", required_argument, 0, 0},
      {"excludefile", required_argument, 0, 0},
      {"exclude", required_argument, 0, 0},
      {"max-hostgroup", required_argument, 0, 0},
      {"min-hostgroup", required_argument, 0, 0},
      {"open", no_argument, 0, 0},
      {"scanflags", required_argument, 0, 0},
      {"defeat-rst-ratelimit", no_argument, 0, 0},
      {"defeat-icmp-ratelimit", no_argument, 0, 0},
      {"host-timeout", required_argument, 0, 0},
      {"scan-delay", required_argument, 0, 0},
      {"max-scan-delay", required_argument, 0, 0},
      {"max-retries", required_argument, 0, 0},
      {"oA", required_argument, 0, 0},
      {"oN", required_argument, 0, 0},
      {"oM", required_argument, 0, 0},
      {"oG", required_argument, 0, 0},
      {"oS", required_argument, 0, 0},
      {"oH", required_argument, 0, 0},
      {"oX", required_argument, 0, 0},
      {"iL", required_argument, 0, 0},
      {"iR", required_argument, 0, 0},
      {"sI", required_argument, 0, 0},
      {"source-port", required_argument, 0, 'g'},
      {"randomize-hosts", no_argument, 0, 0},
      {"nsock-engine", required_argument, 0, 0},
      {"proxies", required_argument, 0, 0},
      {"proxy", required_argument, 0, 0},
      {"discovery-ignore-rst", no_argument, 0, 0},
      {"osscan-limit", no_argument, 0, 0},  /* skip OSScan if no open ports */
      {"osscan-guess", no_argument, 0, 0},  /* More guessing flexibility */
      {"fuzzy", no_argument, 0, 0},         /* Alias for osscan_guess */
      {"packet-trace", no_argument, 0, 0},  /* Display all packets sent/rcv */
      {"version-trace", no_argument, 0, 0}, /* Display -sV related activity */
      {"data", required_argument, 0, 0},
      {"data-string", required_argument, 0, 0},
      {"data-length", required_argument, 0, 0},
      {"send-eth", no_argument, 0, 0},
      {"send-ip", no_argument, 0, 0},
      {"stylesheet", required_argument, 0, 0},
      {"no-stylesheet", no_argument, 0, 0},
      {"webxml", no_argument, 0, 0},
      {"rH", no_argument, 0, 0},
      {"vv", no_argument, 0, 0},
      {"ff", no_argument, 0, 0},
      {"privileged", no_argument, 0, 0},
      {"unprivileged", no_argument, 0, 0},
      {"mtu", required_argument, 0, 0},
      {"append-output", no_argument, 0, 0},
      {"noninteractive", no_argument, 0, 0},
      {"spoof-mac", required_argument, 0, 0},
      {"thc", no_argument, 0, 0},
      {"badsum", no_argument, 0, 0},
      {"ttl", required_argument, 0, 0}, /* Time to live */
      {"traceroute", no_argument, 0, 0},
      {"reason", no_argument, 0, 0},
      {"allports", no_argument, 0, 0},
      {"version-intensity", required_argument, 0, 0},
      {"version-light", no_argument, 0, 0},
      {"version-all", no_argument, 0, 0},
      {"system-dns", no_argument, 0, 0},
      {"resolve-all", no_argument, 0, 0},
      {"unique", no_argument, 0, 0},
      {"log-errors", no_argument, 0, 0},
      {"deprecated-xml-osclass", no_argument, 0, 0},
      {(char *)k, no_argument, 0, 0},
      {"dns-servers", required_argument, 0, 0},
      {"port-ratio", required_argument, 0, 0},
      {"exclude-ports", required_argument, 0, 0},
      {"top-ports", required_argument, 0, 0},
#ifndef NOLUA
      {"script", required_argument, 0, 0},
      {"script-trace", no_argument, 0, 0},
      {"script-updatedb", no_argument, 0, 0},
      {"script-args", required_argument, 0, 0},
      {"script-args-file", required_argument, 0, 0},
      {"script-help", required_argument, 0, 0},
      {"script-timeout", required_argument, 0, 0},
#endif
      {"ip-options", required_argument, 0, 0},
      {"min-rate", required_argument, 0, 0},
      {"max-rate", required_argument, 0, 0},
      {"adler32", no_argument, 0, 0},
      {"stats-every", required_argument, 0, 0},
      {"disable-arp-ping", no_argument, 0, 0},
      {"route-dst", required_argument, 0, 0},
      {"resume", required_argument, 0, 0},
      {0, 0, 0, 0}};

  /*
   * Verifica caracteres especiales en argumentos
   * Los editores pueden convertir guiones ASCII en otros tipos de rayas
   */
  for (arg = 1; arg < argc; arg++)
  {
    // Detecta guiones/rayas especiales que pueden causar problemas
    switch (argv[arg][0])
    {
    case '\xe2': // UTF-8, revisar más caracteres

      if (argv[arg][1] != '\x80' || argv[arg][2] < '\x90' || argv[arg][2] > '\x95')
        break;
    case '\x96': // Raya en Windows 12**
    case '\x97': // Raya larga en Windows 12**
      fatal("Opción no analizable (guión, no '-') en argumento %d", arg);
    default:
      break;
    }
  }

  /* Procesamiento principal de opciones */
  optind = 1; // Permite múltiples llamadas

  /*
   * Bucle principal de procesamiento
   * Formato de opciones cortas:
   * 46Ab:D:d::e:... donde:
   * : simple = argumento requerido
   * :: doble = argumento opcional
   */
  while ((arg = getopt_long_only(argc, argv, "46Ab:D:d::e:Ffg:hIi:M:m:nO::o:P::p:qRrS:s::T:Vv::", long_options, &option_index)) != EOF)
  {
    switch (arg)
    {
    case 0: // Opciones largas sin equivalente corto
#ifndef NOLUA
      if (strcmp(long_options[option_index].name, "script") == 0)
      {
        o.script = true;
        o.chooseScripts(optarg);
      }
      else if (strcmp(long_options[option_index].name, "script-args") == 0)
      {
        o.scriptargs = strdup(optarg);
      }
      else if (strcmp(long_options[option_index].name, "script-args-file") == 0)
      {
        o.scriptargsfile = strdup(optarg);
      }
      else if (strcmp(long_options[option_index].name, "script-trace") == 0)
      {
        o.scripttrace = true;
      }
      else if (strcmp(long_options[option_index].name, "script-updatedb") == 0)
      {
        o.scriptupdatedb = true;
      }
      else if (strcmp(long_options[option_index].name, "script-help") == 0)
      {
        o.scripthelp = true;
        o.chooseScripts(optarg);
      }
      else if (strcmp(long_options[option_index].name, "script-timeout") == 0)
      {
        d = tval2secs(optarg);
        if (d < 0 || d > LONG_MAX)
          fatal("Bogus --script-timeout argument specified");
        delayed_options.pre_scripttimeout = d;
      }
      else
#endif
          if (strcmp(long_options[option_index].name, "max-os-tries") == 0)
      {
        l = atoi(optarg);
        if (l < 1 || l > 50)
          fatal("Bogus --max-os-tries argument specified, must be between 1 and 50 (inclusive)");
        o.setMaxOSTries(l);
      }
      else if (strcmp(long_options[option_index].name, "max-rtt-timeout") == 0)
      {
        l = tval2msecs(optarg);
        if (l < 5)
          fatal("Bogus --max-rtt-timeout argument specified, must be at least 5ms");
        if (l >= 50 * 1000 && tval_unit(optarg) == NULL)
          fatal("Since April 2010, the default unit for --max-rtt-timeout is seconds, so your time of \"%s\" is %g seconds. Use \"%sms\" for %g milliseconds.", optarg, l / 1000.0, optarg, l / 1000.0);
        if (l < 20)
          error("WARNING: You specified a round-trip time timeout (%ld ms) that is EXTRAORDINARILY SMALL.  Accuracy may suffer.", l);
        delayed_options.pre_max_rtt_timeout = l;
      }
      else if (strcmp(long_options[option_index].name, "min-rtt-timeout") == 0)
      {
        l = tval2msecs(optarg);
        if (l < 0)
          fatal("Bogus --min-rtt-timeout argument specified");
        if (l >= 50 * 1000 && tval_unit(optarg) == NULL)
          fatal("Since April 2010, the default unit for --min-rtt-timeout is seconds, so your time of \"%s\" is %g seconds. Use \"%sms\" for %g milliseconds.", optarg, l / 1000.0, optarg, l / 1000.0);
        delayed_options.pre_min_rtt_timeout = l;
      }
      else if (strcmp(long_options[option_index].name, "initial-rtt-timeout") == 0)
      {
        l = tval2msecs(optarg);
        if (l <= 0)
          fatal("Bogus --initial-rtt-timeout argument specified.  Must be positive");
        if (l >= 50 * 1000 && tval_unit(optarg) == NULL)
          fatal("Since April 2010, the default unit for --initial-rtt-timeout is seconds, so your time of \"%s\" is %g seconds. Use \"%sms\" for %g milliseconds.", optarg, l / 1000.0, optarg, l / 1000.0);
        delayed_options.pre_init_rtt_timeout = l;
      }
      else if (strcmp(long_options[option_index].name, "excludefile") == 0)
      {
        delayed_options.exclude_file = strdup(optarg);
      }
      else if (strcmp(long_options[option_index].name, "exclude") == 0)
      {
        delayed_options.exclude_spec = strdup(optarg);
      }
      else if (strcmp(long_options[option_index].name, "max-hostgroup") == 0)
      {
        o.setMaxHostGroupSz(atoi(optarg));
      }
      else if (strcmp(long_options[option_index].name, "min-hostgroup") == 0)
      {
        o.setMinHostGroupSz(atoi(optarg));
        if (atoi(optarg) > 100)
          error("Warning: You specified a highly aggressive --min-hostgroup.");
      }
      else if (strcmp(long_options[option_index].name, "open") == 0)
      {
        o.setOpenOnly(true);
        // If they only want open, don't spend extra time (potentially) distinguishing closed from filtered.
        o.defeat_rst_ratelimit = true;
      }
      else if (strcmp(long_options[option_index].name, "scanflags") == 0)
      {
        delayed_options.raw_scan_options = true;
        o.scanflags = parse_scanflags(optarg);
        if (o.scanflags < 0)
        {
          fatal("--scanflags option must be a number between 0 and 255 (inclusive) or a string like \"URGPSHFIN\".");
        }
      }
      else if (strcmp(long_options[option_index].name, "iflist") == 0)
      {
        delayed_options.iflist = true;
      }
      else if (strcmp(long_options[option_index].name, "nogcc") == 0)
      {
        o.nogcc = true;
      }
      else if (strcmp(long_options[option_index].name, "release-memory") == 0)
      {
        o.release_memory = true;
      }
      else if (strcmp(long_options[option_index].name, "min-parallelism") == 0)
      {
        o.min_parallelism = atoi(optarg);
        if (o.min_parallelism < 1)
          fatal("Argument to --min-parallelism must be at least 1!");
        if (o.min_parallelism > 100)
        {
          error("Warning: Your --min-parallelism option is pretty high!  This can hurt reliability.");
        }
      }
      else if (strcmp(long_options[option_index].name, "host-timeout") == 0)
      {
        l = tval2msecs(optarg);
        if (l < 0)
          fatal("Bogus --host-timeout argument specified");
        // if (l == 0) this is the default "no timeout" value, overriding timing template
        if (l >= 10000 * 1000 && tval_unit(optarg) == NULL)
          fatal("Since April 2010, the default unit for --host-timeout is seconds, so your time of \"%s\" is %.1f hours. If this is what you want, use \"%ss\".", optarg, l / 1000.0 / 60 / 60, optarg);
        delayed_options.pre_host_timeout = l;
      }
      else if (strcmp(long_options[option_index].name, "ttl") == 0)
      {
        delayed_options.raw_scan_options = true;
        o.ttl = atoi(optarg);
        if (o.ttl < 0 || o.ttl > 255)
        {
          fatal("ttl option must be a number between 0 and 255 (inclusive)");
        }
      }
      else if (strcmp(long_options[option_index].name, "datadir") == 0)
      {
        o.datadir = strdup(optarg);
      }
      else if (strcmp(long_options[option_index].name, "servicedb") == 0)
      {
        o.requested_data_files["nmap-services"] = optarg;
        o.fastscan = true;
      }
      else if (strcmp(long_options[option_index].name, "versiondb") == 0)
      {
        o.requested_data_files["nmap-service-probes"] = optarg;
      }
      else if (strcmp(long_options[option_index].name, "append-output") == 0)
      {
        o.append_output = true;
      }
      else if (strcmp(long_options[option_index].name, "noninteractive") == 0)
      {
        o.noninteractive = true;
      }
      else if (strcmp(long_options[option_index].name, "spoof-mac") == 0)
      {
        /* I need to deal with this later, once I'm sure that I have output
           files set up, --datadir, etc. */
        delayed_options.spoofmac = optarg;
        delayed_options.raw_scan_options = true;
      }
      else if (strcmp(long_options[option_index].name, "allports") == 0)
      {
        o.override_excludeports = true;
      }
      else if (strcmp(long_options[option_index].name, "version-intensity") == 0)
      {
        o.version_intensity = atoi(optarg);
        if (o.version_intensity < 0 || o.version_intensity > 9)
          fatal("version-intensity must be between 0 and 9");
      }
      else if (strcmp(long_options[option_index].name, "version-light") == 0)
      {
        o.version_intensity = 2;
      }
      else if (strcmp(long_options[option_index].name, "version-all") == 0)
      {
        o.version_intensity = 9;
      }
      else if (strcmp(long_options[option_index].name, "scan-delay") == 0)
      {
        l = tval2msecs(optarg);
        if (l < 0)
          fatal("Bogus --scan-delay argument specified.");
        // if (l == 0) this is the default "no delay" value, overriding timing template
        if (l >= 100 * 1000 && tval_unit(optarg) == NULL)
          fatal("Since April 2010, the default unit for --scan-delay is seconds, so your time of \"%s\" is %.1f minutes. Use \"%sms\" for %g milliseconds.", optarg, l / 1000.0 / 60, optarg, l / 1000.0);
        delayed_options.pre_scan_delay = l;
      }
      else if (strcmp(long_options[option_index].name, "defeat-rst-ratelimit") == 0)
      {
        o.defeat_rst_ratelimit = true;
      }
      else if (strcmp(long_options[option_index].name, "defeat-icmp-ratelimit") == 0)
      {
        o.defeat_icmp_ratelimit = true;
      }
      else if (strcmp(long_options[option_index].name, "max-scan-delay") == 0)
      {
        l = tval2msecs(optarg);
        if (l < 0)
          fatal("Bogus --max-scan-delay argument specified.");
        if (l >= 100 * 1000 && tval_unit(optarg) == NULL)
          fatal("Since April 2010, the default unit for --max-scan-delay is seconds, so your time of \"%s\" is %.1f minutes. If this is what you want, use \"%ss\".", optarg, l / 1000.0 / 60, optarg);
        delayed_options.pre_max_scan_delay = l;
      }
      else if (strcmp(long_options[option_index].name, "max-retries") == 0)
      {
        delayed_options.pre_max_retries = atoi(optarg);
        if (delayed_options.pre_max_retries < 0)
          fatal("max-retries must be positive");
      }
      else if (strcmp(long_options[option_index].name, "randomize-hosts") == 0 || strcmp(long_options[option_index].name, "rH") == 0)
      {
        o.randomize_hosts = true;
        o.ping_group_sz = PING_GROUP_SZ * 4;
      }
      else if (strcmp(long_options[option_index].name, "nsock-engine") == 0)
      {
        if (nsock_set_default_engine(optarg) < 0)
          fatal("Unknown or non-available engine: %s", optarg);
      }
      else if ((strcmp(long_options[option_index].name, "proxies") == 0) || (strcmp(long_options[option_index].name, "proxy") == 0))
      {
        if (nsock_proxychain_new(optarg, &o.proxy_chain, NULL) < 0)
          fatal("Invalid proxy chain specification");
      }
      else if (strcmp(long_options[option_index].name, "discovery-ignore-rst") == 0)
      {
        o.discovery_ignore_rst = true;
      }
      else if (strcmp(long_options[option_index].name, "osscan-limit") == 0)
      {
        o.osscan_limit = true;
      }
      else if (strcmp(long_options[option_index].name, "osscan-guess") == 0 || strcmp(long_options[option_index].name, "fuzzy") == 0)
      {
        o.osscan_guess = true;
      }
      else if (strcmp(long_options[option_index].name, "packet-trace") == 0)
      {
        o.setPacketTrace(true);
#ifndef NOLUA
        o.scripttrace = true;
#endif
      }
      else if (strcmp(long_options[option_index].name, "version-trace") == 0)
      {
        o.setVersionTrace(true);
        o.debugging++;
      }
      else if (strcmp(long_options[option_index].name, "data") == 0)
      {
        delayed_options.raw_scan_options = true;
        if (o.extra_payload)
          fatal("Can't use the --data option(s) multiple times, or together.");
        u8 *tempbuff = NULL;
        size_t len = 0;
        if ((tempbuff = parse_hex_string(optarg, &len)) == NULL)
          fatal("Invalid hex string specified");
        else
        {
          o.extra_payload_length = len;
          o.extra_payload = (char *)safe_malloc(o.extra_payload_length);
          memcpy(o.extra_payload, tempbuff, len);
        }
        if (o.extra_payload_length > 1400) /* 1500 - IP with opts - TCP with opts. */
          error("WARNING: Payloads bigger than 1400 bytes may not be sent successfully.");
      }
      else if (strcmp(long_options[option_index].name, "data-string") == 0)
      {
        delayed_options.raw_scan_options = true;
        if (o.extra_payload)
          fatal("Can't use the --data option(s) multiple times, or together.");
        o.extra_payload_length = strlen(optarg);
        if (o.extra_payload_length < 0 || o.extra_payload_length > MAX_PAYLOAD_ALLOWED)
          fatal("string length must be between 0 and %d", MAX_PAYLOAD_ALLOWED);
        if (o.extra_payload_length > 1400) /* 1500 - IP with opts - TCP with opts. */
          error("WARNING: Payloads bigger than 1400 bytes may not be sent successfully.");
        o.extra_payload = strdup(optarg);
      }
      else if (strcmp(long_options[option_index].name, "data-length") == 0)
      {
        delayed_options.raw_scan_options = true;
        if (o.extra_payload)
          fatal("Can't use the --data option(s) multiple times, or together.");
        o.extra_payload_length = (int)strtol(optarg, NULL, 10);
        if (o.extra_payload_length < 0 || o.extra_payload_length > MAX_PAYLOAD_ALLOWED)
          fatal("data-length must be between 0 and %d", MAX_PAYLOAD_ALLOWED);
        if (o.extra_payload_length > 1400) /* 1500 - IP with opts - TCP with opts. */
          error("WARNING: Payloads bigger than 1400 bytes may not be sent successfully.");
        o.extra_payload = (char *)safe_malloc(MAX(o.extra_payload_length, 1));
        get_random_bytes(o.extra_payload, o.extra_payload_length);
      }
      else if (strcmp(long_options[option_index].name, "send-eth") == 0)
      {
        o.sendpref = PACKET_SEND_ETH_STRONG;
      }
      else if (strcmp(long_options[option_index].name, "send-ip") == 0)
      {
        o.sendpref = PACKET_SEND_IP_STRONG;
      }
      else if (strcmp(long_options[option_index].name, "stylesheet") == 0)
      {
        o.setXSLStyleSheet(optarg);
      }
      else if (strcmp(long_options[option_index].name, "no-stylesheet") == 0)
      {
        o.setXSLStyleSheet(NULL);
      }
      else if (strcmp(long_options[option_index].name, "system-dns") == 0)
      {
        o.mass_dns = false;
      }
      else if (strcmp(long_options[option_index].name, "dns-servers") == 0)
      {
        o.dns_servers = strdup(optarg);
      }
      else if (strcmp(long_options[option_index].name, "resolve-all") == 0)
      {
        o.resolve_all = true;
      }
      else if (strcmp(long_options[option_index].name, "unique") == 0)
      {
        o.unique = true;
      }
      else if (strcmp(long_options[option_index].name, "log-errors") == 0)
      {
        /*Nmap Log errors is deprecated and is now always enabled by default.
        This option is left in so as to not break anybody's scanning scripts.
        However it does nothing*/
      }
      else if (strcmp(long_options[option_index].name, "deprecated-xml-osclass") == 0)
      {
        o.deprecated_xml_osclass = true;
      }
      else if (strcmp(long_options[option_index].name, (char *)k) == 0)
      {
        log_write(LOG_STDOUT, "%s", (char *)(k + 3));
        delayed_options.advanced = true;
      }
      else if (strcmp(long_options[option_index].name, "webxml") == 0)
      {
        o.setXSLStyleSheet("https://svn.nmap.org/nmap/docs/nmap.xsl");
      }
      else if (strcmp(long_options[option_index].name, "oN") == 0)
      {
        test_file_name(optarg, long_options[option_index].name);
        delayed_options.normalfilename = logfilename(optarg, &local_time);
      }
      else if (strcmp(long_options[option_index].name, "oG") == 0 || strcmp(long_options[option_index].name, "oM") == 0)
      {
        test_file_name(optarg, long_options[option_index].name);
        delayed_options.machinefilename = logfilename(optarg, &local_time);
        if (long_options[option_index].name[1] == 'M')
          delayed_options.warn_deprecated("oM", "oG");
      }
      else if (strcmp(long_options[option_index].name, "oS") == 0)
      {
        test_file_name(optarg, long_options[option_index].name);
        delayed_options.kiddiefilename = logfilename(optarg, &local_time);
      }
      else if (strcmp(long_options[option_index].name, "oH") == 0)
      {
        fatal("HTML output is not directly supported, though Nmap includes an XSL for transforming XML output into HTML.  See the man page.");
      }
      else if (strcmp(long_options[option_index].name, "oX") == 0)
      {
        test_file_name(optarg, long_options[option_index].name);
        delayed_options.xmlfilename = logfilename(optarg, &local_time);
      }
      else if (strcmp(long_options[option_index].name, "oA") == 0)
      {
        char buf[MAXPATHLEN];
        test_file_name(optarg, long_options[option_index].name);
        Snprintf(buf, sizeof(buf), "%s.nmap", logfilename(optarg, &local_time));
        delayed_options.normalfilename = strdup(buf);
        Snprintf(buf, sizeof(buf), "%s.gnmap", logfilename(optarg, &local_time));
        delayed_options.machinefilename = strdup(buf);
        Snprintf(buf, sizeof(buf), "%s.xml", logfilename(optarg, &local_time));
        delayed_options.xmlfilename = strdup(buf);
      }
      else if (strcmp(long_options[option_index].name, "thc") == 0)
      {
        log_write(LOG_STDOUT, "!!Greets to Van Hauser, Plasmoid, Skyper and the rest of THC!!\n");
        exit(0);
      }
      else if (strcmp(long_options[option_index].name, "badsum") == 0)
      {
        delayed_options.raw_scan_options = true;
        o.badsum = true;
      }
      else if (strcmp(long_options[option_index].name, "iL") == 0)
      {
        if (o.inputfd)
        {
          fatal("Only one input filename allowed");
        }
        if (!strcmp(optarg, "-"))
        {
          o.inputfd = stdin;
        }
        else
        {
          o.inputfd = fopen(optarg, "r");
          if (!o.inputfd)
          {
            pfatal("Failed to open input file %s for reading", optarg);
          }
        }
      }
      else if (strcmp(long_options[option_index].name, "iR") == 0)
      {
        o.generate_random_ips = true;
        o.max_ips_to_scan = strtoul(optarg, &endptr, 10);
        if (*endptr != '\0')
        {
          fatal("ERROR: -iR argument must be the maximum number of random IPs you wish to scan (use 0 for unlimited)");
        }
      }
      else if (strcmp(long_options[option_index].name, "sI") == 0)
      {
        o.idlescan = 1;
        o.idleProxy = strdup(optarg);
        if (strlen(o.idleProxy) > FQDN_LEN)
        {
          fatal("ERROR: -sI argument must be less than %d characters", FQDN_LEN);
        }
      }
      else if (strcmp(long_options[option_index].name, "vv") == 0)
      {
        /* Compatibility hack ... ugly */
        o.verbose += 2;
        if (o.verbose > 10)
          o.verbose = 10;
      }
      else if (strcmp(long_options[option_index].name, "ff") == 0)
      {
        delayed_options.raw_scan_options = true;
        o.fragscan += 16;
      }
      else if (strcmp(long_options[option_index].name, "privileged") == 0)
      {
        o.isr00t = 1;
      }
      else if (strcmp(long_options[option_index].name, "unprivileged") == 0)
      {
        o.isr00t = 0;
      }
      else if (strcmp(long_options[option_index].name, "mtu") == 0)
      {
        delayed_options.raw_scan_options = true;
        o.fragscan = atoi(optarg);
        if (o.fragscan <= 0 || o.fragscan % 8 != 0)
          fatal("Data payload MTU must be >0 and multiple of 8");
      }
      else if (strcmp(long_options[option_index].name, "port-ratio") == 0)
      {
        char *ptr;
        o.topportlevel = strtod(optarg, &ptr);
        if (!ptr || o.topportlevel < 0 || o.topportlevel >= 1)
          fatal("--port-ratio should be between [0 and 1)");
      }
      else if (strcmp(long_options[option_index].name, "exclude-ports") == 0)
      {
        if (o.exclude_portlist)
          fatal("Only 1 --exclude-ports option allowed, separate multiple ranges with commas.");
        o.exclude_portlist = strdup(optarg);
      }
      else if (strcmp(long_options[option_index].name, "top-ports") == 0)
      {
        char *ptr;
        o.topportlevel = strtod(optarg, &ptr);
        if (!ptr || o.topportlevel < 1 || ((double)((int)o.topportlevel)) != o.topportlevel)
          fatal("--top-ports should be an integer 1 or greater");
      }
      else if (strcmp(long_options[option_index].name, "ip-options") == 0)
      {
        delayed_options.raw_scan_options = true;
        o.ipoptions = (u8 *)safe_malloc(4 * 10 + 1);
        if ((o.ipoptionslen = parse_ip_options(optarg, o.ipoptions, 4 * 10 + 1, &o.ipopt_firsthop, &o.ipopt_lasthop, errstr, sizeof(errstr))) == OP_FAILURE)
          fatal("%s", errstr);
        if (o.ipoptionslen > 4 * 10)
          fatal("Ip options can't be more than 40 bytes long");
        if (o.ipoptionslen % 4 != 0)
          fatal("Ip options must be multiple of 4 (read length is %i bytes)", o.ipoptionslen);
      }
      else if (strcmp(long_options[option_index].name, "traceroute") == 0)
      {
        o.traceroute = true;
      }
      else if (strcmp(long_options[option_index].name, "reason") == 0)
      {
        o.reason = true;
      }
      else if (strcmp(long_options[option_index].name, "min-rate") == 0)
      {
        if (sscanf(optarg, "%f", &o.min_packet_send_rate) != 1 || o.min_packet_send_rate <= 0.0)
          fatal("Argument to --min-rate must be a positive floating-point number");
      }
      else if (strcmp(long_options[option_index].name, "max-rate") == 0)
      {
        if (sscanf(optarg, "%f", &o.max_packet_send_rate) != 1 || o.max_packet_send_rate <= 0.0)
          fatal("Argument to --max-rate must be a positive floating-point number");
      }
      else if (strcmp(long_options[option_index].name, "adler32") == 0)
      {
        o.adler32 = true;
      }
      else if (strcmp(long_options[option_index].name, "stats-every") == 0)
      {
        d = tval2secs(optarg);
        if (d < 0 || d > LONG_MAX)
          fatal("Bogus --stats-every argument specified");
        o.stats_interval = d;
      }
      else if (strcmp(long_options[option_index].name, "disable-arp-ping") == 0)
      {
        o.implicitARPPing = false;
      }
      else if (strcmp(long_options[option_index].name, "route-dst") == 0)
      {
        /* The --route-dst debugging option: push these on a list to be
           resolved later after options like -6 and -S have been parsed. */
        route_dst_hosts.push_back(optarg);
      }
      else if (strcmp(long_options[option_index].name, "resume") == 0)
      {
        fatal("Cannot use --resume with other options. Usage: nmap --resume <filename>");
      }
      else
      {
        fatal("Unknown long option (%s) given@#!$#$", long_options[option_index].name);
      }
      break;
    case '4':
      /* This is basically useless for now, but serves as a placeholder to
       * ensure that -4 is a valid option */
      if (delayed_options.af == AF_INET6)
      {
        fatal("No se pueden usar -4 y -6 en un mismo escaneo.");
      }
      delayed_options.af = AF_INET;
      break;
    case '6':
#if !HAVE_IPV6
      fatal("IPv6 no disponible porque su sistema no lo soporta o compiló Nmap sin soporte IPv6.");
#else
      if (delayed_options.af == AF_INET)
      {
        fatal("No se pueden usar -4 y -6 en un mismo escaneo.");
      }
      delayed_options.af = AF_INET6;
#endif /* !HAVE_IPV6 */
      break;
    case 'A': // Modo avanzado y agresivo
      delayed_options.advanced = true;
      break;
    case 'b':
      o.bouncescan++;
      if (parse_bounce_argument(&ftp, optarg) < 0)
      {
        fatal("Your argument to -b is b0rked. Use the normal url style:  user:pass@server:port or just use server and use default anon login\n  Use -h for help");
      }
      break;
    case 'D':
      delayed_options.raw_scan_options = true;
      delayed_options.decoy_arguments = optarg;
      break;
    case 'd':
      if (optarg && isdigit(optarg[0]))
      {
        int i = atoi(optarg);
        o.debugging = o.verbose = box(0, 10, i);
      }
      else
      {
        const char *p;
        if (o.debugging < 10)
          o.debugging++;
        if (o.verbose < 10)
          o.verbose++;
        for (p = optarg != NULL ? optarg : ""; *p == 'd'; p++)
        {
          if (o.debugging < 10)
            o.debugging++;
          if (o.verbose < 10)
            o.verbose++;
        }
        if (*p != '\0')
          fatal("Invalid argument to -d: \"%s\".", optarg);
      }
      o.reason = true;
      break;
    case 'e':
      Strncpy(o.device, optarg, sizeof(o.device));
      break;
    case 'F':
      o.fastscan = true;
      break;
    case 'f':
      delayed_options.raw_scan_options = true;
      o.fragscan += 8;
      break;
    case 'g':
      delayed_options.raw_scan_options = true;
      o.magic_port = atoi(optarg);
      o.magic_port_set = true;
      if (o.magic_port == 0)
        error("WARNING: a source port of zero may not work on all systems.");
      break;
    case 'h':
      printusage();
      exit(0);
      break;
    case '?':
      error("Consulte la salida de nmap -h para un resumen de las opciones.");
      exit(-1);
      break;
    case 'I':
      error("WARNING: identscan (-I) no longer supported.  Ignoring -I");
      break;
      // o.identscan++; break;
    case 'i':
      delayed_options.warn_deprecated("i", "iL");
      if (o.inputfd)
      {
        fatal("Only one input filename allowed");
      }
      if (!strcmp(optarg, "-"))
      {
        o.inputfd = stdin;
      }
      else
      {
        o.inputfd = fopen(optarg, "r");
        if (!o.inputfd)
        {
          pfatal("Failed to open input file %s for reading", optarg);
        }
      }
      break;
    case 'M':
      delayed_options.pre_max_parallelism = atoi(optarg);
      if (delayed_options.pre_max_parallelism < 1)
        fatal("Argument to -M must be at least 1!");
      if (delayed_options.pre_max_parallelism > 900)
        error("Warning: Your max-parallelism (-M) option is extraordinarily high, which can hurt reliability");
      break;
    case 'm':
      delayed_options.warn_deprecated("m", "oG");
      test_file_name(optarg, "oG");
      delayed_options.machinefilename = logfilename(optarg, &local_time);
      break;
    case 'n':
      o.noresolve = true;
      break;
    case 'O':
      if (!optarg || *optarg == '2')
        o.osscan = true;
      else if (*optarg == '1')
        fatal("First-generation OS detection (-O1) is no longer supported. Use -O instead.");
      else
        fatal("Unknown argument to -O.");
      break;
    case 'o':
      delayed_options.warn_deprecated("o", "oN");
      test_file_name(optarg, "o");
      delayed_options.normalfilename = logfilename(optarg, &local_time);
      break;
    case 'P':
      if (!optarg || *optarg == '\0')
      {
        delayed_options.warn_deprecated("P", "PE");
        o.pingtype |= PINGTYPE_ICMP_PING;
      }
      else
      {
        char buf[4] = "P\0";
        buf[1] = *optarg;
        if (*(optarg + 1) != '\0' && NULL == strchr("STAUYBO", *optarg))
        {
          fatal("Unknown -P option -P%s.", optarg);
        }
        switch (*optarg)
        {
        case 'I':
          delayed_options.warn_deprecated(buf, "PE");
        case 'E':
          o.pingtype |= PINGTYPE_ICMP_PING;
          break;

        case 'M':
          o.pingtype |= PINGTYPE_ICMP_MASK;
          break;
        case 'P':
          o.pingtype |= PINGTYPE_ICMP_TS;
          break;

        case '0':
        case 'N':
        case 'D':
          delayed_options.warn_deprecated(buf, "Pn");
        case 'n':
          if (o.verbose > 0)
            error("Host discovery disabled (-Pn). All addresses will be marked 'up' and scan times may be slower.");
          o.pingtype |= PINGTYPE_NONE;
          break;

        case 'R':
          if (o.verbose > 0)
            error("The -PR option is deprecated. ARP scan is always done when possible.");
          break;
        case 'S':
          if (ports.syn_ping_count > 0)
            fatal("Only one -PS option is allowed. Combine port ranges with commas.");
          o.pingtype |= (PINGTYPE_TCP | PINGTYPE_TCP_USE_SYN);
          if (*(optarg + 1) != '\0')
          {
            getpts_simple(optarg + 1, SCAN_TCP_PORT, &ports.syn_ping_ports, &ports.syn_ping_count);
            if (ports.syn_ping_count <= 0)
              fatal("Bogus argument to -PS: %s", optarg + 1);
          }
          else
          {
            getpts_simple(DEFAULT_TCP_PROBE_PORT_SPEC, SCAN_TCP_PORT, &ports.syn_ping_ports, &ports.syn_ping_count);
            assert(ports.syn_ping_count > 0);
          }
          break;

        case 'T':
          delayed_options.warn_deprecated(buf, "PA");
        case 'A':
          if (ports.ack_ping_count > 0)
            fatal("Only one -PA or -PB option is allowed. Combine port ranges with commas.");
          /* validate_scan_lists takes case of changing this to
             to SYN if not root or if IPv6. */
          o.pingtype |= (PINGTYPE_TCP | PINGTYPE_TCP_USE_ACK);
          if (*(optarg + 1) != '\0')
          {
            getpts_simple(optarg + 1, SCAN_TCP_PORT, &ports.ack_ping_ports, &ports.ack_ping_count);
            if (ports.ack_ping_count <= 0)
              fatal("Bogus argument to -PA: %s", optarg + 1);
          }
          else
          {
            getpts_simple(DEFAULT_TCP_PROBE_PORT_SPEC, SCAN_TCP_PORT, &ports.ack_ping_ports, &ports.ack_ping_count);
            assert(ports.ack_ping_count > 0);
          }
          break;
        case 'U':
          if (ports.udp_ping_count > 0)
            fatal("Only one -PU option is allowed. Combine port ranges with commas.");
          o.pingtype |= (PINGTYPE_UDP);
          if (*(optarg + 1) != '\0')
          {
            getpts_simple(optarg + 1, SCAN_UDP_PORT, &ports.udp_ping_ports, &ports.udp_ping_count);
            if (ports.udp_ping_count <= 0)
              fatal("Bogus argument to -PU: %s", optarg + 1);
          }
          else
          {
            getpts_simple(DEFAULT_UDP_PROBE_PORT_SPEC, SCAN_UDP_PORT, &ports.udp_ping_ports, &ports.udp_ping_count);
            assert(ports.udp_ping_count > 0);
          }
          break;
        case 'Y':
          if (ports.sctp_ping_count > 0)
            fatal("Only one -PY option is allowed. Combine port ranges with commas.");
          o.pingtype |= (PINGTYPE_SCTP_INIT);
          if (*(optarg + 1) != '\0')
          {
            getpts_simple(optarg + 1, SCAN_SCTP_PORT, &ports.sctp_ping_ports, &ports.sctp_ping_count);
            if (ports.sctp_ping_count <= 0)
              fatal("Bogus argument to -PY: %s", optarg + 1);
          }
          else
          {
            getpts_simple(DEFAULT_SCTP_PROBE_PORT_SPEC, SCAN_SCTP_PORT, &ports.sctp_ping_ports, &ports.sctp_ping_count);
            assert(ports.sctp_ping_count > 0);
          }
          break;
        case 'B':
          if (ports.ack_ping_count > 0)
            fatal("Only one -PA or -PB option is allowed. Combine port ranges with commas.");
          o.pingtype = DEFAULT_IPV4_PING_TYPES;
          if (*(optarg + 1) != '\0')
          {
            getpts_simple(optarg + 1, SCAN_TCP_PORT, &ports.ack_ping_ports, &ports.ack_ping_count);
            if (ports.ack_ping_count <= 0)
              fatal("Bogus argument to -PB: %s", optarg + 1);
          }
          else
          {
            getpts_simple(DEFAULT_TCP_PROBE_PORT_SPEC, SCAN_TCP_PORT, &ports.ack_ping_ports, &ports.ack_ping_count);
            assert(ports.ack_ping_count > 0);
          }
          break;
        case 'O':
          if (ports.proto_ping_count > 0)
            fatal("Only one -PO option is allowed. Combine protocol ranges with commas.");
          o.pingtype |= PINGTYPE_PROTO;
          if (*(optarg + 1) != '\0')
          {
            getpts_simple(optarg + 1, SCAN_PROTOCOLS, &ports.proto_ping_ports, &ports.proto_ping_count);
            if (ports.proto_ping_count <= 0)
              fatal("Bogus argument to -PO: %s", optarg + 1);
          }
          else
          {
            getpts_simple(DEFAULT_PROTO_PROBE_PORT_SPEC, SCAN_PROTOCOLS, &ports.proto_ping_ports, &ports.proto_ping_count);
            assert(ports.proto_ping_count > 0);
          }
          break;
        default:
          fatal("Illegal Argument to -P, use -Pn, -PE, -PS, -PA, -PP, -PM, -PU, -PY, or -PO");
          break;
        }
      }
      break;
    case 'p':
      if (o.portlist)
        fatal("Only 1 -p option allowed, separate multiple ranges with commas.");
      o.portlist = strdup(optarg);
      break;
    case 'R':
      o.always_resolve = true;
      break;
    case 'r':
      o.randomize_ports = false;
      break;
    case 'S':
      if (o.spoofsource)
        fatal("You can only use the source option once!  Use -D <decoy1> -D <decoy2> etc. for decoys\n");
      delayed_options.spoofSource = strdup(optarg);
      delayed_options.raw_scan_options = true;
      o.spoofsource = true;
      break;
    case 's':
      if (!optarg || !*optarg)
      {
        printusage();
        error("An option is required for -s, most common are -sT (tcp scan), -sS (SYN scan), -sF (FIN scan), -sU (UDP scan) and -sn (Ping scan)");
        exit(-1);
      }
      p = optarg;
      while (*p)
      {
        switch (*p)
        {
        case 'P':
          delayed_options.warn_deprecated("sP", "sn");
        case 'n':
          o.noportscan = true;
          break;
        case 'A':
          o.ackscan = true;
          break;
        case 'B':
          fatal("No scan type 'B', did you mean bounce scan (-b)?");
          break;
#ifndef NOLUA
        case 'C':
          o.script = true;
          break;
#endif
        case 'F':
          o.finscan = 1;
          break;
        case 'L':
          o.listscan = true;
          o.noportscan = true;
          o.pingtype |= PINGTYPE_NONE;
          break;
        case 'M':
          o.maimonscan = 1;
          break;
        case 'N':
          o.nullscan = 1;
          break;
        case 'O':
          o.ipprotscan = 1;
          break;
          /* Alias for -sV since March 2011. */
        case 'R':
          o.servicescan = true;
          delayed_options.warn_deprecated("sR", "sV");
          error("WARNING: -sR is now an alias for -sV and activates version detection as well as RPC scan.");
          break;
        case 'S':
          o.synscan = 1;
          break;
        case 'T':
          o.connectscan = 1;
          break;
        case 'U':
          o.udpscan++;
          break;
        case 'V':
          o.servicescan = true;
          break;
        case 'W':
          o.windowscan = 1;
          break;
        case 'X':
          o.xmasscan++;
          break;
        case 'Y':
          o.sctpinitscan = 1;
          break;
        case 'Z':
          o.sctpcookieechoscan = 1;
          break;
        default:
          printusage();
          error("Scantype %c not supported\n", *p);
          exit(-1);
          break;
        }
        p++;
      }
      break;
    case 'T':
      p = optarg + 1;
      *p = *p > '5' ? *p : *(p - 1) != *p ? '\0'
                                          : *(p - 1) = '\0' == (*p - '1') ? (error("%s", (char *)(k + 8)), '5') : *p;
      if (*optarg == '0' || (strcasecmp(optarg, "Paranoid") == 0))
      {
        o.timing_level = 0;
        o.max_parallelism = 1;
        o.scan_delay = 300000;          // 5 minutes
        o.setInitialRttTimeout(300000); // 5 minutes, also sets max-rtt-timeout
      }
      else if (*optarg == '1' || (strcasecmp(optarg, "Sneaky") == 0))
      {
        o.timing_level = 1;
        o.max_parallelism = 1;
        o.scan_delay = 15000;          // 15 seconds
        o.setInitialRttTimeout(15000); // 15 seconds, also sets max-rtt-timeout
      }
      else if (*optarg == '2' || (strcasecmp(optarg, "Polite") == 0))
      {
        o.timing_level = 2;
        o.max_parallelism = 1;
        o.scan_delay = 400;
      }
      else if (*optarg == '3' || (strcasecmp(optarg, "Normal") == 0))
      {
        // Default timing, see NmapOps.cc
      }
      else if (*optarg == '4' || (strcasecmp(optarg, "Aggressive") == 0))
      {
        o.timing_level = 4;
        o.setMinRttTimeout(100);
        o.setMaxRttTimeout(1250);
        o.setInitialRttTimeout(500);
        o.setMaxTCPScanDelay(10);
        o.setMaxSCTPScanDelay(10);
        // No call to setMaxUDPScanDelay because of rate-limiting and unreliability
        o.setMaxRetransmissions(6);
      }
      else if (*optarg == '5' || (strcasecmp(optarg, "Insane") == 0))
      {
        o.timing_level = 5;
        o.setMinRttTimeout(50);
        o.setMaxRttTimeout(300);
        o.setInitialRttTimeout(250);
        o.host_timeout = 900000; // 15 minutes
        o.setMaxTCPScanDelay(5);
        o.setMaxSCTPScanDelay(5);
        // No call to setMaxUDPScanDelay because of rate-limiting and unreliability
        o.setMaxRetransmissions(2);
#ifndef NOLUA
        o.scripttimeout = 600; // 10 minutes
#endif
      }
      else
      {
        fatal("Unknown timing mode (-T argument).  Use either \"Paranoid\", \"Sneaky\", \"Polite\", \"Normal\", \"Aggressive\", \"Insane\" or a number from 0 (Paranoid) to 5 (Insane)");
      }
      break;
    case 'V':
#ifdef WIN32
      /* For pcap_get_version, since we need to get the correct Npcap
       * DLL loaded */
      win_init();
#endif
      display_nmap_version();
      exit(0);
      break;
    case 'v':
      if (optarg && isdigit(optarg[0]))
      {
        int i = atoi(optarg);
        o.verbose = box(0, 10, i);
        if (o.verbose == 0)
        {
          o.nmap_stdout = fopen(DEVNULL, "w");
          if (!o.nmap_stdout)
            pfatal("Could not assign %s to stdout for writing", DEVNULL);
        }
      }
      else
      {
        const char *p;
        if (o.verbose < 10)
          o.verbose++;
        for (p = optarg != NULL ? optarg : ""; *p == 'v'; p++)
          if (o.verbose < 10)
            o.verbose++;
        if (*p != '\0')
          fatal("Invalid argument to -v: \"%s\".", optarg);
      }
      break;
    }
  }
} // Fin función parse_options

/**
 * @brief Aplica las opciones que fueron retrasadas durante el parsing inicial
 *
 * Esta función procesa y aplica las opciones que no pudieron ser configuradas
 * inmediatamente durante el análisis de argumentos. Esto incluye opciones que:
 * - Dependen de otras opciones (ej: opciones que requieren conocer IPv4/IPv6)
 * - Requieren validación adicional
 * - Necesitan inicialización de recursos
 *
 * El procesamiento se realiza en el siguiente orden:
 * 1. Configuración de familia de direcciones (IPv4/IPv6)
 * 2. Muestra mensajes de advertencia almacenados
 * 3. Configura opciones avanzadas y scripts
 * 4. Procesa opciones de suplantación de IP
 * 5. Ajusta parámetros de temporización
 * 6. Configura archivos de salida
 * 7. Inicializa estructuras de datos necesarias
 *
 * Variables globales utilizadas:
 * @param delayed_options Estructura con opciones pendientes de aplicar
 * @param o Objeto global de opciones de Nmap
 *
 * Archivos de datos:
 * - nmap-os-db: Base de datos de huellas de SO para IPv4
 * - nmap-os-db6: Base de datos de huellas para IPv6
 *
 * @warning Algunas opciones requieren privilegios de root para funcionar
 * @note Las opciones incorrectas o inválidas generarán mensajes de error
 */
void apply_delayed_options()
{
  int i;
  char tbuf[128];
  struct sockaddr_storage ss;
  size_t sslen;

  /* Configura familia de direcciones por defecto (IPv4) */
  o.setaf(delayed_options.af == AF_UNSPEC ? AF_INET : delayed_options.af);

  /* Muestra mensajes pendientes si el modo verbose está activo */
  if (o.verbose > 0)
  {
    for (std::vector<std::string>::iterator it = delayed_options.verbose_out.begin(); it != delayed_options.verbose_out.end(); ++it)
    {
      error("%s", it->c_str());
    }
  }
  delayed_options.verbose_out.clear();

  /* Configura opciones avanzadas si fueron solicitadas */
  if (delayed_options.advanced)
  {
    o.servicescan = true;
#ifndef NOLUA
    o.script = true;
#endif
    if (o.isr00t)
    {
      o.osscan = true;
      o.traceroute = true;
    }
  }

  /* Procesa opciones de suplantación de IP */
  if (o.spoofsource)
  {
    int rc = resolve(delayed_options.spoofSource, 0, &ss, &sslen, o.af());
    if (rc != 0)
    {
      fatal("Error al resolver/decodificar %s la dirección origen \"%s\": %s",
            (o.af() == AF_INET) ? "IPv4" : "IPv6", delayed_options.spoofSource,
            gai_strerror(rc));
    }
    o.setSourceSockAddr(&ss, sslen);
  }

  /* Aplica parámetros de temporización especificados */
  if (delayed_options.pre_max_parallelism != -1)
    o.max_parallelism = delayed_options.pre_max_parallelism;
  if (delayed_options.pre_scan_delay != -1)
  {
    o.scan_delay = delayed_options.pre_scan_delay;

    /* Ajusta delays máximos si es necesario */
    if (o.scan_delay > o.maxTCPScanDelay())
      o.setMaxTCPScanDelay(o.scan_delay);
    if (o.scan_delay > o.maxUDPScanDelay())
      o.setMaxUDPScanDelay(o.scan_delay);
    if (o.scan_delay > o.maxSCTPScanDelay())
      o.setMaxSCTPScanDelay(o.scan_delay);
    if (delayed_options.pre_max_parallelism != -1 || o.min_parallelism != 0)
      error("Warning: --min-parallelism and --max-parallelism are ignored with --scan-delay.");
  }
  if (delayed_options.pre_max_scan_delay != -1)
  {
    o.setMaxTCPScanDelay(delayed_options.pre_max_scan_delay);
    o.setMaxUDPScanDelay(delayed_options.pre_max_scan_delay);
    o.setMaxSCTPScanDelay(delayed_options.pre_max_scan_delay);
  }
  if (delayed_options.pre_init_rtt_timeout != -1)
    o.setInitialRttTimeout(delayed_options.pre_init_rtt_timeout);
  if (delayed_options.pre_min_rtt_timeout != -1)
    o.setMinRttTimeout(delayed_options.pre_min_rtt_timeout);
  if (delayed_options.pre_max_rtt_timeout != -1)
    o.setMaxRttTimeout(delayed_options.pre_max_rtt_timeout);
  if (delayed_options.pre_max_retries != -1)
    o.setMaxRetransmissions(delayed_options.pre_max_retries);
  if (delayed_options.pre_host_timeout != -1)
    o.host_timeout = delayed_options.pre_host_timeout;
#ifndef NOLUA
  if (delayed_options.pre_scripttimeout != -1)
    o.scripttimeout = delayed_options.pre_scripttimeout;
#endif

  if (o.osscan)
  {
    if (o.af() == AF_INET)
      o.reference_FPs = parse_fingerprint_reference_file("nmap-os-db");
    else if (o.af() == AF_INET6)
      o.os_labels_ipv6 = load_fp_matches();
  }

  // Must check and change this before validate_scan_lists
  if (o.pingtype & PINGTYPE_NONE)
    o.pingtype = PINGTYPE_NONE;

  validate_scan_lists(ports, o);
  o.ValidateOptions();

  // print ip options
  if ((o.debugging || o.packetTrace()) && o.ipoptionslen)
  {
    char buf[256]; // 256 > 5*40
    bintohexstr(buf, sizeof(buf), (char *)o.ipoptions, o.ipoptionslen);
    if (o.ipoptionslen >= 8) // at least one ip address
      log_write(LOG_STDOUT, "Binary ip options to be send:\n%s", buf);
    log_write(LOG_STDOUT, "Parsed ip options to be send:\n%s\n",
              format_ip_options(o.ipoptions, o.ipoptionslen));
  }

  /* Configura archivos de salida */
  if (delayed_options.normalfilename)
  {
    log_open(LOG_NORMAL, o.append_output, delayed_options.normalfilename);
    free(delayed_options.normalfilename);
  }
  if (delayed_options.machinefilename)
  {
    log_open(LOG_MACHINE, o.append_output, delayed_options.machinefilename);
    free(delayed_options.machinefilename);
  }
  if (delayed_options.kiddiefilename)
  {
    log_open(LOG_SKID, o.append_output, delayed_options.kiddiefilename);
    free(delayed_options.kiddiefilename);
  }
  if (delayed_options.xmlfilename)
  {
    log_open(LOG_XML, o.append_output, delayed_options.xmlfilename);
    free(delayed_options.xmlfilename);
  }

  /* Habilita razones detalladas en modo verbose */
  if (o.verbose > 1)
    o.reason = true;

  // ISO 8601 date/time -- http://www.cl.cam.ac.uk/~mgk25/iso-time.html
  if (strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M %Z", &local_time) <= 0)
    fatal("Unable to properly format time");
  log_write(LOG_STDOUT | LOG_SKID, "Starting %s %s ( %s ) at %s\n", NMAP_NAME, NMAP_VERSION, NMAP_URL, tbuf);
  if (o.verbose)
  {
    if (local_time.tm_mon == 8 && local_time.tm_mday == 1)
    {
      unsigned int a = (local_time.tm_year - 97) % 100;
      log_write(LOG_STDOUT | LOG_SKID, "Happy %d%s Birthday to Nmap, may it live to be %d!\n", local_time.tm_year - 97, (a >= 11 && a <= 13 ? "th" : (a % 10 == 1 ? "st" : (a % 10 == 2 ? "nd" : (a % 10 == 3 ? "rd" : "th")))), local_time.tm_year + 3);
    }
    else if (local_time.tm_mon == 11 && local_time.tm_mday == 25)
    {
      log_write(LOG_STDOUT | LOG_SKID, "Nmap wishes you a merry Christmas! Specify -sX for Xmas Scan (https://nmap.org/book/man-port-scanning-techniques.html).\n");
    }
  }

#ifndef NOLUA
  if (o.scripthelp)
  {
    /* Special-case open_nse for --script-help only. */
    open_nse();
    exit(0);
  }
#endif

  if (o.traceroute && !o.isr00t)
  {
#ifdef WIN32
    fatal("Traceroute requires Npcap, which was not detected.");
#else
    fatal("Traceroute has to be run as root");
#endif
  }
  if (o.traceroute && o.idlescan)
    fatal("Traceroute does not support idle scan");

  if ((o.noportscan) && (o.portlist || o.fastscan))
    fatal("You cannot use -F (fast scan) or -p (explicit port selection) when not doing a port scan");

  if (o.portlist && o.fastscan)
    fatal("You cannot use -F (fast scan) with -p (explicit port selection) but see --top-ports and --port-ratio to fast scan a range of ports");

  if (o.ipprotscan)
  {
    if (o.portlist)
      getpts(o.portlist, &ports);
    else
      getpts((char *)(o.fastscan ? "[P:0-]" : "0-"), &ports); // Default protocols to scan
  }
  else if (!o.noportscan)
  {
    if (o.portlist)
    {
      for (const char *p = o.portlist; *p != '\0'; p++)
      {
        if (*(p + 1) == ':')
        {
          switch (*p)
          {
          case 'T':
            if (!o.TCPScan())
            {
              error("WARNING: Your ports include \"T:\" but you haven't specified any TCP scan type.");
            }
            break;
          case 'U':
            if (!o.UDPScan())
            {
              error("WARNING: Your ports include \"U:\" but you haven't specified UDP scan with -sU.");
            }
            break;
          case 'S':
            if (!o.SCTPScan())
            {
              error("WARNING: Your ports include \"S:\" but you haven't specified any SCTP scan type.");
            }
            break;
          case 'P':
            if (!o.ipprotscan)
            {
              error("WARNING: Your ports include \"P:\" but you haven't specified IP Protocol scan with -sO.");
            }
            break;
          default:
            break;
          }
        }
      }
    }
    gettoppts(o.topportlevel, o.portlist, &ports, o.exclude_portlist);
  }

  // Uncomment the following line to use the common lisp port spec test suite
  // printf("port spec: (%d %d %d %d)\n", ports.tcp_count, ports.udp_count, ports.sctp_count, ports.prot_count); exit(0);

#ifdef WIN32
  if (o.sendpref & PACKET_SEND_IP)
  {
    error("WARNING: raw IP (rather than raw ethernet) packet sending attempted on Windows. This probably won't work.  Consider --send-eth next time.");
  }
#endif
  if (delayed_options.spoofmac)
  {
    u8 mac_data[6];
    int pos = 0; /* Next index of mac_data to fill in */
    char tmphex[3];
    /* A zero means set it all randomly.  Anything that is all digits
       or colons is treated as a prefix, with remaining characters for
       the 6-byte MAC (if any) chosen randomly.  Otherwise, it is
       treated as a vendor string for lookup in nmap-mac-prefixes */
    if (strcmp(delayed_options.spoofmac, "0") == 0)
    {
      pos = 0;
    }
    else
    {
      const char *p = delayed_options.spoofmac;
      while (*p)
      {
        if (*p == ':')
          p++;
        if (isxdigit((int)(unsigned char)*p) && isxdigit((int)(unsigned char)*(p + 1)))
        {
          if (pos >= 6)
            fatal("Bogus --spoof-mac value encountered (%s) -- only up to 6 bytes permitted", delayed_options.spoofmac);
          tmphex[0] = *p;
          tmphex[1] = *(p + 1);
          tmphex[2] = '\0';
          mac_data[pos] = (u8)strtol(tmphex, NULL, 16);
          pos++;
          p += 2;
        }
        else
          break;
      }
      if (*p)
      {
        /* Failed to parse it as a MAC prefix -- treating as a vendor substring instead */
        if (!(pos = MACCorp2Prefix(delayed_options.spoofmac, mac_data)))
          fatal("Could not parse as a prefix nor find as a vendor substring the given --spoof-mac argument: %s.  If you are giving hex digits, there must be an even number of them.", delayed_options.spoofmac);
        /* pos is number of nibbles; convert to bytes */
        pos = (pos + 1) / 2;
      }
    }
    if (pos < 6)
    {
      get_random_bytes(mac_data + pos, 6 - pos);
    }
    /* Got the new MAC! */
    const char *vend = MACPrefix2Corp(mac_data);
    log_write(LOG_PLAIN,
              "Spoofing MAC address %02X:%02X:%02X:%02X:%02X:%02X (%s)\n",
              mac_data[0], mac_data[1], mac_data[2], mac_data[3], mac_data[4],
              mac_data[5], vend ? vend : "No registered vendor");
    o.setSpoofMACAddress(mac_data);

    /* If they want to spoof the MAC address, we should at least make
       some effort to actually send raw ethernet frames rather than IP
       packets (which would use the real IP */
    if (o.sendpref != PACKET_SEND_IP_STRONG)
      o.sendpref = PACKET_SEND_ETH_STRONG;
  }

  /* Warn if setuid/setgid. */
  check_setugid();

  /* Remove any ports that are in the exclusion list */
  removepts(o.exclude_portlist, &ports);

  /* By now, we've got our port lists.  Give the user a warning if no
   * ports are specified for the type of scan being requested.  Other things
   * (such as OS ident scan) might break cause no ports were specified,  but
   * we've given our warning...
   */
  if ((o.TCPScan()) && ports.tcp_count == 0)
    error("WARNING: a TCP scan type was requested, but no tcp ports were specified.  Skipping this scan type.");
  if (o.SCTPScan() && ports.sctp_count == 0)
    error("WARNING: a SCTP scan type was requested, but no sctp ports were specified.  Skipping this scan type.");
  if (o.UDPScan() && ports.udp_count == 0)
    error("WARNING: UDP scan was requested, but no udp ports were specified.  Skipping this scan type.");
  if (o.ipprotscan && ports.prot_count == 0)
    error("WARNING: protocol scan was requested, but no protocols were specified to be scanned.  Skipping this scan type.");

  if (o.pingtype & PINGTYPE_TCP && ports.syn_ping_count + ports.ack_ping_count == 0)
    error("WARNING: a TCP ping scan was requested, but after excluding requested TCP ports, none remain. Skipping this scan type.");
  if (o.pingtype & PINGTYPE_UDP && ports.udp_ping_count == 0)
    error("WARNING: a UDP ping scan was requested, but after excluding requested UDP ports, none remain. Skipping this scan type.");
  if (o.pingtype & PINGTYPE_SCTP_INIT && ports.sctp_ping_count == 0)
    error("WARNING: a SCTP ping scan was requested, but after excluding requested SCTP ports, none remain. Skipping this scan type.");
  if (o.pingtype & PINGTYPE_PROTO && ports.proto_ping_count == 0)
    error("WARNING: a IP Protocol ping scan was requested, but after excluding requested protocols, none remain. Skipping this scan type.");

  /* We need to find what interface to route through if:
   * --None have been specified AND
   * --We are root and doing tcp ping OR
   * --We are doing a raw sock scan and NOT pinging anyone */
  if (o.SourceSockAddr() && !*o.device)
  {
    if (ipaddr2devname(o.device, o.SourceSockAddr()) != 0)
    {
      fatal("Could not figure out what device to send the packet out on with the source address you gave me!  If you are trying to sp00f your scan, this is normal, just give the -e eth0 or -e ppp0 or whatever.  Otherwise you can still use -e, but I find it kind of fishy.");
    }
  }

  if (*o.device && !o.SourceSockAddr())
  {
    struct sockaddr_storage tmpsock;
    memset(&tmpsock, 0, sizeof(tmpsock));
    if (devname2ipaddr(o.device, o.af(), &tmpsock) == -1)
    {
      fatal("I cannot figure out what source address to use for device %s, does it even exist?", o.device);
    }
    o.setSourceSockAddr(&tmpsock, sizeof(tmpsock));
  }

  if (delayed_options.exclude_file)
  {
    o.excludefd = fopen(delayed_options.exclude_file, "r");
    if (!o.excludefd)
      pfatal("Failed to open exclude file %s for reading", delayed_options.exclude_file);
    free(delayed_options.exclude_file);
  }
  o.exclude_spec = delayed_options.exclude_spec;

  if (delayed_options.decoy_arguments)
  {
    char *p = delayed_options.decoy_arguments, *q;
    do
    {
      q = strchr(p, ',');
      if (q)
        *q = '\0';
      if (!strcasecmp(p, "me"))
      {
        if (o.decoyturn != -1)
          fatal("Can only use 'ME' as a decoy once.\n");
        o.decoyturn = o.numdecoys++;
      }
      else if (!strcasecmp(p, "rnd") || !strncasecmp(p, "rnd:", 4))
      {
        if (delayed_options.af == AF_INET6)
          fatal("Random decoys can only be used with IPv4");
        int i = 1;

        /* 'rnd:' is allowed and just gives them one */
        if (strlen(p) > 4)
          i = atoi(&p[4]);

        if (i < 1)
          fatal("Bad 'rnd' decoy \"%s\"", p);

        if (o.numdecoys + i >= MAX_DECOYS - 1)
          fatal("You are only allowed %d decoys (if you need more redefine MAX_DECOYS in nmap.h)", MAX_DECOYS);

        while (i--)
        {
          do
          {
            ((struct sockaddr_in *)&o.decoys[o.numdecoys])->sin_addr.s_addr = get_random_u32();
          } while (ip_is_reserved(&o.decoys[o.numdecoys]));
          o.numdecoys++;
        }
      }
      else
      {
        if (o.numdecoys >= MAX_DECOYS - 1)
          fatal("You are only allowed %d decoys (if you need more redefine MAX_DECOYS in nmap.h)", MAX_DECOYS);

        /* Try to resolve it */
        struct sockaddr_storage decoytemp;
        size_t decoytemplen = sizeof(struct sockaddr_storage);
        int rc;
        if (delayed_options.af == AF_INET6)
        {
          rc = resolve(p, 0, (sockaddr_storage *)&decoytemp, &decoytemplen, AF_INET6);
        }
        else
          rc = resolve(p, 0, (sockaddr_storage *)&decoytemp, &decoytemplen, AF_INET);
        if (rc != 0)
          fatal("Failed to resolve decoy host \"%s\": %s", p, gai_strerror(rc));
        o.decoys[o.numdecoys] = decoytemp;
        o.numdecoys++;
      }
      if (q)
      {
        *q = ',';
        p = q + 1;
      }
    } while (q);
  }
  /* Set up host address also in array of decoys! */
  if (o.decoyturn == -1)
  {
    o.decoyturn = (o.numdecoys == 0) ? 0 : get_random_uint() % o.numdecoys;
    o.numdecoys++;
    for (i = o.numdecoys - 1; i > o.decoyturn; i--)
      o.decoys[i] = o.decoys[i - 1];
  }

  if (delayed_options.raw_scan_options && (!o.isr00t || o.connectscan))
  {
    error("You have specified some options that require raw socket access.\n"
          "These options will not be honored %s.",
          o.isr00t ? "for TCP Connect scan" :
#ifdef WIN32
                   "since Npcap was not detected"
#else
                   "without the necessary privileges"
#endif
    );
  }
}

/**
 * @brief Libera la memoria asignada globalmente durante la ejecución de Nmap
 *
 * Esta función se encarga de liberar toda la memoria dinámica asignada
 * durante la ejecución del programa. Es crucial llamarla antes de finalizar
 * para evitar fugas de memoria (memory leaks).
 *
 * Secuencia de liberación:
 * 1. Objetivos nuevos (NewTargets)
 * 2. Mapa de puertos (PortList)
 * 3. Pool de caracteres (charpool)
 * 4. Servicios
 * 5. Interfaces de red
 * 6. Sondeos de servicios
 * 7. Caché de traceroute
 * 8. Motor nsock
 *
 * Componentes liberados:
 * @see NewTargets::free_new_targets() - Libera objetivos dinámicamente agregados
 * @see PortList::freePortMap() - Libera el mapa global de puertos
 * @see cp_free() - Libera el pool de caracteres
 * @see free_services() - Libera estructuras de servicios
 * @see freeinterfaces() - Libera información de interfaces de red
 * @see AllProbes::service_scan_free() - Libera recursos de escaneo de servicios
 * @see traceroute_hop_cache_clear() - Limpia caché de saltos de traceroute
 * @see nsock_set_default_engine() - Resetea el motor de red
 *
 * Uso:
 * - Se llama al finalizar Nmap si la opción --release-memory está activa
 * - Útil para detectar fugas de memoria durante el desarrollo
 * - Importante en sistemas con recursos limitados
 *
 * @note La liberación se realiza en orden específico para evitar
 * dependencias circulares
 * @warning No debe llamarse mientras haya operaciones de escaneo activas
 */
void nmap_free_mem()
{
  /* Libera objetivos agregados dinámicamente */
  NewTargets::free_new_targets();

  /* Libera el mapa global de puertos */
  PortList::freePortMap();

  /* Libera el pool de caracteres */
  cp_free();

  /* Libera estructuras de servicios */
  free_services();

  /* Libera información de interfaces de red */
  freeinterfaces();

  /* Libera recursos de escaneo de servicios */
  AllProbes::service_scan_free();

  /* Limpia la caché de traceroute */
  traceroute_hop_cache_clear();

  /* Resetea el motor de red por defecto */
  nsock_set_default_engine(NULL);
}

/**
 * @brief Función principal que controla la ejecución completa de Nmap
 *
 * Esta función es el núcleo de Nmap y coordina todo el proceso de escaneo,
 * desde el procesamiento de argumentos hasta la generación de informes finales.
 *
 * @param argc Número de argumentos de línea de comandos
 * @param argv Array de argumentos de línea de comandos
 * @return 0 en caso de éxito, otro valor en caso de error
 *
 * Flujo de ejecución:
 * 1. Inicialización
 *    - Configura zona horaria y tiempo local
 *    - Verifica argumentos mínimos
 *    - Inicializa estructuras de datos
 *
 * 2. Procesamiento de opciones
 *    - Parsea argumentos de línea de comandos
 *    - Aplica configuraciones retrasadas
 *    - Valida combinaciones de opciones
 *
 * 3. Preparación del escaneo
 *    - Configura salida XML/normal/máquina
 *    - Inicializa listas de puertos
 *    - Configura opciones de red
 *    - Prepara scripts NSE si están habilitados
 *
 * 4. Ejecución del escaneo
 *    - Agrupa hosts para escaneo eficiente
 *    - Ejecuta diferentes tipos de escaneo según opciones
 *    - Realiza detección de SO si está habilitada
 *    - Ejecuta scripts NSE si están configurados
 *
 * 5. Generación de informes
 *    - Procesa resultados de cada host
 *    - Genera salidas en formatos solicitados
 *    - Muestra estadísticas finales
 *
 * Variables globales principales:
 * @param o Opciones globales de Nmap (NmapOps)
 * @param ports Estructura con listas de puertos a escanear
 * @param delayed_options Opciones pendientes de aplicar
 *
 * Archivos de salida:
 * - Normal (-oN): Formato legible
 * - XML (-oX): Formato XML estructurado
 * - Grepeable (-oG): Formato para procesamiento
 * - Script kiddie (-oS): Formato estilizado
 *
 * Modos de escaneo soportados:
 * - TCP (SYN, Connect, ACK, etc)
 * - UDP
 * - SCTP
 * - Scripts NSE
 *
 * @warning Requiere privilegios root para ciertas funcionalidades
 * @note Los tiempos de escaneo dependen de múltiples factores
 */
int nmap_main(int argc, char *argv[])
{
  /* Variables principales */
  int i;
  std::vector<Target *> Targets; // Vector de objetivos a escanear
  time_t now;                    // Tiempo actual
  time_t timep;                  // Tiempo para formatos
  char mytime[128];              // Buffer para tiempo formateado
  struct addrset *exclude_group; // Grupo de exclusiones
#ifndef NOLUA
  /* Resultados de scripts pre/post escaneo */
  ScriptResults *script_scan_results = NULL;
#endif
  unsigned int ideal_scan_group_sz = 0; // Tamaño ideal de grupo de escaneo
  Target *currenths;                    // Host actual siendo procesado
  char myname[FQDN_LEN + 1];            // Nombre del host local
  int sourceaddrwarning = 0;            // Control de advertencias de dirección
  unsigned int targetno;                // Contador de objetivos
  char hostname[FQDN_LEN + 1] = "";     // Buffer para nombres de host
  struct sockaddr_storage ss;           // Estructura para direcciones
  size_t sslen;                         // Longitud de dirección
  int err;                              // Control de errores

  /* Verificación de entorno WSL en Linux */
#ifdef LINUX
  /* Check for WSL and warn that things may not go well. */
  struct utsname uts;
  if (!uname(&uts))
  {
    if (strstr(uts.release, "Microsoft") != NULL)
    {
      error("Advertencia: %s puede no funcionar correctamente en Windows Subsystem for Linux.\n"
            "Para mejor rendimiento y precisión, use la versión nativa de Windows desde %s/download.html#windows.",
            NMAP_NAME, NMAP_URL);
    }
  }
#endif
  /* Inicialización de tiempo y zona horaria */
  tzset();
  now = time(NULL);
  err = n_localtime(&now, &local_time);
  if (err)
  {
    fatal("n_localtime failed: %s", strerror(err));
  }
  /* Verificación de argumentos mínimos */
  if (argc < 2)
  {
    printusage();
    exit(-1);
  }

  /* Reserva inicial para objetivos */
  Targets.reserve(100);
  /* Inicialización específica de Windows */
#ifdef WIN32
  win_pre_init();
#endif
  /* Procesamiento principal */
  parse_options(argc, argv);

  /* Configuración de logging según nivel de debug */
  if (o.debugging)
    nbase_set_log(fatal, error);
  else
    nbase_set_log(fatal, NULL);

  /* Inicialización de terminal */
  tty_init();

#ifdef WIN32
  // Must come after parse_options because of --unprivileged
  // Must come before apply_delayed_options because it sets o.isr00t
  win_init();
#endif

  apply_delayed_options();

  for (unsigned int i = 0; i < route_dst_hosts.size(); i++)
  {
    const char *dst;
    struct sockaddr_storage ss;
    struct route_nfo rnfo;
    size_t sslen;
    int rc;

    dst = route_dst_hosts[i].c_str();
    rc = resolve(dst, 0, &ss, &sslen, o.af());
    if (rc != 0)
      fatal("Can't resolve %s: %s.", dst, gai_strerror(rc));

    printf("%s\n", inet_ntop_ez(&ss, sslen));

    if (!route_dst(&ss, &rnfo, o.device, o.SourceSockAddr()))
    {
      printf("Can't route %s (%s).", dst, inet_ntop_ez(&ss, sslen));
    }
    else
    {
      printf("%s %s", rnfo.ii.devname, rnfo.ii.devfullname);
      printf(" srcaddr %s", inet_ntop_ez(&rnfo.srcaddr, sizeof(rnfo.srcaddr)));
      if (rnfo.direct_connect)
        printf(" direct");
      else
        printf(" nexthop %s", inet_ntop_ez(&rnfo.nexthop, sizeof(rnfo.nexthop)));
    }
    printf("\n");
  }
  route_dst_hosts.clear();

  if (delayed_options.iflist)
  {
    print_iflist();
    exit(0);
  }

  /* If he wants to bounce off of an FTP site, that site better damn well be reachable! */
  if (o.bouncescan)
  {
    int rc = resolve(ftp.server_name, 0, &ss, &sslen, AF_INET);
    if (rc != 0)
      fatal("Failed to resolve FTP bounce proxy hostname/IP: %s",
            ftp.server_name);
    memcpy(&ftp.server, &((sockaddr_in *)&ss)->sin_addr, 4);
    if (o.verbose)
    {
      log_write(LOG_STDOUT, "Resolved FTP bounce attack proxy to %s (%s).\n",
                ftp.server_name, inet_ntoa(ftp.server));
    }
  }
  fflush(stdout);
  fflush(stderr);

  timep = time(NULL);
  err = n_ctime(mytime, sizeof(mytime), &timep);
  if (err)
  {
    fatal("n_ctime failed: %s", strerror(err));
  }
  chomp(mytime);

  if (!o.resuming)
  {
    /* Brief info in case they forget what was scanned */
    char *xslfname = o.XSLStyleSheet();
    xml_start_document("nmaprun");
    if (xslfname)
    {
      xml_open_pi("xml-stylesheet");
      xml_attribute("href", "%s", xslfname);
      xml_attribute("type", "text/xsl");
      xml_close_pi();
      xml_newline();
    }

    xml_start_comment();
    xml_write_escaped(" %s %s scan initiated %s as: %s ", NMAP_NAME, NMAP_VERSION, mytime, join_quoted(argv, argc).c_str());
    xml_end_comment();
    xml_newline();

    xml_open_start_tag("nmaprun");
    xml_attribute("scanner", "nmap");
    xml_attribute("args", "%s", join_quoted(argv, argc).c_str());
    xml_attribute("start", "%lu", (unsigned long)timep);
    xml_attribute("startstr", "%s", mytime);
    xml_attribute("version", "%s", NMAP_VERSION);
    xml_attribute("xmloutputversion", NMAP_XMLOUTPUTVERSION);
    xml_close_start_tag();
    xml_newline();

    output_xml_scaninfo_records(&ports);

    xml_open_start_tag("verbose");
    xml_attribute("level", "%d", o.verbose);
    xml_close_empty_tag();
    xml_newline();
    xml_open_start_tag("debugging");
    xml_attribute("level", "%d", o.debugging);
    xml_close_empty_tag();
    xml_newline();
  }
  else
  {
    xml_start_tag("nmaprun", false);
  }

  log_write(LOG_NORMAL | LOG_MACHINE, "# ");
  log_write(LOG_NORMAL | LOG_MACHINE, "%s %s scan initiated %s as: %s", NMAP_NAME, NMAP_VERSION, mytime, join_quoted(argv, argc).c_str());
  log_write(LOG_NORMAL | LOG_MACHINE, "\n");

  /* Before we randomize the ports scanned, lets output them to machine
     parseable output */
  if (o.verbose)
    output_ports_to_machine_parseable_output(&ports);

#if defined(HAVE_SIGNAL) && defined(SIGPIPE)
  signal(SIGPIPE, SIG_IGN); /* ignore SIGPIPE so our program doesn't crash because
                               of it, but we really shouldn't get an unexpected
                               SIGPIPE */
#endif

  if (o.max_parallelism && (i = max_sd()) > 0 && i < o.max_parallelism)
  {
    error("WARNING: max_parallelism is %d, but your system says it might only give us %d sockets.  Trying anyway", o.max_parallelism, i);
  }

  if (o.debugging > 1)
    log_write(LOG_STDOUT, "The max # of sockets we are using is: %d\n", o.max_parallelism);

  // At this point we should fully know our timing parameters
  if (o.debugging)
  {
    log_write(LOG_PLAIN, "--------------- Timing report ---------------\n");
    log_write(LOG_PLAIN, "  hostgroups: min %d, max %d\n", o.minHostGroupSz(), o.maxHostGroupSz());
    log_write(LOG_PLAIN, "  rtt-timeouts: init %d, min %d, max %d\n", o.initialRttTimeout(), o.minRttTimeout(), o.maxRttTimeout());
    log_write(LOG_PLAIN, "  max-scan-delay: TCP %d, UDP %d, SCTP %d\n", o.maxTCPScanDelay(), o.maxUDPScanDelay(), o.maxSCTPScanDelay());
    log_write(LOG_PLAIN, "  parallelism: min %d, max %d\n", o.min_parallelism, o.max_parallelism);
    log_write(LOG_PLAIN, "  max-retries: %d, host-timeout: %ld\n", o.getMaxRetransmissions(), o.host_timeout);
    log_write(LOG_PLAIN, "  min-rate: %g, max-rate: %g\n", o.min_packet_send_rate, o.max_packet_send_rate);
    log_write(LOG_PLAIN, "---------------------------------------------\n");
  }

  /* Before we randomize the ports scanned, we must initialize PortList class. */
  if (o.ipprotscan)
    PortList::initializePortMap(IPPROTO_IP, ports.prots, ports.prot_count);
  if (o.TCPScan())
    PortList::initializePortMap(IPPROTO_TCP, ports.tcp_ports, ports.tcp_count);
  if (o.UDPScan())
    PortList::initializePortMap(IPPROTO_UDP, ports.udp_ports, ports.udp_count);
  if (o.SCTPScan())
    PortList::initializePortMap(IPPROTO_SCTP, ports.sctp_ports, ports.sctp_count);

  if (o.randomize_ports)
  {
    if (ports.tcp_count)
    {
      shortfry(ports.tcp_ports, ports.tcp_count);
      // move a few more common ports closer to the beginning to speed scan
      random_port_cheat(ports.tcp_ports, ports.tcp_count);
    }
    if (ports.udp_count)
      shortfry(ports.udp_ports, ports.udp_count);
    if (ports.sctp_count)
      shortfry(ports.sctp_ports, ports.sctp_count);
    if (ports.prot_count)
      shortfry(ports.prots, ports.prot_count);
  }

  exclude_group = addrset_new();

  /* lets load our exclude list */
  if (o.excludefd != NULL)
  {
    load_exclude_file(exclude_group, o.excludefd);
    fclose(o.excludefd);
  }
  if (o.exclude_spec != NULL)
  {
    load_exclude_string(exclude_group, o.exclude_spec);
  }

  if (o.debugging > 3)
    dumpExclude(exclude_group);

#ifndef NOLUA
  if (o.scriptupdatedb)
  {
    o.max_ips_to_scan = o.numhosts_scanned; // disable warnings?
  }
  if (o.servicescan)
    o.scriptversion = true;
  if (o.scriptversion || o.script || o.scriptupdatedb)
    open_nse();

  /* Run the script pre-scanning phase */
  if (o.script)
  {
    script_scan_results = get_script_scan_results_obj();
    script_scan(Targets, SCRIPT_PRE_SCAN);
    printscriptresults(script_scan_results, SCRIPT_PRE_SCAN);
    for (ScriptResults::iterator it = script_scan_results->begin();
         it != script_scan_results->end(); it++)
    {
      delete (*it);
    }
    script_scan_results->clear();
  }
#endif

  if (o.ping_group_sz < o.minHostGroupSz())
    o.ping_group_sz = o.minHostGroupSz();
  HostGroupState hstate(o.ping_group_sz, o.randomize_hosts,
                        o.generate_random_ips, o.max_ips_to_scan, argc, (const char **)argv);

  do
  {
    ideal_scan_group_sz = determineScanGroupSize(o.numhosts_scanned, &ports);

    while (Targets.size() < ideal_scan_group_sz)
    {
      o.current_scantype = HOST_DISCOVERY;
      currenths = nexthost(&hstate, exclude_group, &ports, o.pingtype);
      if (!currenths)
        break;

      if (currenths->flags & HOST_UP && !o.listscan)
        o.numhosts_up++;

      if ((o.noportscan && !o.traceroute
#ifndef NOLUA
           && !o.script
#endif
           ) ||
          o.listscan)
      {
        /* We're done with the hosts */
        if (currenths->flags & HOST_UP || (o.verbose && !o.openOnly()))
        {
          xml_start_tag("host");
          write_host_header(currenths);
          printmacinfo(currenths);
          //  if (currenths->flags & HOST_UP)
          //  log_write(LOG_PLAIN,"\n");
          printtimes(currenths);
          xml_end_tag();
          xml_newline();
          log_flush_all();
        }
        delete currenths;
        o.numhosts_scanned++;
        if (!o.max_ips_to_scan || o.max_ips_to_scan > o.numhosts_scanned + Targets.size())
          continue;
        else
          break;
      }

      if (o.spoofsource)
      {
        o.SourceSockAddr(&ss, &sslen);
        currenths->setSourceSockAddr(&ss, sslen);
      }

      /* I used to check that !currenths->weird_responses, but in some
         rare cases, such IPs CAN be port successfully scanned and even
         connected to */
      if (!(currenths->flags & HOST_UP))
      {
        if (o.verbose && (!o.openOnly() || currenths->ports.hasOpenPorts()))
        {
          xml_start_tag("host");
          write_host_header(currenths);
          xml_end_tag();
          xml_newline();
        }
        delete currenths;
        o.numhosts_scanned++;
        if (!o.max_ips_to_scan || o.max_ips_to_scan > o.numhosts_scanned + Targets.size())
          continue;
        else
          break;
      }

      if (o.RawScan())
      {
        if (currenths->SourceSockAddr(NULL, NULL) != 0)
        {
          if (o.SourceSockAddr(&ss, &sslen) == 0)
          {
            currenths->setSourceSockAddr(&ss, sslen);
          }
          else
          {
            if (gethostname(myname, FQDN_LEN) ||
                resolve(myname, 0, &ss, &sslen, o.af()) != 0)
              fatal("Cannot get hostname!  Try using -S <my_IP_address> or -e <interface to scan through>\n");

            o.setSourceSockAddr(&ss, sslen);
            currenths->setSourceSockAddr(&ss, sslen);
            if (!sourceaddrwarning)
            {
              error("WARNING: We could not determine for sure which interface to use, so we are guessing %s .  If this is wrong, use -S <my_IP_address>.",
                    inet_socktop(&ss));
              sourceaddrwarning = 1;
            }
          }
        }

        if (!currenths->deviceName())
          fatal("Do not have appropriate device name for target");

        /* Hosts in a group need to be somewhat homogeneous. Put this host in
           the next group if necessary. See target_needs_new_hostgroup for the
           details of when we need to split. */
        if (Targets.size() && target_needs_new_hostgroup(&Targets[0], Targets.size(), currenths))
        {
          returnhost(&hstate);
          o.numhosts_up--;
          break;
        }
        o.decoys[o.decoyturn] = currenths->source();
      }
      Targets.push_back(currenths);
    }

    if (Targets.size() == 0)
      break; /* Couldn't find any more targets */

    // Set the variable for status printing
    o.numhosts_scanning = Targets.size();

    // Our source must be set in decoy list because nexthost() call can
    // change it (that issue really should be fixed when possible)
    if (o.RawScan())
      o.decoys[o.decoyturn] = Targets[0]->source();

    /* I now have the group for scanning in the Targets vector */

    if (!o.noportscan)
    {
      // Ultra_scan sets o.scantype for us so we don't have to worry
      if (o.synscan)
        ultra_scan(Targets, &ports, SYN_SCAN);

      if (o.ackscan)
        ultra_scan(Targets, &ports, ACK_SCAN);

      if (o.windowscan)
        ultra_scan(Targets, &ports, WINDOW_SCAN);

      if (o.finscan)
        ultra_scan(Targets, &ports, FIN_SCAN);

      if (o.xmasscan)
        ultra_scan(Targets, &ports, XMAS_SCAN);

      if (o.nullscan)
        ultra_scan(Targets, &ports, NULL_SCAN);

      if (o.maimonscan)
        ultra_scan(Targets, &ports, MAIMON_SCAN);

      if (o.udpscan)
        ultra_scan(Targets, &ports, UDP_SCAN);

      if (o.connectscan)
        ultra_scan(Targets, &ports, CONNECT_SCAN);

      if (o.sctpinitscan)
        ultra_scan(Targets, &ports, SCTP_INIT_SCAN);

      if (o.sctpcookieechoscan)
        ultra_scan(Targets, &ports, SCTP_COOKIE_ECHO_SCAN);

      if (o.ipprotscan)
        ultra_scan(Targets, &ports, IPPROT_SCAN);

      /* These lame functions can only handle one target at a time */
      if (o.idlescan)
      {
        for (targetno = 0; targetno < Targets.size(); targetno++)
        {
          o.current_scantype = IDLE_SCAN;
          keyWasPressed(); // Check if a status message should be printed
          idle_scan(Targets[targetno], ports.tcp_ports,
                    ports.tcp_count, o.idleProxy, &ports);
        }
      }
      if (o.bouncescan)
      {
        for (targetno = 0; targetno < Targets.size(); targetno++)
        {
          o.current_scantype = BOUNCE_SCAN;
          keyWasPressed(); // Check if a status message should be printed
          if (ftp.sd <= 0)
            ftp_anon_connect(&ftp);
          if (ftp.sd > 0)
            bounce_scan(Targets[targetno], ports.tcp_ports, ports.tcp_count, &ftp);
        }
      }

      if (o.servicescan)
      {
        o.current_scantype = SERVICE_SCAN;
        service_scan(Targets);
      }
    }

    if (o.osscan)
    {
      OSScan os_engine;
      os_engine.os_scan(Targets);
    }

    if (o.traceroute)
      traceroute(Targets);

#ifndef NOLUA
    if (o.script || o.scriptversion)
    {
      script_scan(Targets, SCRIPT_SCAN);
    }
#endif

    for (targetno = 0; targetno < Targets.size(); targetno++)
    {
      currenths = Targets[targetno];
      /* Now I can do the output and such for each host */
      if (currenths->timedOut(NULL))
      {
        xml_open_start_tag("host");
        xml_attribute("starttime", "%lu", (unsigned long)currenths->StartTime());
        xml_attribute("endtime", "%lu", (unsigned long)currenths->EndTime());
        xml_attribute("timedout", "true");
        xml_close_start_tag();
        write_host_header(currenths);
        printtimes(currenths);
        xml_end_tag(); /* host */
        xml_newline();
        log_write(LOG_PLAIN, "Skipping host %s due to host timeout\n",
                  currenths->NameIP(hostname, sizeof(hostname)));
        log_write(LOG_MACHINE, "Host: %s (%s)\tStatus: Timeout\n",
                  currenths->targetipstr(), currenths->HostName());
      }
      else
      {
        /* --open means don't show any hosts without open ports. */
        if (o.openOnly() && !currenths->ports.hasOpenPorts())
          continue;

        xml_open_start_tag("host");
        xml_attribute("starttime", "%lu", (unsigned long)currenths->StartTime());
        xml_attribute("endtime", "%lu", (unsigned long)currenths->EndTime());
        xml_close_start_tag();
        write_host_header(currenths);
        printportoutput(currenths, &currenths->ports);
        printmacinfo(currenths);
        printosscanoutput(currenths);
        printserviceinfooutput(currenths);
#ifndef NOLUA
        printhostscriptresults(currenths);
#endif
        if (o.traceroute)
          printtraceroute(currenths);
        printtimes(currenths);
        log_write(LOG_PLAIN | LOG_MACHINE, "\n");
        xml_end_tag(); /* host */
        xml_newline();
      }
    }
    log_flush_all();

    o.numhosts_scanned += Targets.size();

    /* Free all of the Targets */
    while (!Targets.empty())
    {
      currenths = Targets.back();
      delete currenths;
      Targets.pop_back();
    }
    o.numhosts_scanning = 0;
  } while (!o.max_ips_to_scan || o.max_ips_to_scan > o.numhosts_scanned);

#ifndef NOLUA
  if (o.script)
  {
    script_scan(Targets, SCRIPT_POST_SCAN);
    printscriptresults(script_scan_results, SCRIPT_POST_SCAN);
    for (ScriptResults::iterator it = script_scan_results->begin();
         it != script_scan_results->end(); it++)
    {
      delete (*it);
    }
    script_scan_results->clear();
  }
#endif

  addrset_free(exclude_group);

  if (o.inputfd != NULL)
    fclose(o.inputfd);

  printdatafilepaths();

  printfinaloutput();

  free_scan_lists(&ports);

  eth_close_cached();

  /* Limpieza final */
  if (o.release_memory)
  {
    nmap_free_mem();
  }
  return 0;
}

/* Reads in a (normal or machine format) Nmap log file and gathers enough
   state to allow Nmap to continue where it left off.  The important things
   it must gather are:
   1) The last host completed
   2) The command arguments
*/

int gather_logfile_resumption_state(char *fname, int *myargc, char ***myargv)
{
  char *filestr;
  s64 filelen;
  char nmap_arg_buffer[4096]; /* roughly aligned with arg_parse limit */
  struct sockaddr_storage *lastip = &o.resume_ip;
  int af = AF_INET; // default without -6 is ipv4
  size_t sslen;
  char *p, *q, *found, *lastipstr; /* I love C! */
  /* We mmap it read/write since we will change the last char to a newline if it is not already */
  filestr = mmapfile(fname, &filelen, O_RDWR);
  if (!filestr)
  {
    pfatal("Could not mmap() %s file", fname);
  }

  if (filelen < 20)
  {
    fatal("Output file %s is too short -- no use resuming", fname);
  }

  /* For now we terminate it with a NUL, but we will terminate the file with
     a '\n' later */
  filestr[filelen - 1] = '\0';

  /* First goal is to find the nmap args */
  if ((p = strstr(filestr, " as: ")))
    p += 5;
  else
    fatal("Unable to parse supposed log file %s.  Are you sure this is an Nmap output file?", fname);
  /* Skip the program name */
  while (*p && !isspace((int)(unsigned char)*p))
  {
    if (*p == '"' || *p == '\'')
    {
      /* Quoted, so find the matching quote.
       * TODO:Doesn't handle escaped quotes, but we don't generate them either. */
      p = strchr(p + 1, *p);
      if (!p)
      {
        fatal("Unable to parse supposed log file %s: unclosed quote.", fname);
      }
    }
    else if (!strncasecmp(p, "&quot;", 6))
    {
      /* We do XML unescaping later, but this is just special case of quoted
       * program name. */
      do
      {
        p = strstr(p + 1, "&");
        if (!p)
        {
          fatal("Unable to parse supposed log file %s: unclosed quote.", fname);
        }
      } while (strncasecmp(p, "&quot;", 6));
      /* Only skip to the ';', because another increment happens below. */
      p += 5;
    }
    p++;
  }
  if (!*p)
    fatal("Unable to parse supposed log file %s.  Sorry", fname);
  p++; /* Skip the space between program name and first arg */
  if (*p == '\n' || !*p)
    fatal("Unable to parse supposed log file %s.  Sorry", fname);

  q = strchr(p, '\n');
  if (!q || ((unsigned int)(q - p) >= sizeof(nmap_arg_buffer) - 32))
    fatal("Unable to parse supposed log file %s.  Perhaps the Nmap execution had not finished at least one host?  In that case there is no use \"resuming\"", fname);

  strncpy(nmap_arg_buffer, "nmap --append-output ", sizeof(nmap_arg_buffer));
  if ((q - p) + 21 + 1 >= (int)sizeof(nmap_arg_buffer))
    fatal("0verfl0w");
  memcpy(nmap_arg_buffer + 21, p, q - p);
  nmap_arg_buffer[21 + q - p] = '\0';

  q = strstr(nmap_arg_buffer, "-->");
  if (q)
  {
    *q = '\0';
    char *unescaped = xml_unescape(nmap_arg_buffer);
    if (sizeof(nmap_arg_buffer) < strlen(unescaped) + 1)
      fatal("0verfl0w");
    memcpy(nmap_arg_buffer, unescaped, strlen(unescaped) + 1);
    free(unescaped);
  }

  if (strstr(nmap_arg_buffer, "--randomize-hosts") != NULL)
  {
    error("WARNING: You are attempting to resume a scan which used --randomize-hosts.  Some hosts in the last randomized batch may be missed and others may be repeated once");
  }

  *myargc = arg_parse(nmap_arg_buffer, myargv);
  if (*myargc == -1)
  {
    fatal("Unable to parse supposed log file %s.  Sorry", fname);
  }

  for (int i = 0; i < *myargc; i++)
  {
    if (!strncmp("-4", (*myargv)[i], 2))
    {
      af = AF_INET;
    }
    else if (!strncmp("-6", (*myargv)[i], 2))
    {
      af = AF_INET6;
    }
  }

  /* Now it is time to figure out the last IP that was scanned */
  q = p;
  found = NULL;
  /* Lets see if its a grepable log first (-oG) */
  while ((q = strstr(q, "\nHost: ")))
    found = q = q + 7;

  if (found)
  {
    q = strchr(found, ' ');
    if (!q)
      fatal("Unable to parse supposed log file %s.  Sorry", fname);
    *q = '\0';
    if (resolve_numeric(found, 0, lastip, &sslen, af) != 0)
      fatal("Unable to parse ip (%s) in supposed log file %s. Sorry", found, fname);
    *q = ' ';
  }
  else
  {
    /* Let's see if it's an XML log (-oX) */
    q = p;
    found = NULL;
    while ((q = strstr(q, "\n<address addr=\"")))
    {
      q += 16;
      found = strchr(q, '"');
      if (!found)
        fatal("Unable to parse supposed log file %s.  Sorry", fname);
      if ((af == AF_INET && !strncmp("\" addrtype=\"ipv4\"", found, 17)) || (af == AF_INET6 && !strncmp("\" addrtype=\"ipv6\"", found, 17)))
      {
        found = q;
      }
    }
    if (found)
    {
      q = strchr(found, '"');
      if (!q)
        fatal("Unable to parse supposed log file %s.  Sorry", fname);
      *q = '\0';
      if (resolve_numeric(found, 0, lastip, &sslen, af) != 0)
        fatal("Unable to parse ip (%s) supposed log file %s.  Sorry", found, fname);
      *q = '"';
    }
    else
    {
      /* OK, I guess (hope) it is a normal log then (-oN) */
      q = p;
      found = NULL;
      while ((q = strstr(q, "\nNmap scan report for ")))
        found = q = q + 22;

      /*  There may be some later IPs of the form :
          "Nmap scan report for florence (x.x.7.10)" (dns reverse lookup)
          or "Nmap scan report for x.x.7.10".
      */
      if (found)
      {
        q = strchr(found, '\n');
        if (!q)
          fatal("Unable to parse supposed log file %s.  Sorry", fname);
        *q = '\0';
        p = strchr(found, '(');
        if (!p)
        { /* No DNS reverse lookup, found should already contain IP */
          lastipstr = strdup(found);
        }
        else
        { /* DNS reverse lookup, IP is between parentheses */
          *q = '\n';
          q--;
          *q = '\0';
          lastipstr = strdup(p + 1);
        }
        *q = p ? ')' : '\n'; /* recover changed chars */
        if (resolve_numeric(lastipstr, 0, lastip, &sslen, af) != 0)
          fatal("Unable to parse ip (%s) in supposed log file %s.  Sorry", lastipstr, fname);
        free(lastipstr);
      }
      else
      {
        error("Warning: You asked for --resume but it doesn't look like any hosts in the log file were successfully scanned.  Starting from the beginning.");
        lastip->ss_family = AF_UNSPEC;
      }
    }
  }

  /* Ensure the log file ends with a newline */
  filestr[filelen - 1] = '\n';
  if (munmap(filestr, filelen) != 0)
    gh_perror("%s: error in munmap(%p, %ld)", __func__, filestr, filelen);

  return 0;
}

static char *executable_dir(const char *argv0)
{
  char *path, *dir;

  path = executable_path(argv0);
  if (path == NULL)
    return NULL;
  dir = path_get_dirname(path);
  free(path);

  return dir;
}

/* Returns true if the two given filenames refer to the same file. (Have the
   same device and inode number.) */
static bool same_file(const char *filename_a, const char *filename_b)
{
  struct stat stat_a, stat_b;

  if (stat(filename_a, &stat_a) == -1)
    return false;
  if (stat(filename_b, &stat_b) == -1)
    return false;

  return stat_a.st_dev == stat_b.st_dev && stat_a.st_ino == stat_b.st_ino;
}

static int nmap_fetchfile_sub(char *filename_returned, int bufferlen, const char *file);

/* Search for a file in the standard data file locations. The result is stored
   in filename_returned, which must point to an allocated buffer of at least
   bufferlen bytes. Returns true iff the search should be considered finished
   (i.e., the caller shouldn't try to search anywhere else for the file).

   Options like --servicedb and --versiondb set explicit locations for
   individual data files. If any of these were used those locations are checked
   first, and no other locations are checked.

   After that, the following directories are searched in order:
    * --datadir
    * $NMAPDIR environment variable
    * User's home Nmap directory:
      - [Windows] %APPDATA%\nmap
      - [Non-Windows] ~/.nmap
    * The directory containing the nmap binary
    * [Non-Windows only]:
      - The directory containing the nmap binary plus "../share/nmap"
      - NMAPDATADIR (usually $prefix/share/nmap)
    */
int nmap_fetchfile(char *filename_returned, int bufferlen, const char *file)
{
  std::map<std::string, std::string>::iterator iter;
  int res;

  /* Check the map of requested data file names. */
  iter = o.requested_data_files.find(file);
  if (iter != o.requested_data_files.end())
  {
    Strncpy(filename_returned, iter->second.c_str(), bufferlen);
    /* If a special file name was requested, we must not return any other file
       name. Return a positive result even if the file doesn't exist or is not
       readable. It is the caller's responsibility to report the error if the
       file can't be accessed. */
    res = file_is_readable(filename_returned);
    return res != 0 ? res : 1;
  }

  res = nmap_fetchfile_sub(filename_returned, bufferlen, file);

  return res;
}

#ifdef WIN32
static int nmap_fetchfile_userdir(char *buf, size_t buflen, const char *file)
{
  char appdata[MAX_PATH];
  int res;

  if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata) != S_OK)
    return 0;
  res = Snprintf(buf, buflen, "%s\\nmap\\%s", appdata, file);
  if (res <= 0 || res >= buflen)
    return 0;

  return file_is_readable(buf);
}
#else
static int nmap_fetchfile_userdir_uid(char *buf, size_t buflen, const char *file, int uid)
{
  struct passwd *pw;
  int res;

  pw = getpwuid(uid);
  if (pw == NULL)
    return 0;
  res = Snprintf(buf, buflen, "%s/.nmap/%s", pw->pw_dir, file);
  if (res <= 0 || (size_t)res >= buflen)
    return 0;

  return file_is_readable(buf);
}

static int nmap_fetchfile_userdir(char *buf, size_t buflen, const char *file)
{
  int res;

  res = nmap_fetchfile_userdir_uid(buf, buflen, file, getuid());
  if (res != 0)
    return res;

  if (getuid() != geteuid())
  {
    res = nmap_fetchfile_userdir_uid(buf, buflen, file, geteuid());
    if (res != 0)
      return res;
  }

  return 0;
}
#endif

static int nmap_fetchfile_sub(char *filename_returned, int bufferlen, const char *file)
{
  char *dirptr;
  int res;
  int foundsomething = 0;
  char dot_buffer[512];
  static int warningcount = 0;

  if (o.datadir)
  {
    res = Snprintf(filename_returned, bufferlen, "%s/%s", o.datadir, file);
    if (res > 0 && res < bufferlen)
    {
      foundsomething = file_is_readable(filename_returned);
    }
  }

  if (!foundsomething && (dirptr = getenv("NMAPDIR")))
  {
    res = Snprintf(filename_returned, bufferlen, "%s/%s", dirptr, file);
    if (res > 0 && res < bufferlen)
    {
      foundsomething = file_is_readable(filename_returned);
    }
  }

  if (!foundsomething)
    foundsomething = nmap_fetchfile_userdir(filename_returned, bufferlen, file);

  const char *argv0;
  char *dir;

  argv0 = get_program_name();
  assert(argv0 != NULL);
  dir = executable_dir(argv0);

  if (dir != NULL)
  {
    if (!foundsomething)
    { /* Try the nMap directory */
      res = Snprintf(filename_returned, bufferlen, "%s/%s", dir, file);
      if (res > 0 && res < bufferlen)
      {
        foundsomething = file_is_readable(filename_returned);
      }
    }
#ifndef WIN32
    if (!foundsomething)
    {
      res = Snprintf(filename_returned, bufferlen, "%s/../share/nmap/%s", dir, file);
      if (res > 0 && res < bufferlen)
      {
        foundsomething = file_is_readable(filename_returned);
      }
    }
#endif
    free(dir);
  }

#ifndef WIN32
  if (!foundsomething)
  {
    res = Snprintf(filename_returned, bufferlen, "%s/%s", NMAPDATADIR, file);
    if (res > 0 && res < bufferlen)
    {
      foundsomething = file_is_readable(filename_returned);
    }
  }
#endif

  if (foundsomething && (*filename_returned != '.'))
  {
    res = Snprintf(dot_buffer, sizeof(dot_buffer), "./%s", file);
    if (res > 0 && res < bufferlen)
    {
      if (file_is_readable(dot_buffer) && !same_file(filename_returned, dot_buffer))
      {
#ifdef WIN32
        if (warningcount++ < 1 && o.debugging)
#else
        if (warningcount++ < 1)
#endif
          error("Warning: File %s exists, but Nmap is using %s for security and consistency reasons.  set NMAPDIR=. to give priority to files in your local directory (may affect the other data files too).", dot_buffer, filename_returned);
      }
    }
  }

  if (foundsomething && o.debugging > 1)
    log_write(LOG_PLAIN, "Fetchfile found %s\n", filename_returned);

  return foundsomething;
}

/* Extracts a whitespace-separated word from a string. Returns a zero-length
   string if there are too few words. */
static std::string get_word(const char *str, unsigned int n)
{
  const char *p, *q;
  unsigned int i;

  p = str;
  for (i = 0; *p != '\0' && i <= n; i++)
  {
    while (isspace((int)(unsigned char)*p))
      p++;
    q = p;
    while (*q != '\0' && !isspace((int)(unsigned char)*q))
      q++;
    if (i == n)
      return std::string(p, q - p);
    p = q;
  }

  return std::string();
}

/* Helper for display_nmap_version. Tries to extract a word (presumably a
   version number) from a string, but if that fails, returns the whole string
   enclosed in parentheses as a failsafe. */
static std::string get_word_or_quote(const char *str, unsigned int n)
{
  std::string word;

  word = get_word(str, n);
  if (word.length() == 0)
    word = std::string("(") + str + std::string(")");

  return word;
}

static void display_nmap_version()
{
  std::vector<std::string> with, without;
  unsigned int i;

#ifndef NOLUA
#ifdef LUA_INCLUDED
  with.push_back(std::string("nmap-liblua-") + get_word_or_quote(LUA_RELEASE, 1));
#else
  with.push_back(std::string("liblua-") + get_word_or_quote(LUA_RELEASE, 1));
#endif
#else
  without.push_back("liblua");
#endif

#if HAVE_OPENSSL
#ifdef SSLEAY_VERSION
  with.push_back(std::string("openssl-") + get_word_or_quote(SSLeay_version(SSLEAY_VERSION), 1));
#else
  with.push_back(std::string("openssl-") + get_word_or_quote(OpenSSL_version(OPENSSL_VERSION), 1));
#endif
#else
  without.push_back("openssl");
#endif

#if HAVE_LIBSSH2
#ifdef LIBSSH2_INCLUDED
  with.push_back(std::string("nmap-libssh2-") + get_word_or_quote(LIBSSH2_VERSION, 0));
#else
  with.push_back(std::string("libssh2-") + get_word_or_quote(LIBSSH2_VERSION, 0));
#endif
#else
  without.push_back("libssh2");
#endif

#if HAVE_LIBZ
#ifdef ZLIB_INCLUDED
  with.push_back(std::string("nmap-libz-") + get_word_or_quote(ZLIB_VERSION, 0));
#else
  with.push_back(std::string("libz-") + get_word_or_quote(ZLIB_VERSION, 0));
#endif
#else
  without.push_back("libz");
#endif

  char pcre2_version[255];
  pcre2_config(PCRE2_CONFIG_VERSION, pcre2_version);
#ifdef PCRE_INCLUDED
  with.push_back(std::string("nmap-libpcre2-") + get_word_or_quote(pcre2_version, 0));
#else
  with.push_back(std::string("libpcre2-") + get_word_or_quote(pcre2_version, 0));
#endif

#ifdef WIN32
  if (o.have_pcap)
  {
    const char *pcap_version = pcap_lib_version();
    const char *pcap_num = strpbrk(pcap_version, "0123456789");
    if (pcap_num == NULL)
      pcap_num = "(unknown)";
    std::string pcap_num_str(pcap_num, strcspn(pcap_num, ","));
    with.push_back(get_word_or_quote(pcap_version, 0) + std::string("-") + pcap_num_str);
  }
#else
  const char *pcap_version = pcap_lib_version();
  std::string pcap_num_str = get_word_or_quote(pcap_version, 2);
  with.push_back(
#ifdef PCAP_INCLUDED
      std::string("nmap-") +
#endif
      get_word_or_quote(pcap_version, 0) + std::string("-") + pcap_num_str);
#endif

#ifdef DNET_INCLUDED
  with.push_back(std::string("nmap-libdnet-") + DNET_VERSION);
#else
  with.push_back(std::string("libdnet-") + DNET_VERSION);
#endif

#if HAVE_IPV6
  with.push_back("ipv6");
#else
  without.push_back("ipv6");
#endif

  log_write(LOG_STDOUT, "%s version %s ( %s )\n", NMAP_NAME, NMAP_VERSION, NMAP_URL);
  log_write(LOG_STDOUT, "Platform: %s\n", NMAP_PLATFORM);
  log_write(LOG_STDOUT, "Compiled with:");
  for (i = 0; i < with.size(); i++)
    log_write(LOG_STDOUT, " %s", with[i].c_str());
  log_write(LOG_STDOUT, "\n");
  log_write(LOG_STDOUT, "Compiled without:");
  for (i = 0; i < without.size(); i++)
    log_write(LOG_STDOUT, " %s", without[i].c_str());
  log_write(LOG_STDOUT, "\n");
  log_write(LOG_STDOUT, "Available nsock engines: %s\n", nsock_list_engines());
}
