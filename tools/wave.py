#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Tue Jan 14 16:21:36 2025

@author: hm
"""

import numpy as np
import matplotlib.pyplot as plt

# Configuration parameters
filename = "../i2s_data.raw"
sample_rate = 441  # Adjust based on your audio stream
bit_depth = 32  # 16-bit audio
channels = 2  # Mono audio

# Read raw audio data
data = np.fromfile(filename, dtype=np.int32)

# Plot waveform
plt.figure(figsize=(12, 6))
plt.plot(data[:sample_rate])  # Plot 1 second of audio
plt.title("Waveform of PCM Audio Stream")
plt.xlabel("Sample Number")
plt.ylabel("Amplitude")
plt.show()
