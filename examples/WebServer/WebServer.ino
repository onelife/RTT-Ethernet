/*
  Web Server

 A simple web server that shows the value of the analog input pins.

 Circuit:
 * STM32 board with Ethernet support
 * Analog inputs attached to pins A0 through A5 (optional)

 created 18 Dec 2009
 by David A. Mellis
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

#define LOG_TAG "WEB_SRV"
#include <log.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 10, 85);
IPAddress myDns(192, 168, 10, 254);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

void setup() {
  RT_T.begin();
}

void setup_after_rtt_start() {
  static int init_done = 0;
  if (init_done) {
    return;
  }

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  if (Ethernet.linkStatus() == LinkOFF) {
    LOG_I("Ethernet cable is not connected.");
  }

  // start the server
  server.begin();
  IPAddress addr = Ethernet.localIP();
  LOG_I("server is at %u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    LOG_I("new client");
    while (client.connected()) {
      int len = client.available();
      if (len > 0) {
        byte buffer[161];
        if (len > 160) {
          len = 160;
        }
        len = client.read(buffer, len);
        buffer[len] = 0;
        LOG_RAW("%s", buffer);
      } else {
        // send a standard http response header
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");  // the connection will be closed after completion of the response
        client.println("Refresh: 5");  // refresh the page automatically every 5 sec
        client.println();
        client.println("<!DOCTYPE HTML>");
        client.println("<html>");
        // output the value of each analog input pin
        for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
          int sensorReading = analogRead(analogChannel);
          client.print("analog input ");
          client.print(analogChannel);
          client.print(" is ");
          client.print(sensorReading);
          client.println("<br />");
        }
        client.println("</html>");
        break;
      }
    }
    // give the web browser time to receive the data
    rt_thread_mdelay(100);
    // close the connection:
    client.stop();
    LOG_I("client disconnected");
  }
}

