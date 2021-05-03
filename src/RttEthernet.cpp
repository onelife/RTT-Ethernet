#include "RttEthernet.h"

#include "lwip/sys.h"
#include "lwip/dns.h"

// Possible return codes from ProcessResponse
#define SUCCESS          1
#define TIMED_OUT        -1
#define INVALID_SERVER   -2
#define TRUNCATED        -3
#define INVALID_RESPONSE -4


int EthernetClass::begin(unsigned long timeout)
{
  stm32_eth_init(MACAddressDefault(), NULL, NULL, NULL);

  // Now try to get our config info from a DHCP server
  int ret = beginWithDHCP(timeout);
  if (ret == 1) {
    _dnsServerAddress = getDnsServerIp();
  }

  return ret;
}

void EthernetClass::begin(IPAddress local_ip)
{
  IPAddress subnet(255, 255, 255, 0);
  begin(local_ip, subnet);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet)
{
  // Assume the gateway will be the machine on the same network as the local IP
  // but with last octet being '1'
  IPAddress gateway = local_ip;
  gateway[3] = 1;
  begin(local_ip, subnet, gateway);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet, IPAddress gateway)
{
  // Assume the DNS server will be the machine on the same network as the local IP
  // but with last octet being '1'
  IPAddress dns_server = local_ip;
  dns_server[3] = 1;
  begin(local_ip, subnet, gateway, dns_server);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet, IPAddress gateway, IPAddress dns_server)
{
  ip_addr_t ip;
  stm32_eth_init(MACAddressDefault(), local_ip.raw_address(), gateway.raw_address(), subnet.raw_address());
  /* If there is a local DHCP informs it of our manual IP configuration to
  prevent IP conflict */
  stm32_DHCP_manual_config();
  IP_ADDR4(&ip, dns_server[0], dns_server[1], dns_server[2], dns_server[3]);
  dns_setserver(0, &ip);
  _dnsServerAddress = dns_server;
}

int EthernetClass::begin(uint8_t *mac_address, unsigned long timeout)
{
  stm32_eth_init(mac_address, NULL, NULL, NULL);

  // Now try to get our config info from a DHCP server
  int ret = beginWithDHCP(timeout);
  if (ret == 1) {
    _dnsServerAddress = getDnsServerIp();
  }
  MACAddress(mac_address);
  return ret;
}

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip)
{
  // Assume the DNS server will be the machine on the same network as the local IP
  // but with last octet being '1'
  IPAddress dns_server = local_ip;
  dns_server[3] = 1;
  begin(mac_address, local_ip, dns_server);
}

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server)
{
  // Assume the gateway will be the machine on the same network as the local IP
  // but with last octet being '1'
  IPAddress gateway = local_ip;
  gateway[3] = 1;
  begin(mac_address, local_ip, dns_server, gateway);
}

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway)
{
  IPAddress subnet(255, 255, 255, 0);
  begin(mac_address, local_ip, dns_server, gateway, subnet);
}

void EthernetClass::begin(uint8_t *mac, IPAddress local_ip, IPAddress dns_server, IPAddress gateway, IPAddress subnet)
{
  ip_addr_t ip;
  stm32_eth_init(mac, local_ip.raw_address(), gateway.raw_address(), subnet.raw_address());
  /* If there is a local DHCP informs it of our manual IP configuration to
  prevent IP conflict */
  stm32_DHCP_manual_config();
  IP_ADDR4(&ip, dns_server[0], dns_server[1], dns_server[2], dns_server[3]);
  dns_setserver(0, &ip);
  _dnsServerAddress = dns_server;
  MACAddress(mac);
}

EthernetLinkStatus EthernetClass::linkStatus()
{
  return (!stm32_eth_is_init()) ? Unknown : (stm32_eth_link_up() ? LinkON : LinkOFF);
}

int EthernetClass::maintain()
{
  uint8_t rc = checkLease();

  if ((DHCP_CHECK_RENEW_OK == rc) || (DHCP_CHECK_REBIND_OK == rc))
    _dnsServerAddress = getDnsServerIp();

  return (int)rc;
}

