/*

 Udp NTP Client

 Get the time from a Network Time Protocol (NTP) time server
 Demonstrates use of UDP sendPacket and ReceivePacket
 For more on NTP time servers and the messages needed to communicate with them,
 see http://en.wikipedia.org/wiki/Network_Time_Protocol

 created 4 Sep 2010
 by Michael Margolis
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

 This code is in the public domain.

 */

#include <rtt.h>
#include <LwIP.h>
#include <RttEthernet.h>
#include <EthernetUdp.h>

#define LOG_TAG "NTP_CL"
#include <log.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

unsigned int localPort = 8888;       // local port to listen for UDP packets

const char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  RT_T.begin();
}

void setup_after_rtt_start() {
  static int init_done = 0;
  if (init_done) {
    return;
  }

  // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
    LOG_I("Failed to configure Ethernet using DHCP");
    if (Ethernet.linkStatus() == LinkOFF) {
      LOG_I("Ethernet cable is not connected.");
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      rt_thread_mdelay(1000);
    }
  }
  Udp.begin(localPort);

  init_done = 1;
}

void loop() {
  setup_after_rtt_start();

  sendNTPpacket(timeServer); // send an NTP packet to a time server

  // wait to see if a reply is available
  rt_thread_mdelay(1000);
  if (Udp.parsePacket()) {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    LOG_I("Seconds since Jan 1 1900 = %u", secsSince1900);

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    LOG_I("Unix time = %u", epoch);


    // print the hour, minute and second:
    LOG_I("The UTC time is %02u:%02u:%02u",   // UTC is the time at Greenwich Meridian (GMT)
      (epoch  % 86400L) / 3600,               // print the hour (86400 equals secs per day)
      (epoch  % 3600) / 60,                   // print the minute (3600 equals secs per minute)
      (epoch % 60)                            // print the second
    );
  }
  // wait ten seconds before asking for the time again
  rt_thread_mdelay(10000);
  Ethernet.maintain();
}

// send an NTP request to the time server at the given address
void sendNTPpacket(const char * address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
