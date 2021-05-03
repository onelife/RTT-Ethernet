/*
  Repeating Web client

 This sketch connects to a a web server and makes a request.

 This example uses DNS, by assigning the Ethernet client with a MAC address,
 IP address, and DNS address.

 Circuit:
 * STM32 board with Ethernet support

 created 19 Apr 2012
 by Tom Igoe
 modified 21 Jan 2014
 by Federico Vanzati
 modified 23 Jun 2017
 by Wi6Labs
 modified 1 Jun 2018
 by sstaub
 modified 16 Apr 2021
 by onelife
 
 http://www.arduino.cc/en/Tutorial/WebClientRepeating
 This code is in the public domain.

 */

#include <rtt.h>
#include <LwIP.h>
#include <RttEthernet.h>

#define LOG_TAG "WEB_CRP"
#include <log.h>

// assign a MAC address for the ethernet controller.
// fill in your address here:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 10, 85);
IPAddress myDns(192, 168, 10, 254);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);

// initialize the library instance:
EthernetClient client;

char server[] = "www.arduino.cc";  // also change the Host line in httpRequest()
//IPAddress server(64,131,82,241);

unsigned long lastConnectionTime = 0;           // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 10*1000;  // delay between updates, in milliseconds

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
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, myDns, gateway, subnet);
    IPAddress addr = Ethernet.localIP();
    LOG_I("My IP address: %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
  } else {
    IPAddress addr = Ethernet.localIP();
    LOG_I("  DHCP assigned IP %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
  }

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();

  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  if (client.available()) {
    int len = client.available();
    if (len > 0) {
      byte buffer[161];
      if (len > 160) {
        len = 160;
      }
      len = client.read(buffer, len);
      buffer[len] = 0;
      LOG_RAW("%s", buffer);
    }
  }

  // if ten seconds have passed since your last connection,
  // then connect again and send data:
  if (millis() - lastConnectionTime > postingInterval) {
    httpRequest();
  }
}

// this method makes a HTTP connection to the server:
void httpRequest() {
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  client.stop();

  // if there's a successful connection:
  if (client.connect(server, 80)) {
    LOG_I("connecting...");
    // send the HTTP GET request:
    client.println("GET /latest.txt HTTP/1.1");
    client.println("Host: www.arduino.cc");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  } else {
    // if you couldn't make a connection:
    LOG_I("connection failed");
  }
}

