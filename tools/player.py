#!/usr/bin/env python3

import sys

# Constants
FRAME_SIZE = 32  # Each frame has 24 bytes (8 samples in S24_BE format)
CHANNELS_TO_KEEP = 8  # We want to keep only the first 6 bytes (2 channels)

# Read and process input in chunks of FRAME_SIZE
while True:
    frame = sys.stdin.buffer.read(FRAME_SIZE)
    if not frame:
        break

    # Extract the first 6 bytes for channels #0 and #1
    sys.stdout.buffer.write(frame[:CHANNELS_TO_KEEP])

