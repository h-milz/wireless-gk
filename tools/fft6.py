#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sun Jan 26 13:00:54 2025

@author: hm
"""

import numpy as np
import matplotlib.pyplot as plt

# Settable fundamental frequency (in Hz, range 82.4 to 1000)
f0 = 330  # Example: A3 note
# Enable/disable smoothing
smooth_jumps = True
jump_width = 3  # Number of samples to spread transition
jumps_per_second = 2
jump_amplitude = 0.2
max_fft_freq = 1000   # for the display only, change with f0 accordingly. 


fs = 44100  # Sampling rate (Hz)
duration = 10  # Duration in seconds
samples = duration * fs

# Time vector
t = np.arange(samples) / fs

# Harmonics & amplitude scaling
harmonics = np.arange(1, 13)  # First 12 harmonics
amplitudes = 1 / harmonics ** 1.2  # Slightly stronger than 1/n decay
phases = np.random.uniform(0, np.pi, len(harmonics))  # Random phases

# Construct waveform
guitar_wave = np.sum(
    [a * np.sin(2 * np.pi * n * f0 * t + p) for n, a, p in zip(harmonics, amplitudes, phases)],
    axis=0
)

# Apply optional envelope (plucking decay)
decay = np.exp(-t * 3)  # 3/sec decay rate (adjustable)
guitar_wave *= decay

# Normalize
guitar_wave /= np.max(np.abs(guitar_wave))

# --- Add Random Jumps at 60-Sample Boundaries ---
boundary_positions = np.arange(60, samples, 60)
num_jumps = np.random.randint(duration, jumps_per_second * duration)  # 1-2 jumps per second
jump_indices = np.random.choice(boundary_positions, num_jumps, replace=False)

# **Increase jump magnitude for visibility (±20%)**
jump_magnitudes = np.random.uniform(-jump_amplitude, jump_amplitude, num_jumps) * guitar_wave[jump_indices]

for idx, mag in zip(jump_indices, jump_magnitudes):
    if smooth_jumps and (idx + jump_width < samples):  
        transition = np.linspace(0, mag, jump_width)  # Linear smoothing
        guitar_wave[idx:idx + jump_width] += transition
    else:
        guitar_wave[idx:] += mag  # Instantaneous jump

# --- Plot Waveform ---
plt.figure(figsize=(10, 4))
plt.plot(t[:2000], guitar_wave[:2000], label="Guitar String with Jumps")
plt.xlabel("Time (s)")
plt.ylabel("Amplitude")
plt.title(f"Guitar String Signal with {'Smoothed' if smooth_jumps else 'Instant'} Jumps (f0 = {f0} Hz)")
plt.grid()
plt.legend()
plt.show()

# --- Compute FFT ---
fft_spectrum = np.fft.fft(guitar_wave)
frequencies = np.fft.fftfreq(samples, d=1/fs)
magnitude = np.abs(fft_spectrum)[:samples // 2]
frequencies = frequencies[:samples // 2]

# --- Plot FFT ---
plt.figure(figsize=(10, 5))
plt.plot(frequencies, 20 * np.log10(magnitude), label="FFT Magnitude (dB)")
plt.xlabel("Frequency (Hz)")
plt.ylabel("Magnitude (dB)")
plt.title(f"FFT of Guitar String Signal with {'Smoothed' if smooth_jumps else 'Instant'} Jumps (f0 = {f0} Hz)")
plt.xlim(0, max_fft_freq)  # Focus on pickup range
plt.grid()
plt.legend()
plt.show()
