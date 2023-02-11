/**
  ******************************************************************************
  * @file    ethernetif.c
  * @author  MCD Application Team & Wi6Labs
  * @version V1.5.0
  * @date    20-june-2017
  * @brief   This file implements Ethernet network interface drivers for lwIP
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32_def.h"
#include "PeripheralPins.h"
#include "stm32_eth.h"
#if !defined(STM32_CORE_VERSION) || (STM32_CORE_VERSION  <= 0x01050000)
  #include "variant.h"
#endif

#include "lwip/tcpip.h"
#include "lwip/igmp.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"

#include "queue.h"
#include "ethernetif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_TAG "ETH_IF"
#include <log.h>

/* Private typedef -----------------------------------------------------------*/
typedef struct lwip_custom_pbuf {
    struct pbuf_custom custom_p;
    uint8_t *buf;
} lwip_custom_pbuf_t;

/* Private define ------------------------------------------------------------*/
/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

/* Definition of PHY SPECIAL CONTROL/STATUS REGISTER bitfield Auto-negotiation done indication */
/* Placed in STM32Ethernet library instead of HAL conf to avoid compatibility dependence with Arduino_Core_STM32 */
/* Could be moved from this file once Generic PHY is implemented */
#define PHY_SR_AUTODONE ((uint16_t)0x1000)

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma data_alignment=4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef  DMARxDscrTab[ETH_RXBUFNB] __ALIGN_END;/* Ethernet Rx MA Descriptor */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma data_alignment=4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef  DMATxDscrTab[ETH_TXBUFNB] __ALIGN_END;/* Ethernet Tx DMA Descriptor */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Rx_Buff[ETH_RXBUFNB * 2][ETH_RX_BUF_SIZE] __ALIGN_END; /* Ethernet Receive Buffer */

ETH_HandleTypeDef EthHandle;

static uint8_t macaddress[6] = { MAC_ADDR0, MAC_ADDR1, MAC_ADDR2, MAC_ADDR3, MAC_ADDR4, MAC_ADDR5 };

#if LWIP_IGMP
uint32_t ETH_HashTableHigh = 0x0;
uint32_t ETH_HashTableLow = 0x0;
#endif

/* Custom pbuf pool */
LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RXBUFNB * 2, sizeof(lwip_custom_pbuf_t), "ETH_RX_PBUF");

static void *_tx_free_queue[ETH_TXBUFNB];
static void *_rx_buf_queue[ETH_RXBUFNB];
static queue_t tx_free_queue;
static queue_t rx_buf_queue;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
static void lwip_custom_pbuf_free(struct pbuf *p) {
  lwip_custom_pbuf_t *custom_p = (lwip_custom_pbuf_t *)p;
  bool ret;

  ret = queue_put(&rx_buf_queue, (void *)custom_p->buf);
  if (!ret) {
    LOG_E("RX Q full");
  } else {
    LOG_D("RX Q+ %p %d", custom_p->buf, rx_buf_queue.put_cnt - rx_buf_queue.get_cnt);
  }
  LWIP_MEMPOOL_FREE(RX_POOL, custom_p);
}

/*******************************************************************************
                       Ethernet MSP Routines
*******************************************************************************/
/**
  * @brief  Initializes the ETH MSP.
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  const PinMap *map = PinMap_Ethernet;
  PinName pin = pin_pinName(map);
  GPIO_TypeDef *port;

  UNUSED(heth);

  /* Ethernet pins configuration ************************************************/

  if (map != NULL) {
    while (pin != NC) {
      /* Set port clock */
      port = set_GPIO_Port_Clock(STM_PORT(pin));

      /* pin configuration */
      GPIO_InitStructure.Pin       = STM_GPIO_PIN(pin);
      GPIO_InitStructure.Mode      = STM_PIN_MODE(pinmap_function(pin, PinMap_Ethernet));
      GPIO_InitStructure.Pull      = STM_PIN_PUPD(pinmap_function(pin, PinMap_Ethernet));
      GPIO_InitStructure.Speed     = GPIO_SPEED_FREQ_HIGH;
      GPIO_InitStructure.Alternate = STM_PIN_AFNUM(pinmap_function(pin, PinMap_Ethernet));
      HAL_GPIO_Init(port, &GPIO_InitStructure);

      pin = pin_pinName(++map);
    }
  }

