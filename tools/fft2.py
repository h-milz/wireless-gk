#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sun Jan 26 12:25:42 2025

@author: hm
"""

# we build a 441 Hz sime wave sampled at 44.1 kHz that has random jumps in either 
# direction at 60-sample boundaries 1-2 times per second. 
# The spectral noise can clearly be seen, and it is hard to filter it at the output,
# in particular when the input signal is not a sine wave but a music signal containing
# a wild mix of frequencies. 

import numpy as np
import matplotlib.pyplot as plt

fs = 44100  # Sampling rate
f = 441     # Frequency of sine wave
duration = 3  # Duration in seconds
samples = duration * fs

t = np.arange(samples) / fs
sine_wave = np.sin(2 * np.pi * f * t)

# Find 60-sample boundaries
boundary_positions = np.arange(60, samples, 60)

# Select random boundaries for jumps (1-2 per second)
num_jumps = np.random.randint(duration, 2 * duration)  # 1-2 jumps per second
jump_indices = np.random.choice(boundary_positions, num_jumps, replace=False)

# Generate random jump magnitudes (±10% of local amplitude)
jump_magnitudes = np.random.uniform(-0.1, 0.1, num_jumps) * sine_wave[jump_indices]

# Apply jumps
for idx, mag in zip(jump_indices, jump_magnitudes):
    sine_wave[idx:] += mag  # Offset all future samples

# Plot the wave (first 3000 samples for visibility)
plt.figure(figsize=(10, 4))
plt.plot(t[:3000], sine_wave[:3000], label="Sine Wave with Random Jumps")
plt.xlabel("Time (s)")
plt.ylabel("Amplitude")
plt.title("441 Hz Sine Wave with Random Jumps at 60-sample Boundaries")
plt.grid()
plt.legend()
plt.show()

# Compute FFT
fft_spectrum = np.fft.fft(sine_wave)
frequencies = np.fft.fftfreq(samples, d=1/fs)
magnitude = np.abs(fft_spectrum)[:samples // 2]
frequencies = frequencies[:samples // 2]

# Plot FFT
plt.figure(figsize=(10, 5))
plt.plot(frequencies, 20 * np.log10(magnitude), label="FFT Magnitude (dB)")
plt.xlabel("Frequency (Hz)")
plt.ylabel("Magnitude (dB)")
plt.title("FFT of Sine Wave with Random Jumps at 60-sample Boundaries")
plt.xlim(0, 5000)  # Focus on relevant range
plt.grid()
plt.legend()
plt.show()
