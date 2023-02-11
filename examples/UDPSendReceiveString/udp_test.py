# -*- coding: utf-8 -*-

import socket
from time import sleep

# telnet port
PORT = 8888
REMOTE_IP = "192.168.10.85"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)

cnt = 1
while True:
    # send message
    msg = f"hi x{cnt}, from host"
    sock.sendto(msg.encode("utf-8"), (REMOTE_IP, PORT))
    print(f"Send message to {REMOTE_IP}:{PORT}, {msg}")

    # get reply
    msg, addr = sock.recvfrom(1024)
    if msg:
        print(f"Got reply from {addr}: {msg.decode('utf-8')}")

    cnt += 1
    sleep(5)
