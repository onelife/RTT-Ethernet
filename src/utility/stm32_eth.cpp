/**
  ******************************************************************************
  * @file    stm32_eth.cpp
  * @author  WI6LABS
  * @version V1.0.0
  * @date    24-May-2017
  * @brief   This file implements Ethernet network interface drivers for
  *          Arduino STM32Ethernet library.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

#include "Arduino.h"
#include "stm32_eth.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "ethernetif.h"
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"
#include "lwip/dns.h"

#define LOG_TAG "STM_ETH"
#include <log.h>

/* Check ethernet link status every seconds */
#define TIME_CHECK_ETH_LINK_STATE 500U

/* Timeout for DNS request */
#define TIMEOUT_DNS_REQUEST 10000U

/* Maximum number of retries for DHCP request */
#define MAX_DHCP_TRIES  4

/* Ethernet configuration: user parameters */
struct stm32_eth_config {
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;
};

/* Use to give user parameters to netif configuration */
static struct stm32_eth_config gconfig;

/* Netif global configuration structure */
struct netif gnetif;

/* DHCP periodic timer */
static uint32_t DHCPfineTimer = 0;

/* DHCP current state */
__IO uint8_t DHCP_state = DHCP_OFF;

/* Set to 1 if user use DHCP to obtain network addresses */
static uint8_t DHCP_Started_by_user = 0;

/* Ethernet link status periodic timer */
static uint32_t gEhtLinkTickStart = 0;

/* tcpip_thread set the value to 1 after started */
uint32_t tcpip_started = 0;

/*************************** Function prototype *******************************/
static void tcpip_init_done(void *arg);
static err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t tcp_poll_callback(void *arg, struct tcp_pcb *tpcb);
static void tcp_err_callback(void *arg, err_t err);

/**
* @brief  Configurates the network interface
* @param  None
* @retval None
*/
static void tcpip_init_done(void *arg)
{
  (void)arg;

  /* Add the network interface */
  netif_add(&gnetif, &(gconfig.ipaddr), &(gconfig.netmask), &(gconfig.gw), NULL, &ethernetif_init, &tcpip_input);

  /* Registers the default network interface */
  netif_set_default(&gnetif);

  if (netif_is_link_up(&gnetif)) {
    /* When the netif is fully configured this function must be called */
    netif_set_up(&gnetif);
  } else {
    /* When the netif link is down this function must be called */
    netif_set_down(&gnetif);
  }

#if LWIP_NETIF_LINK_CALLBACK
  /* Set the link callback function, this function is called on change of link status */
  netif_set_link_callback(&gnetif, ethernetif_update_config);
#endif /* LWIP_NETIF_LINK_CALLBACK */

#if LWIP_NETIF_STATUS_CALLBACK
  netif_set_status_callback(&gnetif, ethernetif_status_changed);
#endif /* LWIP_NETIF_STATUS_CALLBACK */
}