#ifdef ETH_INPUT_USE_IT
  /* Enable the Ethernet global Interrupt */
  HAL_NVIC_SetPriority(ETH_IRQn, 0x7, 0);
  HAL_NVIC_EnableIRQ(ETH_IRQn);
#endif /* ETH_INPUT_USE_IT */

  /* Enable ETHERNET clock  */
  __HAL_RCC_ETH_CLK_ENABLE();
}

/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH)
*******************************************************************************/
/**
  * @brief In this function, the hardware should be initialized.
  * Called from ethernetif_init().
  *
  * @param netif the already initialized lwip network interface structure
  *        for this ethernetif
  */
static void low_level_init(struct netif *netif) {
  uint32_t regvalue;
  uint32_t i;

  /* Initialize memory pool */
  LWIP_MEMPOOL_INIT(RX_POOL);

  /* Initialize queues */
  queue_init(tx_free_queue, _tx_free_queue);
  queue_init(rx_buf_queue, _rx_buf_queue);
  for (i = 0; i < ETH_RXBUFNB; i++) {
    queue_put(&rx_buf_queue, (void *)&Rx_Buff[ETH_RXBUFNB + i][0]);
    LOG_D("RX Q+ %p", Rx_Buff[ETH_RXBUFNB + i]);
  }

  EthHandle.Instance = ETH;
  EthHandle.Init.MACAddr = macaddress;
  EthHandle.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
  EthHandle.Init.Speed = ETH_SPEED_100M;
  EthHandle.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
#ifdef ETHERNET_RMII_MODE_CONFIGURATION
  EthHandle.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
#else
  EthHandle.Init.MediaInterface = ETH_MEDIA_INTERFACE_MII;
#endif /* ETHERNET_RMII_MODE_CONFIGURATION */
#ifdef ETH_INPUT_USE_IT
  EthHandle.Init.RxMode = ETH_RXINTERRUPT_MODE;
#else
  EthHandle.Init.RxMode = ETH_RXPOLLING_MODE;
#endif /* ETH_INPUT_USE_IT */
  EthHandle.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  EthHandle.Init.PhyAddress = LAN8742A_PHY_ADDRESS;

  /* configure ethernet peripheral (GPIOs, clocks, MAC, DMA) */
  if (HAL_ETH_Init(&EthHandle) == HAL_OK) {
    /* Set netif link flag */
    netif->flags |= NETIF_FLAG_LINK_UP;
  }

  /* Initialize Tx Descriptors list: Chain Mode */
  HAL_ETH_DMATxDescListInit(&EthHandle, DMATxDscrTab, NULL, ETH_TXBUFNB);

  /* Initialize Rx Descriptors list: Chain Mode  */
  HAL_ETH_DMARxDescListInit(&EthHandle, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

  /* set MAC hardware address length */
  netif->hwaddr_len = ETH_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] =  macaddress[0];
  netif->hwaddr[1] =  macaddress[1];
  netif->hwaddr[2] =  macaddress[2];
  netif->hwaddr[3] =  macaddress[3];
  netif->hwaddr[4] =  macaddress[4];
  netif->hwaddr[5] =  macaddress[5];

  /* maximum transfer unit */
  netif->mtu = 1500;

  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  /* Enable MAC and DMA transmission and reception */
  HAL_ETH_Start(&EthHandle);
#if LWIP_IGMP
  netif_set_igmp_mac_filter(netif, igmp_mac_filter);
#endif
  /**** Configure PHY to generate an interrupt when Eth Link state changes ****/
  /* Read Register Configuration */
  HAL_ETH_ReadPHYRegister(&EthHandle, PHY_IMR, &regvalue);

  regvalue |= PHY_ISFR_INT4;

  /* Enable Interrupt on change of link status */
  HAL_ETH_WritePHYRegister(&EthHandle, PHY_IMR, regvalue);
#if LWIP_IGMP
  ETH_HashTableHigh = EthHandle.Instance->MACHTHR;
  ETH_HashTableLow = EthHandle.Instance->MACHTLR;
#endif
}

