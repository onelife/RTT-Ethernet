# RTT Ethernet Library for Arduino

This project is forked from [STM32Ethernet](https://github.com/stm32duino/STM32Ethernet) and modified to support both RAW and socket API.

It is compatible with [Arduino Ethernet library](https://www.arduino.cc/en/Reference/Ethernet), allows an Arduino board (STM32 architecture for now) to connect with Internet.


## Dependence

* [RT-Thread Library](https://github.com/onelife/Arduino_RT-Thread)
  - Enable `RT_USING_MUTEX`, `RT_USING_SEMAPHORE`, `RT_USING_MESSAGEQUEUE`, `RT_USING_EVENT` and `RT_USING_MEMPOOL`     

* [RTT-CMSIS-OS Library](https://github.com/onelife/RTT-CMSIS-OS)

* [LwIP Library](https://github.com/stm32duino/LwIP)


## Configuration

* Default network config
  - MAC: `00:80:E1:FF:FF:FF`
  - Subnet: `255.255.255.0`
  - Gateway: `xxx.xxx.xxx.1`

* Default number of DMA descriptors (STM32 architecture)
  - Defined in e.g. `.arduino15/packages/STM32/hardware/stm32/1.9.0/system/STM32F7xx/stm32f7xx_hal_conf_default.h`
    - ETH_RXBUFNB == 4
    - ETH_TXBUFNB == 4

* Default Ethernet buffer size
  - Defined in e.g. `.arduino15/packages/STM32/hardware/stm32/1.9.0/system/STM32F7xx/stm32f7xx_hal_conf_default.h`
    - ETH_RX_BUF_SIZE == 1524 (bytes)
    - ETH_TX_BUF_SIZE == 1524 (bytes)

* Default number of buffers (STM32 architecture)
  - RX buffers defined in `utility/ethernetif.cpp`
    - Rx_Buff == ETH_RXBUFNB * 2
  - TX buffers defined in `lwippools.h`
    - 372-byte * 8
    - 736-byte * 8
    - 1540-byte * 8

* Notes
  - It is recommended to set ETH_RXBUFNB to 8 when running examples.


## Initialization

* `Ethernet.begin()`
* `Ethernet.begin(ip)`
* `Ethernet.begin(ip, subnet)`
* `Ethernet.begin(ip, subnet, gateway)`
* `Ethernet.begin(ip, subnet, gateway, dns)`

* `Ethernet.begin(mac)`
* `Ethernet.begin(mac, ip)`
* `Ethernet.begin(mac, ip, dns)`
* `Ethernet.begin(mac, ip, dns, gateway)`
* `Ethernet.begin(mac, ip, dns, gateway, subnet)`


## Examples

For all examples, please select the suitable initialization method and parameters accordingly.

* Ported from Arduino Ethernet library
  - WebClient
    - Connect to specified host (www.github.com) once.
  - WebClientRepeating
    - Connect to specified host (www.arduino.cc) repeatedly.
  - WebServer
    - Serve a HTTP page with port 80.
  - ChatServer
    - Echo TCP message with fixed IP address (`192.168.10.85` by default) and telnet port (23)
  - DhcpChatServer
    - Echo TCP message with telnet port (23)
  - AdvancedChatServer
    - Echo TCP message with telnet port (23) by using `Server.accept()` method.
    - Accept up to 8 clients, and forward messages from one client to the rests.
  - DhcpAddressPrinter
    - Print DHCP assigned IP address.
  - TelnetClient
    - A simple telnet client with MSH command `telnet` to send message to server.
    - A Python script of telnet echo server, "telnet_server.py".
    - Run the Python script before starting the example.
  - BarometricPressureWebServer
    - Serves the output of a Barometric Pressure Sensor as a web page.
  - UDPSendReceiveString
    - A example to send and receive UDP messages.
    - A Python script to send and receive UDP messages, "udp_test.py".
    - Start the example before running Python script.
  - UdpNtpClient
    A simple NTP client.

* LwIP App
  - LwipHttp
    - RAW API example web server and client
  - LwipSocket
    - Socket API example of `nonblocking`, `receive` and `select()` functions.
    - Set the host address and port in "socket_examples.h".
    - Start a simple HTTP server by Python, `python -m http.server 8080`, before starting the example.
  - LwipIperf
    - RAW API example of iPerf TCP server and client