void ethernet_thread(void *arg) {
  extern ETH_HandleTypeDef EthHandle;
  (void)arg;

  while (1) {
    /* Check RX status */
    if ((EthHandle.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET) {
      LOG_D("Re-enable RX");
      ethernetif_input(&gnetif);
    }

    /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
    if ((EthHandle.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET) {
      LOG_D("Re-enable TX");
      ethernetif_output(&gnetif, NULL);
    }

    /* Check ethernet link status */
    if ((HAL_GetTick() - gEhtLinkTickStart) >= TIME_CHECK_ETH_LINK_STATE) {
      LOCK_TCPIP_CORE();
      ethernetif_set_link(&gnetif);
      UNLOCK_TCPIP_CORE();
      gEhtLinkTickStart = HAL_GetTick();
    }

    /* Handle LwIP timeouts */
    // sys_check_timeouts();

#if LWIP_DHCP
    stm32_DHCP_Periodic_Handle(&gnetif);
#endif /* LWIP_DHCP */

    sys_msleep(20);
  }
}

void stm32_eth_init(const uint8_t *mac, const uint8_t *ip, const uint8_t *gw, const uint8_t *netmask) {
  static uint8_t initDone = 0;

  if (!initDone) {
    if (mac != NULL) {
      ethernetif_set_mac_addr(mac);
    } // else default value is used: MAC_ADDR0 ... MAC_ADDR5

    if (ip != NULL) {
      IP_ADDR4(&(gconfig.ipaddr), ip[0], ip[1], ip[2], ip[3]);
    } else {
#if LWIP_DHCP
      ip_addr_set_zero_ip4(&(gconfig.ipaddr));
#else
      IP_ADDR4(&(gconfig.ipaddr), IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
#endif /* LWIP_DHCP */
    }

    if (gw != NULL) {
      IP_ADDR4(&(gconfig.gw), gw[0], gw[1], gw[2], gw[3]);
    } else {
#if LWIP_DHCP
      ip_addr_set_zero_ip4(&(gconfig.gw));
#else
      IP_ADDR4(&(gconfig.gw), GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
#endif /* LWIP_DHCP */
    }

    if (netmask != NULL) {
      IP_ADDR4(&(gconfig.netmask), netmask[0], netmask[1], netmask[2], netmask[3]);
    } else {
#if LWIP_DHCP
      ip_addr_set_zero_ip4(&(gconfig.netmask));
#else
      IP_ADDR4(&(gconfig.netmask), NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
#endif /* LWIP_DHCP */
    }

    /* Initialize the LwIP stack */
    tcpip_init(&tcpip_init_done, NULL);

    initDone = 1;
  }

  /* Reset DHCP if used */
  User_notification(&gnetif);

  /* Start Ethernet thread */
  (void)sys_thread_new("ethernet", ethernet_thread, NULL, ETHERNET_THREAD_STACKSIZE, ETHERNET_THREAD_PRIO);
}

/**
  * @brief Return Ethernet init status
  * @param  None
  * @retval 1 for initialized, 0 for not initialized
  */
uint8_t stm32_eth_is_init(void)
{
  return ethernetif_is_init();
}

/**
  * @brief Return Ethernet link status
  * @param  None
  * @retval 1 for link up, 0 for link down
  */
uint8_t stm32_eth_link_up(void)
{
  return netif_is_link_up(&gnetif);
}

#if LWIP_DHCP

/**
  * @brief  Returns DHCP activation state
  * @param  None
  * @retval true if DHCP enabled or false if not used
  */
uint8_t stm32_dhcp_started(void)
{
  return DHCP_Started_by_user;
}

/**
  * @brief  DHCP_Process_Handle
  * @param  netif pointer to generic data structure used for all lwIP network interfaces
  * @retval None
  */
void stm32_DHCP_process(struct netif *netif)
{
  struct dhcp *dhcp;

  if (netif_is_up(netif) && netif_is_link_up(netif)) {
    switch (DHCP_state) {
      case DHCP_START: {
          ip_addr_set_zero_ip4(&netif->ip_addr);
          ip_addr_set_zero_ip4(&netif->netmask);
          ip_addr_set_zero_ip4(&netif->gw);
          DHCP_state = DHCP_WAIT_ADDRESS;
          LOCK_TCPIP_CORE();
          dhcp_start(netif);
          UNLOCK_TCPIP_CORE();
        }
        break;

      case DHCP_WAIT_ADDRESS: {
          if (dhcp_supplied_address(netif)) {
            DHCP_state = DHCP_ADDRESS_ASSIGNED;
          } else {
            dhcp = (struct dhcp *)netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

            /* DHCP timeout */
            if (dhcp->tries > MAX_DHCP_TRIES) {
              DHCP_state = DHCP_TIMEOUT;
              // If DHCP address not bind, keep DHCP stopped
              DHCP_Started_by_user = 0;
              /* Stop DHCP */
              LOCK_TCPIP_CORE();
              dhcp_release_and_stop(netif);
              UNLOCK_TCPIP_CORE();
            }
          }
        }
        break;
      case DHCP_ASK_RELEASE:
      case DHCP_LINK_DOWN: {
          /* Force release or Stop DHCP */
          LOCK_TCPIP_CORE();
          dhcp_release_and_stop(netif);
          UNLOCK_TCPIP_CORE();
          DHCP_state = DHCP_OFF;
        }
        break;
      default: break;
    }
  } else {
    DHCP_state = DHCP_OFF;
  }
}

/**
  * @brief  DHCP periodic check
  * @param  netif pointer to generic data structure used for all lwIP network interfaces
  * @retval None
  */
void stm32_DHCP_Periodic_Handle(struct netif *netif)
{
  /* Fine DHCP periodic process every 500ms */
  if (HAL_GetTick() - DHCPfineTimer >= DHCP_FINE_TIMER_MSECS) {
    DHCPfineTimer =  HAL_GetTick();
    /* process DHCP state machine */
    stm32_DHCP_process(netif);
  }
}

/**
  * @brief  Inform the local DHCP of our manual IP configuration
  * @param  None
  * @retval None
  */
void stm32_DHCP_manual_config(void) {
  LOCK_TCPIP_CORE();
  dhcp_inform(&gnetif);
  UNLOCK_TCPIP_CORE();
}

/**
  * @brief  Return status of the DHCP when renew or rebind
  * @param  None
  * @retval Renew or rebind. Adapted from Arduino Ethernet library.
  */
uint8_t stm32_get_DHCP_lease_state(void)
{
  uint8_t res = 0;
  struct dhcp *dhcp = (struct dhcp *)netif_get_client_data(&gnetif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

  if (dhcp->state == DHCP_STATE_RENEWING) {
    res = 2;
  } else if (dhcp->state == DHCP_STATE_REBINDING) {
    res = 4;
  }

  return res;
}

/**
  * @brief  Set DHCP state
  * @param  state: DHCP_START, DHCP_ASK_RELEASE or DHCP_STOP. Others should not be used.
  * @retval None
  */
void stm32_set_DHCP_state(uint8_t state) {
  DHCP_state = state;
  if (DHCP_START == state) {
    DHCP_Started_by_user = 1;
  }
}

/**
  * @brief  Return DHCP state
  * @param  None
  * @retval One of the following state:
              DHCP_OFF
              DHCP_START
              DHCP_WAIT_ADDRESS
              DHCP_ADDRESS_ASSIGNED
              DHCP_TIMEOUT
              DHCP_LINK_DOWN
              DHCP_ASK_RELEASE
  */
uint8_t stm32_get_DHCP_state(void)
{
  return DHCP_state;
}

#endif /* LWIP_DHCP */

/**
  * @brief  Converts IP address in readable format for user.
  * @param  None
  * @retval address in uint32_t format
  */
uint32_t stm32_eth_get_ipaddr(void)
{
  return ip4_addr_get_u32(&(gnetif.ip_addr));
}

/**
  * @brief  Converts gateway address in readable format for user.
  * @param  None
  * @retval address in uint32_t format
  */
uint32_t stm32_eth_get_gwaddr(void)
{
  return ip4_addr_get_u32(&(gnetif.gw));
}

/**
  * @brief  Converts network mask address in readable format for user.
  * @param  None
  * @retval address in uint32_t format
  */
uint32_t stm32_eth_get_netmaskaddr(void)
{
  return ip4_addr_get_u32(&(gnetif.netmask));
}

/**
  * @brief  Converts DNS address in readable format for user.
  * @param  None
  * @retval address in uint32_t format
  */
uint32_t stm32_eth_get_dnsaddr(void)
{
  const ip_addr_t *tmp = dns_getserver(0);
  return ip4_addr_get_u32(tmp);
}

/**
  * @brief  Converts DHCP address in readable format for user.
  * @param  None
  * @retval address in uint32_t format
  */
uint32_t stm32_eth_get_dhcpaddr(void)
{
  struct dhcp *dhcp = (struct dhcp *)netif_get_client_data(&gnetif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);
  return ip4_addr_get_u32(&(dhcp->server_ip_addr));
}

#if LWIP_NETIF_LINK_CALLBACK

/**
  * @brief  This function notify user about link status changement.
  * @param  netif: the network interface
  * @retval None
  */
void ethernetif_notify_conn_changed(struct netif *netif)
{
  if (netif_is_link_up(netif)) {
    /* Update DHCP state machine if DHCP used */
    if (DHCP_Started_by_user == 1) {
      DHCP_state = DHCP_START;
    }

    /* When the netif is fully configured this function must be called.*/
    netif_set_up(netif);
    LOG_I("Link UP\n");
  } else {
    /* Update DHCP state machine if DHCP used */
    if (DHCP_Started_by_user == 1) {
      DHCP_state = DHCP_LINK_DOWN;
    }

    /*  When the netif link is down this function must be called.*/
    netif_set_down(netif);
    LOG_I("Link DOWN\n");
  }
}

#endif /* LWIP_NETIF_LINK_CALLBACK */

/**
  * @brief  Notify the User about the network interface config status
  * @param  netif: the network interface
  * @retval None
  */
void User_notification(struct netif *netif)
{
  if (netif_is_up(netif)) {
    //Link up
  } else {
    /* Update DHCP state machine */
    if (DHCP_Started_by_user == 1) {
      DHCP_state = DHCP_LINK_DOWN;
    }
  }
}

#if LWIP_DNS

/**
  * @brief  Initializes DNS
  * @param  dnsaddr: DNS address
  * @retval None
  */
void stm32_dns_init(const uint8_t *dnsaddr)
{
  ip_addr_t ip;

  /* DNS initialized by DHCP when call dhcp_start() */
  if (!stm32_dhcp_started()) {
    dns_init();
    IP_ADDR4(&ip, dnsaddr[0], dnsaddr[1], dnsaddr[2], dnsaddr[3]);
    dns_setserver(0, &ip);
  }
}

/** Callback which is invoked when a hostname is found.
 * A function of this type must be implemented by the application using the DNS resolver.
 * @param name pointer to the name that was looked up.
 * @param ipaddr pointer to an ip_addr_t containing the IP address of the hostname,
 *        or NULL if the name could not be found (or on any other error).
 * @param callback_arg a user-specified callback argument passed to dns_gethostbyname
*/
void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
  UNUSED(name);

  if (ipaddr != NULL) {
    *((uint32_t *)callback_arg) = ip4_addr_get_u32(ipaddr);
  } else {
    *((uint32_t *)callback_arg) = 0;
  }
}

/**
 * Resolve a hostname (string) into an IP address.
 *
 * @param hostname the hostname that is to be queried
 * @param addr pointer to a uint8_t where to store the address
 * @return an error code compatible with Arduino Ethernet library
 */
int8_t stm32_dns_gethostbyname(const char *hostname, uint32_t *ipaddr)
{
  ip_addr_t iphost;
  err_t err;
  uint32_t tickstart = 0;
  int8_t ret = 0;

  *ipaddr = 0;
  LOCK_TCPIP_CORE();
  err = dns_gethostbyname(hostname, &iphost, &dns_callback, ipaddr);
  UNLOCK_TCPIP_CORE();

  switch (err) {
    case ERR_OK:
      *ipaddr = ip4_addr_get_u32(&iphost);
      ret = 1;
      break;

    case ERR_INPROGRESS:
      tickstart = HAL_GetTick();
      while (*ipaddr == 0) {
        if ((HAL_GetTick() - tickstart) >= TIMEOUT_DNS_REQUEST) {
          ret = -1;
          break;
        }
        sys_msleep(20);
      }

      if (ret == 0) {
        if (*ipaddr == 0) {
          ret = -2;
        } else {
          ret = 1;
        }
      }
      break;

    case ERR_ARG:
      ret = -4;
      break;

    default:
      ret = -4;
      break;
  }

  return ret;
}

#endif /* LWIP_DNS */

/**
  * @brief  Converts a uint8_t IP address to a ip_addr_t address
  * @param  ipu8: pointer to an address to convert
  * @param  ipaddr: pointer where store the address converted
  * @retval pointer to an address in ip_addr_t format
  */
ip_addr_t *u8_to_ip_addr(uint8_t *ipu8, ip_addr_t *ipaddr)
{
  IP_ADDR4(ipaddr, ipu8[0], ipu8[1], ipu8[2], ipu8[3]);
  return ipaddr;
}

/**
  * @brief  Converts a ip_addr_t IP address to a uint32_t address
  * @param  ipaddr: pointer to an address to convert
  * @retval pointer to the address converted
  */
uint32_t ip_addr_to_u32(ip_addr_t *ipaddr)
{
  return ip4_addr_get_u32(ipaddr);
}

/**
  * @brief  Allocate a pbuf with data pass in parameter
  * @param  p: pointer to pbuf
  * @param  buffer: pointer to data to store
  * @param  size: number of data to store
  * @retval pointer to the pbuf allocated
  */
struct pbuf *stm32_new_data(struct pbuf *p, const uint8_t *buffer, size_t size)
{
  // Allocate memory if pbuf doesn't exit yet.
  if (p == NULL) {
    p = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);

    if (p != NULL) {
      // Copy data inside pbuf
      if (ERR_OK == pbuf_take(p, (uint8_t *)buffer, size)) {
        return p;
      } else {
        pbuf_free(p);
      }
    }
  }
  // If pbuf allocated, grow the size of pbuf and add new data
  // NOTE: pbuf_realloc can't be used to grow the size of pbuf
  else {
    struct pbuf *q = pbuf_alloc(PBUF_TRANSPORT, size + p->tot_len, PBUF_RAM);

    if (q != NULL) {
      if (ERR_OK == pbuf_copy(q, p)) {
        if (ERR_OK == pbuf_take_at(q, (uint8_t *)buffer, size, p->tot_len)) {
          pbuf_free(p);
          p = q;
          return p;
        }
      }

      pbuf_free(q);
    }
  }

  return 0;
}

void stm32_free_data(struct pbuf_data *data) {
  if (data->p == NULL) {
    return;
  }
  while (!is_empty(&data->pbuf_queue)) {
    pbuf_free((struct pbuf *)queue_get(&data->pbuf_queue));
  }
  if (data->p) {
    pbuf_free(data->p);
  }
  data->available = 0;
  data->offset = 0;
}

/**
  * @brief This function passes pbuf data to uin8_t buffer. It takes account if
  * pbuf is chained.
  * @param data pointer to data structure
  * @param buffer the buffer where write the data read
  * @param size the number of data to read
  * @retval number of data read
  */
uint16_t stm32_get_data(struct pbuf_data *data, uint8_t *buffer, size_t size)
{
  uint16_t nb;
  uint16_t to_copy;
  uint16_t copied;

  if ((data->p == NULL) || (buffer == NULL) || (size == 0) || (data->available == 0)) {
    return 0;
  }

  if (size == 1) {
    buffer[0] = pbuf_get_at(data->p, data->offset);
    data->available--;

    if ((data->offset + 1) == data->p->tot_len) {
      /* Current "pbuf" done */
      pbuf_free(data->p);
      data->offset = 0;
      data->p = (struct pbuf *)queue_get(&data->pbuf_queue);
    } else {
      data->offset++;
    }

    return 1;
  }

  nb = 0;
  to_copy = size;
  while (nb < size) {
    copied = pbuf_copy_partial(data->p, &buffer[nb], to_copy, data->offset);
    data->available -= copied;
    nb += copied;

    if ((copied < to_copy) || ((data->offset + copied) == data->p->tot_len)) {
      /* Current "pbuf" done */
      pbuf_free(data->p);
      data->offset = 0;
      data->p = (struct pbuf *)queue_get(&data->pbuf_queue);
      if (data->p == NULL)
        break;
    }
    data->offset += copied;
    to_copy -= copied;
  }

  return nb;
}

#if LWIP_UDP

/**
  * @brief This function is called when an UDP datagram has been received on
  * the port UDP_PORT.
  * @param arg user supplied argument
  * @param pcb the udp_pcb which received data
  * @param p the packet buffer that was received
  * @param addr the remote IP address from which the packet was received
  * @param port the remote port from which the packet was received
  * @retval None
  */
void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                          const ip_addr_t *addr, u16_t port)
{
  struct udp_struct *udp_arg = (struct udp_struct *)arg;

  /* Send data to the application layer */
  if ((udp_arg != NULL) && (udp_arg->pcb == pcb)) {
    // Free the old p buffer if not read
    if (udp_arg->data.p != NULL) {
      pbuf_free(udp_arg->data.p);
    }

    udp_arg->data.p = p;
    udp_arg->data.available = p->len;

    ip_addr_copy(udp_arg->ip, *addr);
    udp_arg->port = port;

    if (udp_arg->onDataArrival != NULL) {
      udp_arg->onDataArrival();
    }
  } else {
    pbuf_free(p);
  }
}

#endif /* LWIP_UDP */

#if LWIP_TCP

/**
  * @brief Function called when TCP connection established
  * @param arg: user supplied argument
  * @param tpcb: pointer on the connection control block
  * @param err: when connection correctly established err should be ERR_OK
  * @retval err_t: returned error
  */
err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
  struct tcp_struct *tcp_arg = (struct tcp_struct *)arg;

  if (err == ERR_OK) {
    if ((tcp_arg != NULL) && (tcp_arg->pcb == tpcb)) {
      tcp_arg->state = TCP_CONNECTED;

      /* initialize LwIP tcp_recv callback function */
      tcp_recv(tpcb, tcp_recv_callback);

      /* initialize LwIP tcp_sent callback function */
      tcp_sent(tpcb, tcp_sent_callback);

      /* initialize LwIP tcp_poll callback function */
      tcp_poll(tpcb, tcp_poll_callback, 2);

      /* initialize LwIP tcp_err callback function */
      tcp_err(tpcb, tcp_err_callback);

      return ERR_OK;
    } else {
      /* close connection */
      tcp_connection_close(tpcb, tcp_arg);

      return ERR_ARG;
    }
  } else {
    /* close connection */
    tcp_connection_close(tpcb, tcp_arg);
  }
  return err;
}

/**
  * @brief  This function is the implementation of tcp_accept LwIP callback
  * @param arg user supplied argument
  * @param  newpcb: pointer on tcp_pcb struct for the newly created tcp connection
  * @param err: when connection correctly established err should be ERR_OK
  * @retval err_t: error status
  */
err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  err_t ret_err;
  uint8_t accepted;
  struct tcp_struct **tcpClient = (struct tcp_struct **)arg;

  /* set priority for the newly accepted tcp connection newpcb */
  tcp_setprio(newpcb, TCP_PRIO_MIN);

  if ((tcpClient != NULL) && (ERR_OK == err)) {
    struct tcp_struct *client = (struct tcp_struct *)mem_malloc(sizeof(struct tcp_struct));

    if (client != NULL) {
      client->state = TCP_ACCEPTED;
      client->pcb = newpcb;
      client->data.p = NULL;
      client->data.available = 0;
      client->data.offset = 0;
      queue_init(client->data.pbuf_queue, client->data._pbuf_queue);
      client->is_accept = 0;

      /* Looking for an empty socket */
      for (uint16_t i = 0; i < MAX_CLIENT; i++) {
        if (tcpClient[i] == NULL) {
          tcpClient[i] = client;
          accepted = 1;
          break;
        }
      }

      if (accepted) {
        /* pass newly allocated client structure as argument to newpcb */
        tcp_arg(newpcb, client);

        /* initialize lwip tcp_recv callback function for newpcb  */
        tcp_recv(newpcb, tcp_recv_callback);

        /* initialize lwip tcp_err callback function for newpcb  */
        tcp_err(newpcb, tcp_err_callback);

        /* initialize LwIP tcp_sent callback function */
        tcp_sent(newpcb, tcp_sent_callback);

        /* initialize LwIP tcp_poll callback function */
        tcp_poll(newpcb, tcp_poll_callback, 2);

        ret_err = ERR_OK;
      } else {
        /*  close tcp connection */
        tcp_connection_close(newpcb, client);
        mem_free(client);

        /* return memory error */
        ret_err = ERR_MEM;
      }
    } else {
      /*  close tcp connection */
      tcp_connection_close(newpcb, client);
      mem_free(client);

      /* return memory error */
      ret_err = ERR_MEM;
    }
  } else {
    if (ERR_OK != tcp_close(newpcb)) {
      tcp_abort(newpcb);
    }
    ret_err = ERR_ARG;
  }
  return ret_err;
}

/**
  * @brief tcp_receiv callback
  * @param arg: argument to be passed to receive callback
  * @param tpcb: tcp connection control block
  * @param err: receive error code
  * @retval err_t: returned error
  */
static err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  struct tcp_struct *tcp_arg = (struct tcp_struct *)arg;
  err_t ret_err;

  /* if we receive an empty tcp frame from server => close connection */
  if (p == NULL) {
    /* we're done sending, close connection */
    tcp_connection_close(tpcb, tcp_arg);
    ret_err = ERR_OK;
  }
  /* else : a non empty frame was received from echo server but for some reason err != ERR_OK */
  else if (err != ERR_OK) {
    /* free received pbuf*/
    if (p != NULL) {
      LOG_E("TCP recv err %d", err);
      pbuf_free(p);
    }
    ret_err = err;
  } else if ((tcp_arg->state == TCP_CONNECTED) || (tcp_arg->state == TCP_ACCEPTED)) {
    /* Acknowledge data reception */
    tcp_recved(tpcb, p->tot_len);

    if (tcp_arg->data.p == NULL) {
      tcp_arg->data.p = p;
      tcp_arg->data.available += p->tot_len;
      tcp_arg->data.offset = 0;
    } else {
      if (!queue_put(&tcp_arg->data.pbuf_queue, (void *)p)) {
        LOG_E("Client Q full");
        pbuf_free(p);
      } else {
        tcp_arg->data.available += p->tot_len;
      }
    }

    ret_err = ERR_OK;
  }
  /* data received when connection already closed */
  else {
    /* Acknowledge data reception */
    tcp_recved(tpcb, p->tot_len);

    /* free pbuf and do nothing */
    pbuf_free(p);
    ret_err = ERR_OK;
  }
  return ret_err;
}

