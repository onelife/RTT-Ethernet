/*
 Advanced Chat Server

 A more advanced server that distributes any incoming messages
 to all connected clients but the client the message comes from.
 To use, telnet to your device's IP address and type.
 You can see the client's input in the serial monitor as well.

 Circuit:
 * STM32 board with Ethernet support

 created 18 Dec 2009
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
 redesigned to make use of operator== 25 Nov 2013
 by Norbert Truchsess
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

#define LOG_TAG "ADV_CHA"
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

EthernetClient clients[8];

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

  // start listening for clients
  server.begin();

  IPAddress addr = Ethernet.localIP();
  LOG_I("Chat server address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();

  // check for any new client connecting, and say hello (before any incoming data)
  EthernetClient newClient = server.accept();
  if (newClient) {
    for (byte i=0; i < 8; i++) {
      if (!clients[i]) {
        LOG_I("We have a new client #%d", i);
        newClient.print("Hello, client number: ");
        newClient.println(i);
        // Once we "accept", the client is no longer tracked by EthernetServer
        // so we must store it into our list of clients
        clients[i] = newClient;
        break;
      }
    }
  }

  // check for incoming data from all clients
  for (byte i=0; i < 8; i++) {
    if (clients[i] && clients[i].available() > 0) {
      // read bytes from a client
      byte buffer[161];
      int count = clients[i].read(buffer, 160);
      buffer[count] = 0;
      LOG_RAW("%s\n", buffer);
      // write the bytes to all other connected clients
      for (byte j=0; j < 8; j++) {
        if (j != i && clients[j].connected()) {
          LOG_I("write %d", i);
          clients[j].write(buffer, count);
        }
      }
      LOG_I("available %d", clients[i].available());
    }
  }

  // stop any clients which disconnect
  for (byte i=0; i < 8; i++) {
    if (clients[i] && !clients[i].connected()) {
      LOG_I("disconnect client #%d", i);
      clients[i].stop();
    }
  }
}