/**
  * @brief  Sends an Ethernet frame.
  * @param  heth pointer to a ETH_HandleTypeDef structure that contains
  *         the configuration information for ETHERNET module
  * @param  bufcount Number of buffer to be sent
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_ETH_TransmitFrame2(ETH_HandleTypeDef *heth, uint32_t bufcount)
{
  uint32_t i = 0;

  /* Process Locked */
  __HAL_LOCK(heth);

  /* Set the ETH peripheral state to BUSY */
  heth->State = HAL_ETH_STATE_BUSY;

  if (bufcount == 0) {
    /* Set ETH HAL state to READY */
    heth->State = HAL_ETH_STATE_READY;

    /* Process Unlocked */
    __HAL_UNLOCK(heth);

    return  HAL_ERROR;
  }

  /* Check if the descriptor is owned by the ETHERNET DMA (when set) or CPU (when reset) */
  if (((heth->TxDesc)->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
    /* OWN bit set */
    heth->State = HAL_ETH_STATE_BUSY_TX;

    /* Process Unlocked */
    __HAL_UNLOCK(heth);

    return HAL_ERROR;
  }

  if (bufcount == 1) {
    /* Set LAST and FIRST segment */
    heth->TxDesc->Status |=ETH_DMATXDESC_FS|ETH_DMATXDESC_LS;
    /* Set Own bit of the Tx descriptor Status: gives the buffer back to ETHERNET DMA */
    heth->TxDesc->Status |= ETH_DMATXDESC_OWN;
    /* Point to next descriptor */
    heth->TxDesc = (ETH_DMADescTypeDef *)(heth->TxDesc->Buffer2NextDescAddr);
  } else {
    for (i = 0; i < bufcount; i++) {
      /* Clear FIRST and LAST segment bits */
      heth->TxDesc->Status &= ~(ETH_DMATXDESC_FS | ETH_DMATXDESC_LS);

      if (i == 0) {
        /* Setting the first segment bit */
        heth->TxDesc->Status |= ETH_DMATXDESC_FS;
      } else if (i == (bufcount-1)) {
        /* Setting the last segment bit */
        heth->TxDesc->Status |= ETH_DMATXDESC_LS;
      }

      /* Set Own bit of the Tx descriptor Status: gives the buffer back to ETHERNET DMA */
      heth->TxDesc->Status |= ETH_DMATXDESC_OWN;
      /* point to next descriptor */
      heth->TxDesc = (ETH_DMADescTypeDef *)(heth->TxDesc->Buffer2NextDescAddr);
    }
  }

  /* When Tx Buffer unavailable flag is set: clear it and resume transmission */
  if (((heth->Instance)->DMASR & ETH_DMASR_TBUS) != (uint32_t)RESET) {
    /* Clear TBUS ETHERNET DMA flag */
    (heth->Instance)->DMASR = ETH_DMASR_TBUS;
    /* Resume DMA transmission*/
    (heth->Instance)->DMATPDR = 0;
  }

  /* Set ETH HAL State to Ready */
  heth->State = HAL_ETH_STATE_READY;

  /* Process Unlocked */
  __HAL_UNLOCK(heth);

  /* Return function status */
  return HAL_OK;
}