uint8_t *EthernetClass::MACAddressDefault(void)
{
  if ((mac_address[0] + mac_address[1] + mac_address[2] + mac_address[3] + mac_address[4] + mac_address[5]) == 0) {
    uint32_t baseUID = *(uint32_t *)UID_BASE;
    mac_address[0] = 0x00;
    mac_address[1] = 0x80;
    mac_address[2] = 0xE1;
    mac_address[3] = (baseUID & 0x00FF0000) >> 16;
    mac_address[4] = (baseUID & 0x0000FF00) >> 8;
    mac_address[5] = (baseUID & 0x000000FF);
  }
  return mac_address;
}

void EthernetClass::MACAddress(uint8_t *mac)
{
  mac_address[0] = mac[0];
  mac_address[1] = mac[1];
  mac_address[2] = mac[2];
  mac_address[3] = mac[3];
  mac_address[4] = mac[4];
  mac_address[5] = mac[5];
}

uint8_t *EthernetClass::MACAddress(void)
{
  return mac_address;
}

IPAddress EthernetClass::localIP()
{
  return IPAddress(stm32_eth_get_ipaddr());
}

IPAddress EthernetClass::subnetMask()
{
  return IPAddress(stm32_eth_get_netmaskaddr());
}

IPAddress EthernetClass::gatewayIP()
{
  return IPAddress(stm32_eth_get_gwaddr());
}

IPAddress EthernetClass::getDhcpServerIp()
{
  return IPAddress(stm32_eth_get_dhcpaddr());
}

IPAddress EthernetClass::getDnsServerIp()
{
  return IPAddress(stm32_eth_get_dnsaddr());
}

int EthernetClass::beginWithDHCP(unsigned long timeout)
{
  _dhcp_lease_state = DHCP_CHECK_NONE;
  uint8_t is_timeout = 0;
  unsigned long startTime = millis();

  stm32_set_DHCP_state(DHCP_START);

  while (stm32_get_DHCP_state() != DHCP_ADDRESS_ASSIGNED) {
    if ((millis() - startTime) > timeout) {
      is_timeout = 1;
      stm32_set_DHCP_state(DHCP_ASK_RELEASE);
      break;
    }
    sys_msleep(20);
  }

  if (is_timeout) {
    return 0;
  }
  return 1;
}

/*
    returns:
    0/DHCP_CHECK_NONE: nothing happened
    1/DHCP_CHECK_RENEW_FAIL: renew failed
    2/DHCP_CHECK_RENEW_OK: renew success
    3/DHCP_CHECK_REBIND_FAIL: rebind fail
    4/DHCP_CHECK_REBIND_OK: rebind success
*/
uint8_t EthernetClass::checkLease()
{
  uint8_t last = _dhcp_lease_state;
  uint8_t current = stm32_get_DHCP_lease_state();

  do {
    if (current == last)
      break;
    _dhcp_lease_state = current;

    if (DHCP_CHECK_NONE == last) {
      current = DHCP_CHECK_NONE;
    } else if (DHCP_CHECK_RENEW_OK == last) {
      if (DHCP_CHECK_NONE == current)
        current = DHCP_CHECK_RENEW_OK;
      else
        current = DHCP_CHECK_RENEW_FAIL;
    } else if (DHCP_CHECK_REBIND_OK == last) {
      if (DHCP_CHECK_NONE == current)
        current = DHCP_CHECK_REBIND_OK;
      else
        current = DHCP_CHECK_REBIND_FAIL;
    } else {
      _dhcp_lease_state = DHCP_CHECK_NONE;
    }
  } while (0);

  return current;
}

IPAddress EthernetClass::dnsServerIP()
{
  return _dnsServerAddress;
}

int EthernetClass::getHostByName(const char *aHostname, IPAddress &aResult)
{
  int ret = 0;
  uint32_t ipResult = 0;

  // Check we've got a valid DNS server to use
  if ((uint32_t)_dnsServerAddress == 0) {
    return INVALID_SERVER;
  }

  ret = stm32_dns_gethostbyname(aHostname, &ipResult);
  aResult = IPAddress(ipResult);

  return ret;
}

EthernetClass Ethernet;
