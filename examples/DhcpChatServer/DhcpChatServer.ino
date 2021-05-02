/*
 DHCP Chat  Server

 A simple server that distributes any incoming messages to all
 connected clients.  To use, telnet to your device's IP address and type.
 You can see the client's input in the serial monitor as well.

 THis version attempts to get an IP address using DHCP

 Circuit:
 * STM32 board with Ethernet support

 created 21 May 2011
 modified 9 Apr 2012
 by Tom Igoe
 modified 02 Sept 2015
 by Arturo Guadalupi
 Based on ChatServer example by David A. Mellis
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

#define LOG_TAG "DHCP_SRV"
#include <log.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
IPAddress ip(192, 168, 10, 85);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress myDns(192, 168, 10, 254);

// telnet defaults to port 23
EthernetServer server(23);
bool gotAMessage = false; // whether or not you got a message from the client yet

void setup() {
  RT_T.begin();
}

void setup_after_rtt_start() {
  static int init_done = 0;
  if (init_done) {
    return;
  }

  // start the Ethernet connection:
  LOG_I("Trying to get an IP address using DHCP");
  if (Ethernet.begin(mac) == 0) {
    LOG_I("Failed to configure Ethernet using DHCP");
    if (Ethernet.linkStatus() == LinkOFF) {
      LOG_I("Ethernet cable is not connected.");
    }
    // initialize the Ethernet device not using DHCP:
    Ethernet.begin(ip, subnet, gateway, myDns);
  }
  // print your local IP address:
  IPAddress addr = Ethernet.localIP();
  LOG_I("My IP address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);

  // start listening for clients
  server.begin();

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();

  // wait for a new client:
  EthernetClient client = server.available();

  // when the client sends the first byte, say hello:
  if (client) {
    if (!gotAMessage) {
      LOG_I("We have a new client");
      client.println("Hello, client!");
      gotAMessage = true;
    }

    int len = client.available();
    if (len > 0) {
      byte buffer[161];
      if (len > 160) {
        len = 160;
      }
      // read the bytes incoming from the client:
      len = client.read(buffer, len);
      // echo the bytes back to the client:
      server.write(buffer, len);
      // echo the bytes to the server as well:
      buffer[len] = 0;
      LOG_RAW("%s\n", buffer);
    }
  }
}
