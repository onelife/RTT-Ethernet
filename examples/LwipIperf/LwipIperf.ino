/*
 iPerf TCP server and client

 LwIP App lwiperf

 created 1 May 2021
 by onelife

*/

#include <rtt.h>
#include <LwIP.h>
#include <RttEthernet.h>

#include <lwip/tcpip.h>
#include <lwip/apps/lwiperf.h>

#define LOG_TAG "LW_IPERF"
#include <log.h>

// Comment out the following line to run client
#define IPERF_SERVER

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 10, 85);
IPAddress myDns(192, 168, 10, 254);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);

// Enter the iPerf server IP address:
const ip_addr_t remote_addr = IPADDR4_INIT_BYTES(192, 168, 10, 9);

void setup() {
  RT_T.begin();
}

void lwiperf_report(void *arg, enum lwiperf_report_type report_type,
  const ip_addr_t* local_addr, u16_t local_port,
  const ip_addr_t* remote_addr, u16_t remote_port,
  u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec) {
  (void)arg;

  switch (report_type) {
  case LWIPERF_TCP_DONE_SERVER:
    LOG_I("[iPerf Server Done]");
    break;
  case LWIPERF_TCP_DONE_CLIENT:
    LOG_I("[iPerf Client Done]");
    break;
  case LWIPERF_TCP_ABORTED_LOCAL:
    LOG_I("[iPerf Aborted]");
    break;
  case LWIPERF_TCP_ABORTED_LOCAL_DATAERROR:
    LOG_I("[iPerf Data Error]");
    break;
  case LWIPERF_TCP_ABORTED_LOCAL_TXERROR:
    LOG_I("[iPerf TX Error]");
    break;
  case LWIPERF_TCP_ABORTED_REMOTE:
    LOG_I("[iPerf Aborted by Remote]");
    break;
  default:
    break;
  }
  LOG_I(
    "Local: %u.%u.%u.%u:%d",
    ((ip_2_ip4(local_addr)->addr) >> 0) & 0xff,
    ((ip_2_ip4(local_addr)->addr) >> 8) & 0xff,
    ((ip_2_ip4(local_addr)->addr) >> 16) & 0xff,
    ((ip_2_ip4(local_addr)->addr) >> 24) & 0xff,
    local_port);
  LOG_I(
    "Remote: %u.%u.%u.%u:%d",
    ((ip_2_ip4(remote_addr)->addr) >> 0) & 0xff,
    ((ip_2_ip4(remote_addr)->addr) >> 8) & 0xff,
    ((ip_2_ip4(remote_addr)->addr) >> 16) & 0xff,
    ((ip_2_ip4(remote_addr)->addr) >> 24) & 0xff,
    remote_port);
  LOG_I("Transferred: %d bytes", bytes_transferred);
  LOG_I("Duration: %d ms", ms_duration);
  LOG_I("Bandwidth: %d kbps", bandwidth_kbitpsec);
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
  #ifdef IPERF_SERVER
  LOG_I("iPerf server address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
  #else
  LOG_I("iPerf client address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
  #endif

  LOCK_TCPIP_CORE();
  #ifdef IPERF_SERVER
  (void)lwiperf_start_tcp_client_default(&remote_addr, lwiperf_report, NULL);
  #else
  (void)lwiperf_start_tcp_server_default(lwiperf_report, NULL);
  #endif
  UNLOCK_TCPIP_CORE();

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();
}
