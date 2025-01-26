#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sun Jan 26 12:42:10 2025

@author: hm
"""

import numpy as np
import matplotlib.pyplot as plt

# switch smooting on or off. 
smooting = False 

fs = 44100  # Sampling rate
duration = 3  # Duration in seconds
samples = duration * fs

# Frequencies and amplitudes
frequencies = [110, 441, 1670, 3240]
amplitudes = [1.0, 0.7, 0.4, 0.6]

# Time vector
t = np.arange(samples) / fs

# Generate mixed sine wave
sine_wave = sum(a * np.sin(2 * np.pi * f * t) for f, a in zip(frequencies, amplitudes))

# Find 60-sample boundaries
boundary_positions = np.arange(60, samples, 60)

# Select random boundaries for jumps (1-2 per second)
num_jumps = np.random.randint(duration, 2 * duration)  # 1-2 jumps per second
jump_indices = np.random.choice(boundary_positions, num_jumps, replace=False)

# Generate random jump magnitudes (±10% of local amplitude)
jump_magnitudes = np.random.uniform(-0.1, 0.1, num_jumps) * sine_wave[jump_indices]

# Apply smooth jumps using linear interpolation over 5 samples
jump_width = 5  # Number of samples to spread the transition

for idx, mag in zip(jump_indices, jump_magnitudes):
    if idx + jump_width < samples:  # Ensure we don't go out of bounds
        # Linear interpolation
        transition = np.linspace(0, mag, jump_width)
        if (smooting): 
            sine_wave[idx:idx + jump_width] += transition
        else:    
            sine_wave[idx:] += mag  # Instantaneous jump at idx

# Plot the wave (first 3000 samples for visibility)
plt.figure(figsize=(10, 4))
if (smooting):
    plt.plot(t[:3000], sine_wave[:3000], label="Mixed Sine Wave with Smoothing")
else:    
    plt.plot(t[:3000], sine_wave[:3000], label="Mixed Sine Wave without Smoothing")
plt.xlabel("Time (s)")
plt.ylabel("Amplitude")
if (smooting):
    plt.title("Mixed Sine Wave with Smoothing")
else:    
    plt.title("Mixed Sine Wave without Smoothing")
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
if (smooting):
    plt.title("FFT of Mixed Sine Wave with Smoothing")
else:
    plt.title("FFT of Mixed Sine Wave without Smoothing")
plt.xlim(0, 5000)  # Focus on relevant range
plt.ylim(-100,100)
plt.grid()
plt.legend()
plt.show()
