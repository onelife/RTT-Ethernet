extern "C" {
#include "string.h"
}

#include "RttEthernet.h"
#include "EthernetClient.h"
#include "EthernetServer.h"

#include "lwip/tcpip.h"

#define LOG_TAG "ETH_SRV"
#include <log.h>

EthernetServer::EthernetServer(uint16_t port)
{
  _port = port;
  for (int n = 0; n < MAX_CLIENT; n++) {
    _tcp_client[n] = {};
  }
  _tcp_server = {};
}

void EthernetServer::begin()
{
  if (_tcp_server.pcb != NULL) {
    return;
  }

  LOCK_TCPIP_CORE();
  _tcp_server.pcb = tcp_new();
  UNLOCK_TCPIP_CORE();
  if (_tcp_server.pcb == NULL) {
    return;
  }

  _tcp_server.state = TCP_NONE;
  LOCK_TCPIP_CORE();
  tcp_arg(_tcp_server.pcb, &_tcp_client);
  if (ERR_OK != tcp_bind(_tcp_server.pcb, IP_ADDR_ANY, _port)) {
    UNLOCK_TCPIP_CORE();
    memp_free(MEMP_TCP_PCB, _tcp_server.pcb);
    _tcp_server.pcb = NULL;
    return;
  }

  _tcp_server.pcb = tcp_listen(_tcp_server.pcb);
  tcp_accept(_tcp_server.pcb, tcp_accept_callback);
  UNLOCK_TCPIP_CORE();
}

void EthernetServer::checkClient()
{
  /* Free client if disconnected */
  for (int n = 0; n < MAX_CLIENT; n++) {
    if (_tcp_client[n] != NULL) {
      EthernetClient client(_tcp_client[n]);
      if (client.status() == TCP_CLOSING) {
        LOG_D("--- free tcp #%d %p", n, _tcp_client[n]);
        mem_free(_tcp_client[n]);
        _tcp_client[n] = NULL;
      }
    }
  }
}

EthernetClient EthernetServer::accept()
{
  checkClient();

  for (int n = 0; n < MAX_CLIENT; n++) {
    if (_tcp_client[n] != NULL) {
      if (_tcp_client[n]->pcb != NULL) {
        EthernetClient client(_tcp_client[n]);
        uint8_t s = client.status();
        if (s == TCP_ACCEPTED) {
          _tcp_client[n]->is_accept = 1;
          _tcp_client[n] = NULL;
          return client;
        }
      }
    }
  }

  struct tcp_struct *default_client = NULL;
  return EthernetClient(default_client);
}

EthernetClient EthernetServer::available()
{
  checkClient();

  for (int n = 0; n < MAX_CLIENT; n++) {
    if (_tcp_client[n] != NULL) {
      if (_tcp_client[n]->pcb != NULL) {
        EthernetClient client(_tcp_client[n]);
        uint8_t s = client.status();
        if (s == TCP_ACCEPTED) {
          if (client.available()) {
            return client;
          }
        }
      }
    }
  }

  struct tcp_struct *default_client = NULL;
  return EthernetClient(default_client);
}

size_t EthernetServer::write(uint8_t b)
{
  return write(&b, 1);
}

size_t EthernetServer::write(const uint8_t *buffer, size_t size)
{
  size_t n = 0;

  checkClient();

  for (int n = 0; n < MAX_CLIENT; n++) {
    if (_tcp_client[n] != NULL) {
      if (_tcp_client[n]->pcb != NULL) {
        EthernetClient client(_tcp_client[n]);
        uint8_t s = client.status();
        if (s == TCP_ACCEPTED) {
          n += client.write(buffer, size);
        }
      }
    }
  }

  return n;
}