/**
  * @brief This function should do the actual transmission of the packet. The packet is
  * contained in the pbuf that is passed to the function. This pbuf
  * might be chained.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
  * @return ERR_OK if the packet could be sent
  *         an err_t value if the packet couldn't be sent
  *
  * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
  *       strange results. You might consider waiting for space in the DMA queue
  *       to become available since the stack doesn't retry to send a packet
  *       dropped because of memory failure (except for the TCP timers).
  */
static err_t low_level_output(struct netif *netif, struct pbuf *p) {
  err_t errval;
  struct pbuf *q;
  __IO ETH_DMADescTypeDef *DmaTxDesc;
  uint32_t bufcount = 0;
  uint32_t i = 0;

  UNUSED(netif);

  /* Check if all done */
  DmaTxDesc = EthHandle.TxDesc;
  for (i = 0; i < ETH_TXBUFNB; i++) {
    // LOG_D("output chk %d %08x %08x", i, DmaTxDesc, DmaTxDesc->Status);
    if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
      break;
    }

    /* Point to next descriptor */
    DmaTxDesc = (ETH_DMADescTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);
  }
  if (i != ETH_TXBUFNB) {
    LOG_D("output chk %d/%d", i, ETH_TXBUFNB);
  }

  /* Free "pbuf" */
  while ((i == ETH_TXBUFNB) && (!is_empty(&tx_free_queue))) {
    q = (struct pbuf *)queue_get(&tx_free_queue);
    pbuf_free(q);
  }

  if (pbuf_clen(p) > ETH_TXBUFNB) {
      errval = ENOBUFS;
      LOG_E("output ENOBUFS");
      goto error;
  }

  /* Prepare DMA buffers */
  DmaTxDesc = EthHandle.TxDesc;
  for (q = p; q != NULL; q = q->next) {
    /* Is this buffer available? If not, goto error */
    if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
      errval = ERR_USE;
      LOG_E("output ERR_USE");
      goto error;
    }

    /* Check if the length of data is bigger than Tx buffer size*/
    if (q->len > ETH_TX_BUF_SIZE) {
      errval = ENOBUFS;
      LOG_E("output ENOBUFS2");
      goto error;
    }

    /* Put "pbuf" to free queue */
    if (!bufcount) {
      if (!queue_put(&tx_free_queue, (void *)p)) {
        LOG_E("TX Q full");
        errval = ENOBUFS;
        goto error;
      }
      pbuf_ref(p);
    }

    DmaTxDesc->Buffer1Addr = (uint32_t)q->payload;
    DmaTxDesc->ControlBufferSize = (q->len & ETH_DMATXDESC_TBS1);
    bufcount++;

    /* Point to next descriptor */
    DmaTxDesc = (ETH_DMADescTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);
  }

  /* Prepare transmit descriptors to give to DMA */
  (void)HAL_ETH_TransmitFrame2(&EthHandle, bufcount);

  errval = ERR_OK;

error:

  /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
  if ((EthHandle.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET) {
    /* Clear TUS ETHERNET DMA flag */
    EthHandle.Instance->DMASR = ETH_DMASR_TUS;

    /* Resume DMA transmission*/
    EthHandle.Instance->DMATPDR = 0;
  }
  return errval;
}

/**
  * @brief Should allocate a pbuf and transfer the bytes of the incoming
  * packet from the interface into the pbuf.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return a pbuf filled with the received packet (including MAC header)
  *         NULL on memory error
  */
static struct pbuf *low_level_input(struct netif *netif)
{
  void *new_buf = NULL;
  lwip_custom_pbuf_t *custom_p = NULL;
  struct pbuf *p = NULL;
  uint16_t len;
  uint8_t *buffer;
  __IO ETH_DMADescTypeDef *dmarxdesc;
  uint32_t i = 0;

  UNUSED(netif);

  EthHandle.RxFrameInfos.SegCount = 0;
  if (HAL_ETH_GetReceivedFrame_IT(&EthHandle) != HAL_OK) {
    return NULL;
  }

