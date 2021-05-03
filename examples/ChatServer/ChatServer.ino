/*
 Chat Server

 A simple server that distributes any incoming messages to all
 connected clients.  To use, telnet to your device's IP address and type.
 You can see the client's input in the serial monitor as well.

 Circuit:
 * STM32 board with Ethernet support

 created 18 Dec 2009
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
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

#define LOG_TAG "CHA_SRV"
#include <log.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 10, 85);
IPAddress myDns(192, 168, 10, 254);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);


// telnet defaults to port 23
EthernetServer server(23);
bool alreadyConnected = false; // whether or not the client was connected previously

void setup() {
  RT_T.begin();
}

void setup_after_rtt_start() {
  static int init_done = 0;
  if (init_done) {
    return;
  }

  // initialize the ethernet device
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  if (Ethernet.linkStatus() == LinkOFF) {
    LOG_I("Ethernet cable is not connected.");
  }

  // start listening for clients
  server.begin();

  IPAddress addr = Ethernet.localIP();
  LOG_I("Chat server address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();

  // wait for a new client:
  EthernetClient client = server.available();

  // when the client sends the first byte, say hello:
  if (client) {
    if (!alreadyConnected) {
      // clear out the input buffer:
      client.flush();
      LOG_I("We have a new client");
      client.println("Hello, client!");
      alreadyConnected = true;
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
