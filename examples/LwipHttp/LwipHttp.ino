/*
 Web server and client

 LwIP App http

 created 1 May 2021
 by onelife

*/

#include <rtt.h>
#include <LwIP.h>
#include <RttEthernet.h>

#include <lwip/tcpip.h>
#include <lwip/apps/httpd.h>
#include <lwip/apps/http_client.h>

#define LOG_TAG "LW_HTTP"
#include <log.h>


// Comment out the following line to run client
#define HTTP_SERVER

void rx_result(void *arg, httpc_result_t httpc_result, u32_t rx_content_len,
  u32_t srv_res, err_t err);
err_t rx_header(httpc_state_t *stats, void *arg, struct pbuf *hdr,
  u16_t hdr_len, u32_t content_len);

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 10, 85);
IPAddress myDns(192, 168, 10, 254);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);

const httpc_connection_t config = {
    .result_fn = rx_result,
    .headers_done_fn = rx_header,
};
u32_t total_len = 0;
u32_t rx_len = 0;

void rx_result(void *arg, httpc_result_t httpc_result, u32_t rx_content_len,
  u32_t srv_res, err_t err) {
  (void)arg;

  if (HTTPC_RESULT_OK == httpc_result) {
    LOG_I("RX done");
  } else {
    LOG_W("RX error, %d, %d", httpc_result, err);
  }
  LOG_I("RX length: %d", rx_content_len);
  LOG_I("RX code: %d", srv_res);
}

err_t rx_header(httpc_state_t *stats, void *arg, struct pbuf *hdr,
  u16_t hdr_len, u32_t content_len) {
  (void)stats;
  (void)arg;
  (void)hdr;

  LOG_I("Header length: %d", hdr_len);
  LOG_I("Content length: %d", content_len);
  rx_len = 0;
  total_len = content_len;
  return ERR_OK;
}

err_t rx_body(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err) {
  (void)arg;
  (void)err;
  altcp_recved(conn, p->tot_len);
  rx_len += p->tot_len;
  pbuf_free(p);

  LOG_I("RX %3d%%, %6d/%6d", rx_len * 100 / total_len, rx_len, total_len);

  return ERR_OK;
}


void setup() {
  RT_T.begin();
}

void setup_after_rtt_start() {
  static int init_done = 0;
  if (init_done) {
    return;
  }

  // initialize the Ethernet device
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  if (Ethernet.linkStatus() == LinkOFF) {
    LOG_I("Ethernet cable is not connected.");
  }

  IPAddress addr = Ethernet.localIP();
  #ifdef HTTP_SERVER
  LOG_I("HTTP server address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
  #else
  LOG_I("HTTP client address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
  #endif

  #ifdef HTTP_SERVER
    LOCK_TCPIP_CORE();
    httpd_init();
    UNLOCK_TCPIP_CORE();
  #else
    static char host[] = "www.http2demo.io";
    u16_t port = 80;
    const char uri[] = "/img/refresh-icon.png";
    err_t ret;

    LOCK_TCPIP_CORE();
    ret = httpc_get_file_dns(host, port, uri, &config, rx_body, NULL, NULL);
    UNLOCK_TCPIP_CORE();
    if (ERR_OK != ret) {
      LOG_W("HTTP client error, %d", ret);
    } else {
      LOG_I("HTTP client started");
    }
  #endif

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();
}
