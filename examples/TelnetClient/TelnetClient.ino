/*
  Telnet client

 This sketch connects to a a telnet server (http://www.google.com)
 using an Arduino Wiznet Ethernet shield.  You'll need a telnet server
 to test this with.
 Processing's ChatServer example (part of the network library) works well,
 running on port 10002. It can be found as part of the examples
 in the Processing application, available at
 http://processing.org/

 Circuit:
 * STM32 board with Ethernet support

 created 14 Sep 2010
 modified 9 Apr 2012
 by Tom Igoe
 modified 23 Jun 2017
 by Wi6Labs
 modified 1 Jun 2018
 by sstaub
 modified 16 Apr 2021
 by onelife

 */

#define ADD_MSH_CMD(name)
#include <user_cmd.h>

#include <rtt.h>
#include <LwIP.h>
#include <RttEthernet.h>

#define LOG_TAG "TEL_CL"
#include <log.h>

struct user_input {
    int available;
    char buffer[161];
};

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 10, 85);
IPAddress myDns(192, 168, 10, 254);
IPAddress gateway(192, 168, 10, 254);
IPAddress subnet(255, 255, 255, 0);

// Enter the IP address of the server you're connecting to:
IPAddress server(192, 168, 10, 16);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 23 is default for telnet;
// if you're using Processing's ChatServer, use port 10002):
EthernetClient client;

struct user_input user;

void setup() {
  RT_T.begin();
}

void setup_after_rtt_start() {
  static int init_done = 0;
  if (init_done) {
    return;
  }

  // start the Ethernet connection:
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  while (Ethernet.linkStatus() == LinkOFF) {
    LOG_I("Ethernet cable is not connected.");
    rt_thread_mdelay(500);
  }

  LOG_I("connecting...");

  // if you get a connection, report back via serial:
  if (client.connect(server, 10002)) {
    LOG_I("connected");
  } else {
    // if you didn't get a connection to the server:
    LOG_I("connection failed");
  }

  user.available = 0;
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
    buffer[len] = 0;
    LOG_RAW("%s\n", buffer); // show in the serial monitor (slows some boards)
  }

  // when there is user input, send the content out the socket:
  if ((user.available > 0) && (client.connected())) {
    client.write(user.buffer, user.available);
    user.available = 0;
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    LOG_I("\ndisconnecting.");
    client.stop();
    // do nothing:
    app_done = 1;
  }
}

#ifdef __cplusplus
extern "C" {
#endif

int telnet_send(int argc, char **argv) {
  if (argc != 2) {
    rt_kprintf("Usage: telnet \"message to send\"\n");
  } else {
    user.available = rt_strlen(argv[1]);
    if (user.available > 160) {
      user.available = 160;
    }
    rt_memcpy(user.buffer, argv[1], user.available);
    user.buffer[user.available] = 0;
  }

  return 0;
}
MSH_CMD_EXPORT_ALIAS(telnet_send, telnet, Send message to Telnet server.)

#ifdef __cplusplus
}
#endif
