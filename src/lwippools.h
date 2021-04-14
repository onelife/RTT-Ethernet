/***************************************************************************//**
 * @file    lwippools.h
 * @brief   Arduino RTT-Ethernet library LwIP memory pool header
 * @author  onelife <onelife.real[at]gmail.com>
 ******************************************************************************/
LWIP_MALLOC_MEMPOOL_START
LWIP_MALLOC_MEMPOOL(16, 365)
LWIP_MALLOC_MEMPOOL(16, 730)
LWIP_MALLOC_MEMPOOL(8, 1536)
LWIP_MALLOC_MEMPOOL_END
