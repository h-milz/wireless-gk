#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sun Jan 26 12:59:21 2025

@author: hm
"""

import numpy as np
import matplotlib.pyplot as plt

fs = 44100  # Sampling rate (Hz)
duration = 2  # Duration in seconds
samples = duration * fs

# Settable fundamental frequency (in Hz, range 82.4 to 1000)
f0 = 220  # Example: A3 note

# Time vector
t = np.arange(samples) / fs

# Harmonics & amplitude scaling (empirical model)
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

# Plot waveform (first 20 ms)
plt.figure(figsize=(10, 4))
plt.plot(t[:2000], guitar_wave[:2000])
plt.xlabel("Time (s)")
plt.ylabel("Amplitude")
plt.title(f"Guitar String Signal (f0 = {f0} Hz)")
plt.grid()
plt.show()

# Compute FFT
fft_spectrum = np.fft.fft(guitar_wave)
frequencies = np.fft.fftfreq(samples, d=1/fs)
magnitude = np.abs(fft_spectrum)[:samples // 2]
frequencies = frequencies[:samples // 2]

# Plot FFT
plt.figure(figsize=(10, 5))
plt.plot(frequencies, 20 * np.log10(magnitude))
plt.xlabel("Frequency (Hz)")
plt.ylabel("Magnitude (dB)")
plt.title(f"FFT of Guitar String Signal (f0 = {f0} Hz)")
plt.xlim(0, 10000)  # Focus on pickup range
plt.grid()
plt.show()
