/***************************************************************************
 * nmap.h -- Actualmente maneja algunas de las características de escaneo   *
 * de puertos de Nmap, así como la interfaz de línea de comandos para el   *
 * usuario. Tenga en cuenta que la función main() real está en main.c      *
 *                                                                         *
 ***********************TÉRMINOS IMPORTANTES DE LICENCIA DE NMAP*************
 *
 * El Escáner de Seguridad Nmap es (C) 1996-2025 Nmap Software LLC ("El Proyecto
 * Nmap"). Nmap también es una marca registrada del Proyecto Nmap.
 *
 * Este programa se distribuye bajo los términos de la Licencia Pública de
 * Código Fuente de Nmap (NPSL). El texto exacto de la licencia que aplica a una
 * versión particular de Nmap o revisión del control de código fuente está
 * contenido en el archivo LICENSE distribuido con esa versión de Nmap o
 * revisión del control de código fuente. Más información sobre derechos de
 * autor/legal de Nmap está disponible en https://nmap.org/book/man-legal.html,
 * y más información sobre la licencia NPSL se puede encontrar en
 * https://nmap.org/npsl/. Este encabezado resume algunos puntos clave de la
 * licencia de Nmap, pero no sustituye al texto real de la licencia.
 *
 * Nmap es generalmente gratuito para que los usuarios finales lo descarguen
 * y lo utilicen, incluso para uso comercial. Está disponible en https://nmap.org.
 *
 * La licencia de Nmap generalmente prohíbe a las empresas usar y redistribuir
 * Nmap en productos comerciales, pero vendemos una Edición OEM especial de
 * Nmap con una licencia más permisiva y características especiales para este
 * propósito. Consulte https://nmap.org/oem/
 *
 * Si ha recibido un acuerdo de licencia escrito de Nmap o un contrato que
 * establece términos diferentes a estos (como una licencia OEM de Nmap), puede
 * optar por usar y redistribuir Nmap bajo esos términos en su lugar.
 *
 * Las compilaciones oficiales de Nmap para Windows incluyen el software Npcap
 * (https://npcap.com) para la captura y transmisión de paquetes. Está bajo
 * términos de licencia separados que prohíben la redistribución sin permiso
 * especial. Por lo tanto, las compilaciones oficiales de Nmap para Windows no
 * pueden redistribuirse sin permiso especial (como una licencia OEM de Nmap).
 *
 * El código fuente se proporciona para este software porque creemos que los
 * usuarios tienen derecho a saber exactamente qué va a hacer un programa
 * antes de ejecutarlo. Esto también le permite auditar el software en busca
 * de vulnerabilidades de seguridad.
 *
 * El código fuente también le permite portar Nmap a nuevas plataformas,
 * corregir errores y agregar nuevas características. Se le anima enormemente
 * a enviar sus cambios como PR de Github o por correo electrónico a la lista
 * de correo dev@nmap.org para su posible incorporación a la distribución
 * principal. A menos que especifique lo contrario, se entiende que nos está
 * ofreciendo derechos muy amplios para usar sus envíos como se describe en
 * el Acuerdo de Contribución de la Licencia Pública de Código Fuente de Nmap.
 * Esto es importante porque financiamos el proyecto vendiendo licencias con
 * varios términos, y también porque la incapacidad de relicenciar código ha
 * causado problemas devastadores para otros proyectos de Software Libre
 * (como KDE y NASM).
 *
 * La versión gratuita de Nmap se distribuye con la esperanza de que sea
 * útil, pero SIN NINGUNA GARANTÍA; sin siquiera la garantía implícita de
 * COMERCIABILIDAD o IDONEIDAD PARA UN PROPÓSITO PARTICULAR. Las garantías,
 * indemnización y soporte comercial están disponibles a través del programa
 * OEM de Npcap--consulte https://nmap.org/oem/
 *
 ***************************************************************************/

/* $Id$ */

#ifndef NMAP_H
#define NMAP_H

/************************INCLUDES**********************************/

#ifdef HAVE_CONFIG_H
#include "nmap_config.h"
#else
#ifdef WIN32
#include "nmap_winconfig.h"
#endif /* WIN32 */
#endif /* HAVE_CONFIG_H */

