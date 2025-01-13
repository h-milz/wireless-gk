#!/usr/bin/env python3

import socket

# Configuration
UDP_IP = "0.0.0.0"  # Listen on all available interfaces
UDP_PORT = 45678     # Port to listen on
OUTPUT_FILE = "i2s_data.raw"
buf_size = 1280     # 40 frames 32 byte each

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening for UDP packets on {UDP_IP}:{UDP_PORT}...\n")

packets = 0
# Open the output file in append mode
with open(OUTPUT_FILE, "wb") as file:
    while True:
        # Receive data from a client
        data, addr = sock.recvfrom(buf_size)  # Buffer size is 1024 bytes
        # print(f"Received data from {addr}: {data.decode('utf-8', 'ignore')}")
        
        # Write data to the file
        file.write(data)
        # file.flush()  # Ensure data is written to disk immediately
        packets = packets + 1
        # print (f"p = {packets}")
        if packets % 300 == 0:
            print (".", end="", flush=True)
            

