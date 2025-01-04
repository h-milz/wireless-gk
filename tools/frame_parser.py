#!/usr/bin/env python3

import sys

# Configuration
INPUT_FILE = "i2s_data.raw"  # Replace with your file name
SAMPLE_SIZE = 3                # Each sample is 3 bytes
SAMPLES_PER_FRAME = 2          # 4 samples per frame
FRAME_SIZE = SAMPLE_SIZE * SAMPLES_PER_FRAME  # 12 bytes per frame

try:
    # Open the file in binary read mode
    with open(INPUT_FILE, "rb") as file:
        frame_number = 0

        while True:
            # Read one frame of data
            frame_data = file.read(FRAME_SIZE)

            # Break if we've reached the end of the file
            if not frame_data or len(frame_data) < FRAME_SIZE:
                break

            # Split the frame into four 3-byte samples
            samples = [frame_data[i:i + SAMPLE_SIZE] for i in range(0, FRAME_SIZE, SAMPLE_SIZE)]

            # Convert each sample to hexadecimal
            hex_samples = [sample.hex() for sample in samples]

            # Print the formatted output
            print(f"{frame_number:08d}", *hex_samples)

            # Increment the frame number
            frame_number += 1

except FileNotFoundError:
    print(f"Error: File '{INPUT_FILE}' not found.")
    sys.exit(1)
except Exception as e:
    print(f"An error occurred: {e}")
    sys.exit(1)

