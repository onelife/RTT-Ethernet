/*
  Web client

 This sketch connects to a website (http://www.github.com)

 Circuit:
 * STM32 board with Ethernet support

 created 18 Dec 2009
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe, based on work by Adrian McEwen
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

#define LOG_TAG "WEB_CL"
#include <log.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(192.30.255.113);  // numeric IP for GitHub (no DNS)
char server[] = "www.github.com";    // name address for GitHub (using DNS)

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 10, 85);
IPAddress myDns(192, 168, 10, 254);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

// Variables to measure the speed
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;
bool printWebData = true;  // set to false for better speed measurement

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
    // try to configure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, myDns, gateway, subnet);
  } else {
    IPAddress addr = Ethernet.localIP();
    LOG_I("  DHCP assigned IP %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
  }
  LOG_I("connecting to %s ...", server);

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    IPAddress addr = client.remoteIP();
    LOG_I("connected to %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
    // Make a HTTP request:
    client.println("GET /search?q=arduino HTTP/1.1");
    client.println("Host: www.github.com");
    client.println("Connection: close");
    client.println();
  } else {
    // if you didn't get a connection to the server:
    LOG_I("connection failed\n");
  }
  beginMicros = micros();

  init_done = 1;
}

void loop() {
  static int app_done = 0;
  if (app_done) {
    return;
  }

  setup_after_rtt_start();

  // if there are incoming bytes available
  // from the server, read them and print them:
  int len = client.available();
  if (len > 0) {
    byte buffer[161];
    if (len > 160) {
      len = 160;
    }
    len = client.read(buffer, len);
    if (printWebData) {
      buffer[len] = 0;
      LOG_RAW("%s", buffer); // show in the serial monitor (slows some boards)
    }
    byteCount = byteCount + len;
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    endMicros = micros();
    LOG_I("\ndisconnecting.");
    client.stop();
    float seconds = (float)(endMicros - beginMicros) / 1000000.0;
    float rate = (float)byteCount / seconds / 1000.0;
    // Sorry, no floating number printing support
    int seconds_ = int((seconds - (int)seconds) * 100000);
    if (seconds_ % 10 >= 5) {
      seconds_ += 10;
    }
    seconds_ /= 10;
    int rate_ = int((rate - (int)rate) * 1000);
    if (rate_ % 10 >= 5) {
      rate_ += 10;
    }
    rate_ /= 10;
    LOG_I("Received %d bytes in %d.%04d, rate = %d.%02d kbytes/second",
      byteCount, int(seconds), seconds_, int(rate), rate_);

    // do nothing forevermore:
    app_done = 1;
  }
}