#ifdef __amigaos__
#include "nmap_amigaos.h"
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_BSTRING_H
#include <bstring.h>
#endif

/* Mantenga assert() definido por razones de seguridad */
#undef NDEBUG

#include <assert.h>

/*#include <net/if_arp.h> */ /* defines struct arphdr needed for if_ether.h */
// #if HAVE_NET_IF_H
// #ifndef NET_IF_H  /* why doesn't OpenBSD do this?! */
// #include <net/if.h>
// #define NET_IF_H
// #endif
// #endif
// #if HAVE_NETINET_IF_ETHER_H
// #ifndef NETINET_IF_ETHER_H
// #include <netinet/if_ether.h>
// #define NETINET_IF_ETHER_H
// #endif /* NETINET_IF_ETHER_H */
// #endif /* HAVE_NETINET_IF_ETHER_H */

/******* DEFINICIONES ************/

#ifdef NMAP_OEM
#include "../nmap-build/nmap-oem.h"
#endif

/* Nombre por defecto si no está definido */
#ifndef NMAP_NAME
#define NMAP_NAME "Nmap"
#endif
#define NMAP_URL "https://nmap.org"

/* Macros para convertir valores a cadenas */
#define _STR(X) #X
#define STR(X) _STR(X)

/* Definición de versiones si no están especificadas */
#ifndef NMAP_VERSION
/* Editar esta definición solo dentro de las comillas, ya que es leída
   por los makefiles */
#define NMAP_MAJOR 7  /* Versión mayor */
#define NMAP_MINOR 95 /* Versión menor */
#define NMAP_BUILD 1  /* Número de compilación */
/* SVN, BETA, etc. */
#define NMAP_SPECIAL "SVN"

/* Construcción de cadenas de versión */
#define NMAP_VERSION STR(NMAP_MAJOR) "." STR(NMAP_MINOR) NMAP_SPECIAL
#define NMAP_NUM_VERSION STR(NMAP_MAJOR) "." STR(NMAP_MINOR) "." STR(NMAP_BUILD) ".0"
#endif

/* Versión del formato de salida XML */
#define NMAP_XMLOUTPUTVERSION "1.05"

/* Definiciones configurables por el usuario: */
#define MAX_PROBE_PORTS 10 /* Número máximo de puertos TCP de sondeo permitidos */

/* Número predeterminado de puertos en paralelo. No siempre involucra sockets reales.
   Se puede ajustar con la opción -M en línea de comandos. */
#define MAX_SOCKETS 36

/* Número máximo de intentos de conexión que agotan el tiempo de espera
   antes de decidir que el host está caído */
#define MAX_TIMEOUTS MAX_SOCKETS

/* Puerto predeterminado para sondeos TCP si no se especifica por el usuario */
#define DEFAULT_TCP_PROBE_PORT 80
#define DEFAULT_TCP_PROBE_PORT_SPEC STR(DEFAULT_TCP_PROBE_PORT)

/* Puerto predeterminado para sondeos UDP */
#define DEFAULT_UDP_PROBE_PORT 40125
#define DEFAULT_UDP_PROBE_PORT_SPEC STR(DEFAULT_UDP_PROBE_PORT)

/* Puerto predeterminado para sondeos SCTP */
#define DEFAULT_SCTP_PROBE_PORT 80
#define DEFAULT_SCTP_PROBE_PORT_SPEC STR(DEFAULT_SCTP_PROBE_PORT)

/* Sondeos IPProto predeterminados si no se especifican */
#define DEFAULT_PROTO_PROBE_PORT_SPEC "1,2,4"

/* Número máximo de señuelos permitidos */
#define MAX_DECOYS 128

/* Opciones TCP para sondeos TCP SYN: MSS 1460 */
#define TCP_SYN_PROBE_OPTIONS "\x02\x04\x05\xb4"
#define TCP_SYN_PROBE_OPTIONS_LEN (sizeof(TCP_SYN_PROBE_OPTIONS) - 1)

/* Retardos máximos predeterminados entre sondeos al mismo host */
#ifndef MAX_TCP_SCAN_DELAY
#define MAX_TCP_SCAN_DELAY 1000
#endif

#ifndef MAX_UDP_SCAN_DELAY
#define MAX_UDP_SCAN_DELAY 1000
#endif

