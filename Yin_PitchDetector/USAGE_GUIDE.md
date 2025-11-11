# WAV Pitch Detection - Quick Reference

## Overview
This provides a complete solution for pitch detection from WAV files using the Yin algorithm.

## Files Needed
- `Yin.c` - Yin algorithm implementation
- `Yin.h` - Yin header file
- `wav_pitch_detection.c` - Your complete pitch detection solution

## Three Ways to Use It

### 1. Command Line Tool
```bash
# Compile
gcc -o wav_pitch_detection wav_pitch_detection.c Yin.c -lm -Wall

# Simple usage
./wav_pitch_detection audio.wav

# With custom parameters
./wav_pitch_detection audio.wav 1000 2048 0.15
```

### 2. Function: detectPitchFromWav()
The main function that does everything for you:

```c
#include "wav_pitch_detection.c"  // or link the compiled object

int main() {
    PitchResult result;
    
    // Detect pitch from WAV file
    if (detectPitchFromWav("audio.wav", 1000, 2048, 0.15, &result) == 0) {
        if (result.pitch > 0) {
            printf("Pitch: %.2f Hz\n", result.pitch);
            printf("Confidence: %.1f%%\n", result.confidence * 100);
        } else {
            printf("No pitch detected\n");
        }
    }
    
    return 0;
}
```

**Parameters:**
- `filename`: Path to WAV file
- `startSample`: Which sample to start from (0 = beginning)
- `numSamples`: How many samples to analyze (buffer size)
- `threshold`: Detection sensitivity (0.05=strict, 0.15=balanced, 0.30=lenient)
- `result`: Pointer to PitchResult structure

### 3. Function: detectPitchSimple()
Simplified version with defaults:

```c
PitchResult result;
if (detectPitchSimple("audio.wav", &result) == 0) {
    printf("Pitch: %.2f Hz (%.0f%% confident)\n", 
           result.pitch, result.confidence * 100);
}
```

### 4. Function: detectPitchFromTime()
Use time instead of samples:

```c
PitchResult result;
// Analyze from 1.5 seconds, for 50ms duration
if (detectPitchFromTime("audio.wav", 1500, 50, 0.15, &result) == 0) {
    printf("Pitch: %.2f Hz\n", result.pitch);
}
```

## PitchResult Structure

```c
typedef struct {
    float pitch;        // Frequency in Hz (-1 if not detected)
    float confidence;   // 0.0 to 1.0 (confidence level)
    int sampleRate;     // Sample rate of audio
    int numSamples;     // Number of samples analyzed
    int bufferSize;     // Buffer size used
} PitchResult;
```

## For Your KV260 Kria Board

Copy these files to your Vitis project:
1. `Yin.c`
2. `Yin.h`
3. `wav_pitch_detection.c`

In your main code:

```c
#include "wav_pitch_detection.c"

int main() {
    PitchResult result;
    
    // Your WAV file from audio input
    detectPitchFromWav("/path/to/audio.wav", 0, 2048, 0.15, &result);
    
    if (result.pitch > 0) {
        xil_printf("Detected pitch: %d Hz\n", (int)result.pitch);
    }
    
    return 0;
}
```

## Key Parameters Guide

### Buffer Size (numSamples)
- **Minimum**: ~2× the period of lowest frequency
  - For 100 Hz @ 44.1kHz: need ≥ 882 samples
  - For 80 Hz @ 44.1kHz: need ≥ 1103 samples
- **Recommended**: 1024-4096 samples
- **Larger** = better low-frequency detection, more CPU

### Threshold
- **0.05**: Very strict, fewer false positives
- **0.15**: Balanced (recommended default)
- **0.30**: Lenient, catches weaker signals
- **0.50**: Very lenient, may have false positives

### Start Sample
- Skip silence at beginning of file
- Find where actual audio starts
- Test different positions if no pitch detected

## Example Results

```bash
$ ./wav_pitch_detection OpenE.wav 1000 2048 0.15

=== Results ===
Sample Rate:    44100 Hz
Samples Analyzed: 2048
Buffer Size:    2048

✓ Pitch Detected!
  Frequency:    81.91 Hz
  Confidence:   98.6%
  Musical Note: D#2
```

## Troubleshooting

**No pitch detected?**
1. Increase `threshold` (try 0.20, 0.30, 0.50)
2. Increase `numSamples` (try 4096)
3. Adjust `startSample` (skip silence)
4. Check if audio section has clear pitch

**Low confidence?**
- Audio might be noisy
- Multiple frequencies present
- Try different buffer sizes
- Adjust threshold

## Integration Tips

1. **Real-time**: Read audio chunks, call detectPitch for each chunk
2. **Batch**: Process multiple segments of a WAV file
3. **Streaming**: Save audio input to WAV, then process
4. **Embedded**: Use smaller buffers (1024) for less memory
