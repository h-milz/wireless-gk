#!/usr/bin/env python3

import sys

# Configuration
INPUT_FILE = "i2s_data.raw"  # Replace with your file name
SAMPLE_SIZE = 3                # Each sample is 3 bytes
SAMPLES_PER_FRAME = 8          # 4 samples per frame
FRAME_SIZE = SAMPLE_SIZE * SAMPLES_PER_FRAME  # 12 bytes per frame

prev_count = -1
curr_count = 0
sixty = False
diff = 0
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
            # print (f"------ {samples[1].hex()}")
            if samples[1].hex() == "ff0000":
                # print ("here")
                curr_count = int.from_bytes(samples[0])
                diff = curr_count - prev_count - 1
                prev_count = curr_count
                sixty = True
                        
            # Convert each sample to hexadecimal
            hex_samples = [sample.hex() for sample in samples]

            # Print the formatted output
            print(f"{frame_number:08d}", *hex_samples, end='')
            
            if diff != 0 and sixty:
                print (f"  lost {diff} packets:")
            else:
                print ("")
            sixty=False    
                
            # Increment the frame number
            frame_number += 1

except FileNotFoundError:
    print(f"Error: File '{INPUT_FILE}' not found.")
    sys.exit(1)
except Exception as e:
    print(f"An error occurred: {e}")
    sys.exit(1)

