# -*- coding: utf-8 -*-

import socket

# telnet port
PORT = 10002
IP_ADDR = None

if IP_ADDR is None:
    sock0 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock0.settimeout(0)
    try:
        sock0.connect(("1.2.3.4", 1))
        IP_ADDR = sock0.getsockname()[0]
    except Exception:
        print("Please set host IP address, IP_ADDR")
        exit(1)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind((IP_ADDR, PORT))
print(f"Start Telnet server, {IP_ADDR}:{PORT}")

# start listening for clients
sock.listen(5)

client, addr = sock.accept()
print(f"Connected by {addr}")

while True:
    msg = client.recv(1024)
    print(f"RX: {msg.decode('utf-8')}")
    if not msg:
        break
    client.sendall(b"RX: " + msg)

client.close()
