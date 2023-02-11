/***************************************************************************//**
 * @file    lwipopts_default.h
 * @brief   Arduino RTT-Ethernet library LwIP default options header
 * @author  onelife <onelife.real[at]gmail.com>
 ******************************************************************************/
#ifndef __LWIPOPTS_DEFAULT_H__
#define __LWIPOPTS_DEFAULT_H__

/* ---------- Debug options ---------- */
#define LWIP_DEBUG
// #define SOCKETS_DEBUG                   LWIP_DBG_ON
// #define NETIF_DEBUG                     LWIP_DBG_ON
// #define HTTPD_DEBUG                     LWIP_DBG_ON
// #define TCPIP_DEBUG                     LWIP_DBG_ON
// #define PBUF_DEBUG                      LWIP_DBG_ON
// #define MEMP_DEBUG                      LWIP_DBG_ON
// #define TCP_DEBUG                       LWIP_DBG_ON
// #define TCP_OUTPUT_DEBUG                LWIP_DBG_ON
// #define ALTCP_WOLFSSL_DEBUG             LWIP_DBG_ON
// #define ALTCP_WOLFSSL_MEM_DEBUG         LWIP_DBG_ON
#define LOG_LVL LOG_LVL_INFO

/*
#define LWIP_PLATFORM_ASSERT(x) \
  do {LOG_E("Assertion \"%s\" failed at line %d in %s\n", \
    x, __LINE__, __FILE__); } while(0)
*/

#define LWIP_MARK_TCPIP_THREAD() { \
  extern uint32_t tcpip_started; \
  tcpip_started = 1; \
}

#define LWIP_ASSERT_CORE_LOCKED() { \
  extern void *lock_tcpip_core; \
  extern uint32_t tcpip_started; \
  if (tcpip_started && !*((uint8_t *)lock_tcpip_core + 39)) { \
    rt_kprintf("!!! CORE_LOCK: %x\n", *((uint8_t *)lock_tcpip_core + 39)); \
    LWIP_ASSERT("TCPIP core lock is not held", (tcpip_started && *((uint8_t *)lock_tcpip_core + 39))); \
  } \
}

#define portNOP()                         asm volatile ( "nop" )

#ifdef __cplusplus
extern "C" {
#endif
extern int rt_kprintf(const char *fmt, ...);
#ifdef __cplusplus
}
#endif
#define LWIP_PLATFORM_DIAG(x)             do {rt_kprintf x;} while(0)

/* ---------- Features ---------- */
#define LWIP_ICMP                         1
#define LWIP_RAW                          1 /* For PING */
#define LWIP_DHCP                         1
#define LWIP_DNS                          1
#define LWIP_UDP                          1
#define LWIP_TCP                          1
#define LWIP_NETIF_API                    1
// #define LWIP_STATS                      0

// #define LWIP_NOASSERT
// #define LWIP_PROVIDE_ERRNO                /* defined in "cc.h" */

/* ---------- TCP options ---------- */
#define LWIP_SO_RCVTIMEO                  1
#define LWIP_SO_SNDTIMEO                  1
#define LWIP_SO_SNDRCVTIMEO_NONSTANDARD   1

#define TCP_QUEUE_OOSEQ                   0
/* TCP_MSS = (Ethernet MTU - IP header size - TCP header size) */
#define TCP_MSS                           (1500 - 40)
/* TCP receive window. */
#define TCP_WND                           (4 * TCP_MSS)
#define TCP_SND_BUF                       (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                  (4 * TCP_SND_BUF / TCP_MSS)

// #define TCP_OVERSIZE                      1
#define LWIP_WND_SCALE                    1
#define TCP_RCV_SCALE                     1

/* ---------- NetIF options ---------- */
#define LWIP_SINGLE_NETIF                 1
#define LWIP_NETIF_HOSTNAME               1
#define LWIP_NETIF_STATUS_CALLBACK        1
#define LWIP_NETIF_LINK_CALLBACK          1

/* ---------- Socket options ---------- */
#define LWIP_TCP_KEEPALIVE                1 /* Important for MQTT/TLS */

/* ---------- SYS options ---------- */
#define TCPIP_THREAD_STACKSIZE            (512 * 3)
#define ETHERNET_THREAD_STACKSIZE         (512 * 2)
/**
 * In CMSIS OS API, bigger priority number means higher priority.
 * In RT-Thread, smaller priority number means higher priority.
 * This library depends on STM32duino LwIP library, which used CMSIS OS API for 
 * "SYS" related functions. So the priorities below following CMSIS OS API way.
 */
#define TCPIP_THREAD_PRIO                 (56 - 6)
#define ETHERNET_THREAD_PRIO              (56 - 7)

#define TCPIP_MBOX_SIZE                   8

#define DEFAULT_RAW_RECVMBOX_SIZE         8 /* for ICMP PING */
#define DEFAULT_UDP_RECVMBOX_SIZE         8
#define DEFAULT_TCP_RECVMBOX_SIZE         8
#define DEFAULT_ACCEPTMBOX_SIZE           8

/* ---------- Memory options ---------- */
#define MEMP_MEM_INIT                     1
#define MEM_ALIGNMENT                     4
// #define MEMP_OVERFLOW_CHECK               1
#define MEM_USE_POOLS                     1
#define MEMP_USE_CUSTOM_POOLS             1
// #define MEM_USE_POOLS_TRY_BIGGER_POOL   1

#define MEMP_NUM_PBUF                     16
#define MEMP_NUM_UDP_PCB                  16
#define MEMP_NUM_TCP_PCB                  16
#define MEMP_NUM_TCP_PCB_LISTEN           4
#define MEMP_NUM_TCP_SEG                  16 //(TCP_SND_QUEUELEN)
#define MEMP_NUM_SYS_TIMEOUT              8
#define MEMP_NUM_NETBUF                   8
#define MEMP_NUM_TCPIP_MSG_API            8
#define MEMP_NUM_TCPIP_MSG_INPKT          8 /* For incoming pkg */

/* ---------- Pbuf options ---------- */
#define PBUF_POOL_SIZE                    16
#define LWIP_SUPPORT_CUSTOM_PBUF          1
#define LWIP_NETIF_TX_SINGLE_PBUF         1

/* ---------- Checksum options ---------- */
#define CHECKSUM_GEN_IP                   0
#define CHECKSUM_GEN_UDP                  0
#define CHECKSUM_GEN_TCP                  0
#define CHECKSUM_GEN_ICMP                 0
#define CHECKSUM_CHECK_IP                 0
#define CHECKSUM_CHECK_UDP                0
#define CHECKSUM_CHECK_TCP                0
#define CHECKSUM_CHECK_ICMP               0

/* ---------- httpd options ---------- */
#define LWIP_HTTPD_CGI                    0
#define LWIP_HTTPD_SSI                    0
#define HTTPD_USE_CUSTOM_FSDATA           1
// #define CONFIG_NO_FCNTL

/* ---------- Other options ---------- */
#define ETHERNET_RMII_MODE_CONFIGURATION  1
#define ETH_INPUT_USE_IT                  1

#endif /* __LWIPOPTS_DEFAULT_H__ */
