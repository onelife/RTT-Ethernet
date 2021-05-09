/*
 Socket API

 LwIP socket example

 created 8 May 2021
 by onelife

*/

#include <rtt.h>
#include <LwIP.h>
#include <RttEthernet.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <socket_examples.h>
#ifdef __cplusplus
}
#endif

#define LOG_TAG "LW_SOCK"
#include <log.h>


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 10, 85);
IPAddress myDns(192, 168, 10, 254);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);


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
  LOG_I("My IP address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);

  socket_examples_init();

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();
}