  /* Obtain the size of the packet and put it into the "len" variable. */
  len = EthHandle.RxFrameInfos.length;
  buffer = (uint8_t *)EthHandle.RxFrameInfos.buffer;

  do {
    /* When (ETH_RX_BUF_SIZE >= ETH_MAX_PACKET_SIZE), the following statments should be true:
       - EthHandle.RxFrameInfos.length <= ETH_RX_BUF_SIZE
       - EthHandle.RxFrameInfos.SegCount == 1
     */
    if ((len == 0) || (len > ETH_RX_BUF_SIZE) || (EthHandle.RxFrameInfos.SegCount != 1)) {
      LOG_E("Unexpected SegCount %d", EthHandle.RxFrameInfos.SegCount);
      break;
    }

    /* Allocate "pbuf" */
    custom_p = (lwip_custom_pbuf_t *)LWIP_MEMPOOL_ALLOC(RX_POOL);
    if (custom_p == NULL) {
      LOG_E("RX_POOL empty");
      break;
    }
    custom_p->custom_p.custom_free_function = lwip_custom_pbuf_free;
    custom_p->buf = buffer;
    p = pbuf_alloced_custom(PBUF_RAW, len, PBUF_REF, &custom_p->custom_p, buffer, ETH_RX_BUF_SIZE);
    if (p == NULL) {
      LOG_E("PBUF_REF empty");
      LWIP_MEMPOOL_FREE(RX_POOL, custom_p);
      break;
    }

    /* Allocate new buf */
    if (is_empty(&rx_buf_queue)) {
      LOG_E("RX Q empty, %d %d", rx_buf_queue.put_cnt, rx_buf_queue.get_cnt);
      LWIP_MEMPOOL_FREE(RX_POOL, custom_p);
      p = NULL;
      break;
    }
    new_buf = queue_get(&rx_buf_queue);
    // LOG_D("RX %p (%d), %d", EthHandle.RxFrameInfos.FSRxDesc->Buffer1Addr, len, (rx_buf_queue.put_cnt - rx_buf_queue.get_cnt));
    LOG_D("RX Q- %p %d", new_buf, rx_buf_queue.put_cnt - rx_buf_queue.get_cnt);
    EthHandle.RxFrameInfos.FSRxDesc->Buffer1Addr = (uint32_t)new_buf;
  } while (0);

  /* Release descriptors to DMA */
  /* Point to first descriptor */
  dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
  /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
  for (i = 0; i < EthHandle.RxFrameInfos.SegCount; i++) {
    dmarxdesc->Status |= ETH_DMARXDESC_OWN;
    dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
  }