/**
  * @brief  This function implements the tcp_sent LwIP callback (called when ACK
  *         is received from remote host for sent data)
  * @param  arg: pointer on argument passed to callback
  * @param  tcp_pcb: tcp connection control block
  * @param  len: length of data sent
  * @retval err_t: returned error code
  */
static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  struct tcp_struct *tcp_arg = (struct tcp_struct *)arg;

  LWIP_UNUSED_ARG(len);

  if ((tcp_arg != NULL) && (tcp_arg->pcb == tpcb)) {
    return ERR_OK;
  }

  return ERR_ARG;
}

static err_t tcp_poll_callback(void *arg, struct tcp_pcb *tpcb) {
  // LOG_D("POLL %p %p", tpcb, arg);
  (void)arg;
  (void)tcp_output(tpcb);

  return ERR_OK;
}

/** Function prototype for tcp error callback functions. Called when the pcb
 * receives a RST or is unexpectedly closed for any other reason.
 *
 * @note The corresponding pcb is already freed when this callback is called!
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param err Error code to indicate why the pcb has been closed
 *            ERR_ABRT: aborted through tcp_abort or by a TCP timer
 *            ERR_RST: the connection was reset by the remote host
 */
static void tcp_err_callback(void *arg, err_t err) {
  struct tcp_struct *tcp_arg = (struct tcp_struct *)arg;

  LOG_E("TCP err %d", err);
  if (tcp_arg != NULL) {
    if (ERR_OK != err) {
      tcp_arg->pcb = NULL;
      tcp_arg->state = TCP_CLOSING;
      LOG_D("TCP_CLOSING");
    }
  }
}

/**
  * @brief This function is used to close the tcp connection with server
  * @param tpcb: tcp connection control block
  * @param es: pointer on echoclient structure
  * @retval None
  */
void tcp_connection_close(struct tcp_pcb *tpcb, struct tcp_struct *tcp) {
  if (tcp->state == TCP_CLOSING) {
    return;
  }

  LOCK_TCPIP_CORE();
  /* remove callbacks */
  tcp_accept(tpcb, NULL);
  tcp_recv(tpcb, NULL);
  tcp_sent(tpcb, NULL);
  tcp_poll(tpcb, NULL, 0);
  tcp_err(tpcb, NULL);
  /* close tcp connection */
  if (ERR_OK != tcp_close(tpcb)) {
    tcp_abort(tpcb);
  }
  UNLOCK_TCPIP_CORE();

  tcp->pcb = NULL;
  tcp->state = TCP_CLOSING;
}

#endif /* LWIP_TCP */