#ifndef MAX_SCTP_SCAN_DELAY
#define MAX_SCTP_SCAN_DELAY 1000
#endif

/* Número máximo de campos de información extra de servicio */
#define MAX_SERVICE_INFO_FIELDS 5

/* Tiempos de espera mínimo y máximo para respuestas */
#ifndef MIN_RTT_TIMEOUT
#define MIN_RTT_TIMEOUT 100
#endif

#ifndef MAX_RTT_TIMEOUT
#define MAX_RTT_TIMEOUT 10000 /* Nunca permitir más de 10 segundos para el viaje de ida y vuelta */
#endif

/* Tiempos de espera iniciales */
#define INITIAL_RTT_TIMEOUT 1000    /* 1 segundo inicial para respuestas */
#define INITIAL_ARP_RTT_TIMEOUT 200 /* Tiempo inicial más bajo para ARP */

/* Número máximo de retransmisiones */
#ifndef MAX_RETRANSMISSIONS
#define MAX_RETRANSMISSIONS 10 /* Máximo 11 sondeos por puerto */
#endif

/* Tamaño del grupo de hosts para pre-ping */
#define PING_GROUP_SZ 4096

/* NO MODIFICAR nada después de este punto */
/* Macros y definiciones de utilidad */
#define UC(b) (((int)b) & 0xff)
#define SA struct sockaddr /* Técnica de R. Stevens */

/* Estados de host */
#define HOST_UNKNOWN 0
#define HOST_UP 1
#define HOST_DOWN 2

/* Tipos de ping */
#define PINGTYPE_UNKNOWN 0
#define PINGTYPE_NONE 1
#define PINGTYPE_ICMP_PING 2
#define PINGTYPE_ICMP_MASK 4
#define PINGTYPE_ICMP_TS 8
#define PINGTYPE_TCP 16
#define PINGTYPE_TCP_USE_ACK 32
#define PINGTYPE_TCP_USE_SYN 64
/* # define PINGTYPE_RAWTCP 128 used to be here, but was never used. */
#define PINGTYPE_CONNECTTCP 256
#define PINGTYPE_UDP 512
/* #define PINGTYPE_ARP 1024 // Not used; see o.implicitARPPing */
#define PINGTYPE_PROTO 2048
#define PINGTYPE_SCTP_INIT 4096

/* Combinaciones óptimas determinadas empíricamente para diferentes tipos de sondeo:
     -PE
     -PE -PA80
     -PE -PA80 -PS443
     -PE -PA80 -PS443 -PP
     -PE -PA80 -PS443 -PP -PU40125
   Utilizamos la combinación de cuatro sondas. */
#define DEFAULT_IPV4_PING_TYPES (PINGTYPE_ICMP_PING | PINGTYPE_TCP | PINGTYPE_TCP_USE_ACK | PINGTYPE_TCP_USE_SYN | PINGTYPE_ICMP_TS)
#define DEFAULT_IPV6_PING_TYPES (PINGTYPE_ICMP_PING | PINGTYPE_TCP | PINGTYPE_TCP_USE_ACK | PINGTYPE_TCP_USE_SYN)
#define DEFAULT_PING_ACK_PORT_SPEC "80"
#define DEFAULT_PING_SYN_PORT_SPEC "443"
/* Para no raíz. */
#define DEFAULT_PING_CONNECT_PORT_SPEC "80,443"

/* Longitud máxima de línea para huella digital */
#define FP_RESULT_WRAP_LINE_LEN 74

/* Longitud máxima de nombre DNS */
#define FQDN_LEN 254

/* Carga útil máxima considerando IPv4 con 40 bytes de opciones y TCP con 20 bytes de opciones */
#define MAX_PAYLOAD_ALLOWED 65535 - 60 - 40

#ifndef recvfrom6_t
#define recvfrom6_t int
#endif

/***********************PROTOTIPOS**********************************/

/* Renombrado main para que el modo interactivo pueda preprocesar cuando sea necesario */
int nmap_main(int argc, char *argv[]);

/* Funciones de utilidad */
int nmap_fetchfile(char *filename_returned, int bufferlen, const char *file);
int gather_logfile_resumption_state(char *fname, int *myargc, char ***myargv);

#endif /* NMAP_H */
