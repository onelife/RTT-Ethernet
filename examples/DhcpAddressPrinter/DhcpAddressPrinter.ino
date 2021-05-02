/*
  DHCP-based IP printer

  This sketch uses the DHCP extensions to the Ethernet library
  to get an IP address via DHCP and print the address obtained.

  Circuit:
   STM32 board with Ethernet support

  created 12 April 2011
  modified 9 Apr 2012
  by Tom Igoe
  modified 02 Sept 2015
  by Arturo Guadalupi
  modified 23 Jun 2017
  by Wi6Labs
  modified 1 Jun 2018
  by sstaub
  modified 16 Apr 2021
  by onelife

*/

#include <rtt.h>
#include <LwIP.h>
#include <RttEthernet.h>

#define LOG_TAG "DHCP_PRT"
#include <log.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

int app_done = 0;

void setup() {
  RT_T.begin();
}

void setup_after_rtt_start() {
  static int init_done = 0;
  if (init_done) {
    return;
  }

  // start the Ethernet connection:
  LOG_I("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    LOG_I("Failed to configure Ethernet using DHCP");
    if (Ethernet.linkStatus() == LinkOFF) {
      LOG_I("Ethernet cable is not connected.");
    }
    // no point in carrying on, so do nothing forevermore:
    init_done = 1;
    app_done = 1;
    return;
  }
  // print your local IP address:
  IPAddress addr = Ethernet.localIP();
  LOG_I("My IP address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();
  if (app_done) {
    return;
  }

  switch (Ethernet.maintain()) {
    case 1:
      //renewed fail
      LOG_I("Error: renewed fail");
      break;

    case 2:
    {
      //renewed success
      LOG_I("Renewed success");
      //print your local IP address:
      IPAddress addr = Ethernet.localIP();
      LOG_I("My IP address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
      break;
    }

    case 3:
      //rebind fail
      LOG_I("Error: rebind fail");
      break;

    case 4:
    {
      //rebind success
      LOG_I("Rebind success");
      //print your local IP address:
      IPAddress addr = Ethernet.localIP();
      LOG_I("My IP address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
      break;
    }

    default:
      //nothing happened
      break;
  }
}