  /* Clear Segment_Count */
  EthHandle.RxFrameInfos.SegCount = 0;

  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((EthHandle.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET) {
    /* Clear RBUS ETHERNET DMA flag */
    EthHandle.Instance->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    EthHandle.Instance->DMARPDR = 0;
  }
  return p;
}

/**
  * @brief This function should be called when a packet is ready to be read
  * from the interface. It uses the function low_level_input() that
  * should handle the actual reception of bytes from the network
  * interface. Then the type of the received packet is determined and
  * the appropriate input function is called.
  *
  * @param netif the lwip network interface structure for this ethernetif
  */
void ethernetif_input(struct netif *netif) {
  err_t err;
  struct pbuf *p = NULL;

  do {
    /* move received packet into a new pbuf */
    p = low_level_input(netif);
    if (p == NULL) {
      /* no packet could be read, silently ignore this */
      break;
    }

    /* pass Ethernet package to LwIP stack */
    err = netif->input(p, netif);
    if (err != ERR_OK) {
      // LOG_E("netif->input err: %d", err);
      pbuf_free(p);
      continue;
    }
  } while (p != NULL);
}

err_t ethernetif_output(struct netif *netif, struct pbuf *p) {
  return low_level_output(netif, p);
}


/**
  * @brief Returns the current state
  *
  * @param None
  * @return 0 not initialized else 1
  */
uint8_t ethernetif_is_init(void)
{
  return (EthHandle.State != HAL_ETH_STATE_RESET);
}

/**
  * @brief Should be called at the beginning of the program to set up the
  * network interface. It calls the function low_level_init() to do the
  * actual setup of the hardware.
  *
  * This function should be passed as a parameter to netif_add().
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return ERR_OK if the loopif is initialized
  *         ERR_MEM if private data couldn't be allocated
  *         any other err_t on error
  */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

/**
  * @brief  Returns the current time in milliseconds
  *         when LWIP_TIMERS == 1 and NO_SYS == 1
  * @param  None
  * @retval Current Time value
  */
u32_t sys_now(void)
{
  return HAL_GetTick();
}

/**
  * @brief  This function sets the netif link status.
  * @param  netif: the network interface
  * @retval None
  */
void ethernetif_set_link(struct netif *netif)
{
  uint32_t regvalue = 0;

  /* Read PHY_MISR*/
  HAL_ETH_ReadPHYRegister(&EthHandle, PHY_ISFR, &regvalue);

  /* Check whether the link interrupt has occurred or not */
  if ((regvalue & PHY_ISFR_INT4) != (uint16_t)RESET) {
    netif_set_link_down(netif);
  }

  HAL_ETH_ReadPHYRegister(&EthHandle, PHY_BSR, &regvalue);

  if ((regvalue & PHY_LINKED_STATUS) != (uint16_t)RESET) {
#if LWIP_IGMP
    if (!(netif->flags & NETIF_FLAG_IGMP)) {
      netif->flags |= NETIF_FLAG_IGMP;
      igmp_init();
      igmp_start(netif);
    }
#endif
    LOCK_TCPIP_CORE();
    netif_set_link_up(netif);
    UNLOCK_TCPIP_CORE();
  }
}

/**
  * @brief  Link callback function, this function is called on change of link status
  *         to update low level driver configuration.
  * @param  netif: The network interface
  * @retval None
  */
void ethernetif_update_config(struct netif *netif)
{
  uint32_t regvalue = 0;

  if (netif_is_link_up(netif)) {
    /* Restart the auto-negotiation */
    if (EthHandle.Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE) {

      /* Check Auto negotiation */
      HAL_ETH_ReadPHYRegister(&EthHandle, PHY_SR, &regvalue);
      if ((regvalue & PHY_SR_AUTODONE) != PHY_SR_AUTODONE) {
        goto error;
      }

      /* Configure the MAC with the Duplex Mode fixed by the auto-negotiation process */
      if ((regvalue & PHY_DUPLEX_STATUS) != (uint32_t)RESET) {
        /* Set Ethernet duplex mode to Full-duplex following the auto-negotiation */
        EthHandle.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
      } else {
        /* Set Ethernet duplex mode to Half-duplex following the auto-negotiation */
        EthHandle.Init.DuplexMode = ETH_MODE_HALFDUPLEX;
      }
      /* Configure the MAC with the speed fixed by the auto-negotiation process */
      if (regvalue & PHY_SPEED_STATUS) {
        /* Set Ethernet speed to 10M following the auto-negotiation */
        EthHandle.Init.Speed = ETH_SPEED_10M;
      } else {
        /* Set Ethernet speed to 100M following the auto-negotiation */
        EthHandle.Init.Speed = ETH_SPEED_100M;
      }
    } else { /* AutoNegotiation Disable */
error :
      /* Check parameters */
      assert_param(IS_ETH_SPEED(EthHandle.Init.Speed));
      assert_param(IS_ETH_DUPLEX_MODE(EthHandle.Init.DuplexMode));

      /* Set MAC Speed and Duplex Mode to PHY */
      HAL_ETH_WritePHYRegister(&EthHandle, PHY_BCR, ((uint16_t)(EthHandle.Init.DuplexMode >> 3) |
                                                     (uint16_t)(EthHandle.Init.Speed >> 1)));
    }

    /* ETHERNET MAC Re-Configuration */
    HAL_ETH_ConfigMAC(&EthHandle, (ETH_MACInitTypeDef *) NULL);

    /* Restart MAC interface */
    HAL_ETH_Start(&EthHandle);
  } else {
    /* Stop MAC interface */
    HAL_ETH_Stop(&EthHandle);
  }

  ethernetif_notify_conn_changed(netif);
}

/**
  * @brief  This function notify user about link status changement.
  * @param  netif: the network interface
  * @retval None
  */
__weak void ethernetif_notify_conn_changed(struct netif *netif)
{
  /* NOTE : This is function clould be implemented in user file
            when the callback is needed,
  */
  UNUSED(netif);
}

void ethernetif_status_changed(struct netif *netif) {
  uint32_t addr;

  if (!netif_is_up(netif) || !netif_is_link_up(netif))
    return;
  addr = stm32_eth_get_ipaddr();
  if (addr) {
    LOG_D("Current IP: %lu.%lu.%lu.%lu",
      (addr >>  0) & 0x000000ff,
      (addr >>  8) & 0x000000ff,
      (addr >> 16) & 0x000000ff,
      (addr >> 24) & 0x000000ff);
  }
}

/**
  * @brief  This function set a custom MAC address. This function must be called
  *         before ethernetif_init().
  * @param  mac: mac address
  * @retval None
  */
void ethernetif_set_mac_addr(const uint8_t *mac)
{
  if (mac != NULL) {
    memcpy(macaddress, mac, 6);
  }
}

#if LWIP_IGMP
err_t igmp_mac_filter(struct netif *netif, const ip4_addr_t *ip4_addr, netif_mac_filter_action action)
{
  uint8_t mac[6];
  const uint8_t *p = (const uint8_t *)ip4_addr;

  mac[0] = 0x01;
  mac[1] = 0x00;
  mac[2] = 0x5E;
  mac[3] = *(p + 1) & 0x7F;
  mac[4] = *(p + 2);
  mac[5] = *(p + 3);

  register_multicast_address(mac);

  return 0;
}

#ifndef HASH_BITS
#define HASH_BITS 6 /* #bits in hash */
#endif

uint32_t ethcrc(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xffffffff;
  size_t i;
  int j;

  for (i = 0; i < length; i++) {
    for (j = 0; j < 8; j++) {
      if (((crc >> 31) ^ (data[i] >> j)) & 0x01) {
        /* x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1 */
        crc = (crc << 1) ^ 0x04C11DB7;
      } else {
        crc = crc << 1;
      }
    }
  }
  return ~crc;
}

void register_multicast_address(const uint8_t *mac)
{
  uint32_t crc;
  uint8_t hash;

  /* Calculate crc32 value of mac address */
  crc = ethcrc(mac, HASH_BITS);

  /*
   * Only upper HASH_BITS are used
   * which point to specific bit in the hash registers
   */
  hash = (crc >> 26) & 0x3F;

  if (hash > 31) {
    ETH_HashTableHigh |= 1 << (hash - 32);
    EthHandle.Instance->MACHTHR = ETH_HashTableHigh;
  } else {
    ETH_HashTableLow |= 1 << hash;
    EthHandle.Instance->MACHTLR = ETH_HashTableLow;
  }
}
#endif /* LWIP_IGMP */


#ifdef ETH_INPUT_USE_IT
/**
  * @brief  Ethernet Rx Transfer completed callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
  (void)heth;
  ethernetif_input(&gnetif);
}

/**
  * @brief  This function handles Ethernet interrupt request.
  * @param  None
  * @retval None
  */
void ETH_IRQHandler(void)
{
  HAL_ETH_IRQHandler(&EthHandle);
}
#endif /* ETH_INPUT_USE_IT */

#ifdef __cplusplus
}
#endif
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
