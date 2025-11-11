/*
 * Example: How to use Yin pitch detection in your project
 * 
 * Basic workflow:
 * 1. Include Yin.h
 * 2. Create a Yin object
 * 3. Initialize it with buffer size and threshold
 * 4. Call Yin_getPitch() with your audio buffer
 * 5. Optionally get the probability/confidence
 * 6. Free the Yin object when done
 */

#include "Yin.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Example 1: Basic pitch detection on a single audio buffer
void example_basic() {
    printf("\n=== Example 1: Basic Usage ===\n");
    
    // Simulate audio buffer (in real project, this comes from your audio input)
    int16_t audioBuffer[1024];
    // ... fill audioBuffer with real audio data ...
    
    // Create and initialize Yin object
    Yin yin;
    int16_t bufferSize = 1024;  // Should be at least 2x the period of lowest frequency
    float threshold = 0.15;      // Lower = more strict (0.05-0.20 typical range)
    
    Yin_init(&yin, bufferSize, threshold);
    
    // Detect pitch
    float pitch = Yin_getPitch(&yin, audioBuffer);
    
    if (pitch > 0) {
        float confidence = Yin_getProbability(&yin);
        printf("Detected pitch: %.2f Hz (confidence: %.1f%%)\n", 
               pitch, confidence * 100);
    } else {
        printf("No pitch detected\n");
    }
    
    // Free memory
    free(yin.yinBuffer);
}

// Example 2: Real-time pitch detection (processing audio stream)
void example_realtime_streaming() {
    printf("\n=== Example 2: Real-time Streaming ===\n");
    
    // Configuration
    int16_t bufferSize = 2048;  // Larger buffer = better low frequency detection
    float threshold = 0.10;      // Adjust based on your needs
    
    Yin yin;
    Yin_init(&yin, bufferSize, threshold);
    
    // Simulate processing multiple audio chunks
    int16_t audioChunk[2048];
    for (int frame = 0; frame < 10; frame++) {
        // In real application:
        // - Read audioChunk from microphone/audio input
        // - Or get next chunk from WAV file
        // ... read audio data into audioChunk ...
        
        float pitch = Yin_getPitch(&yin, audioChunk);
        
        if (pitch > 0) {
            printf("Frame %d: %.2f Hz (%.0f%% confidence)\n", 
                   frame, pitch, Yin_getProbability(&yin) * 100);
        }
        
        // Reset yinBuffer for next chunk
        for (int i = 0; i < yin.halfBufferSize; i++) {
            yin.yinBuffer[i] = 0;
        }
    }
    
    free(yin.yinBuffer);
}

// Example 3: Finding optimal buffer size for your audio
void example_find_buffer_size(int16_t* audio, int audioLength) {
    printf("\n=== Example 3: Finding Optimal Buffer Size ===\n");
    
    float threshold = 0.15;
    int minBufferSize = 512;
    int maxBufferSize = (audioLength < 4096) ? audioLength : 4096;
    
    // Try different buffer sizes to find first valid pitch
    for (int bufSize = minBufferSize; bufSize <= maxBufferSize; bufSize += 64) {
        Yin yin;
        Yin_init(&yin, bufSize, threshold);
        
        float pitch = Yin_getPitch(&yin, audio);
        
        if (pitch > 0) {
            printf("Minimum buffer size: %d samples\n", bufSize);
            printf("Detected pitch: %.2f Hz\n", pitch);
            printf("Confidence: %.1f%%\n", Yin_getProbability(&yin) * 100);
            free(yin.yinBuffer);
            break;
        }
        
        free(yin.yinBuffer);
    }
}

// Example 4: Musical note detection
const char* getNoteFromFrequency(float frequency) {
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", 
                           "F#", "G", "G#", "A", "A#", "B"};
    
    if (frequency <= 0) return "None";
    
    // Calculate MIDI note number
    int midiNote = (int)(12 * log2f(frequency / 440.0f) + 69);
    int noteIndex = midiNote % 12;
    int octave = (midiNote / 12) - 1;
    
    static char noteName[10];
    snprintf(noteName, sizeof(noteName), "%s%d", notes[noteIndex], octave);
    return noteName;
}

void example_musical_note_detection(int16_t* audio, int audioLength) {
    printf("\n=== Example 4: Musical Note Detection ===\n");
    
    Yin yin;
    Yin_init(&yin, 2048, 0.15);
    
    float pitch = Yin_getPitch(&yin, audio);
    
    if (pitch > 0) {
        const char* note = getNoteFromFrequency(pitch);
        float confidence = Yin_getProbability(&yin);
        
        printf("Frequency: %.2f Hz\n", pitch);
        printf("Musical Note: %s\n", note);
        printf("Confidence: %.1f%%\n", confidence * 100);
    } else {
        printf("No musical note detected\n");
    }
    
    free(yin.yinBuffer);
}

// Example 5: Integration with your KV260 Kria board project
void example_kria_integration() {
    printf("\n=== Example 5: KV260 Kria Board Integration ===\n");
    printf("In your Vitis project:\n\n");
    
    printf("1. Add to your project:\n");
    printf("   - Yin.c\n");
    printf("   - Yin.h\n");
    printf("   - Modify YIN_SAMPLING_RATE in Yin.h to match your sample rate\n\n");
    
    printf("2. In your main code:\n");
    printf("   #include \"Yin.h\"\n\n");
    
    printf("3. Initialize once:\n");
    printf("   Yin yin;\n");
    printf("   Yin_init(&yin, 2048, 0.15);\n\n");
    
    printf("4. Process audio:\n");
    printf("   int16_t audioBuffer[2048];\n");
    printf("   // Fill audioBuffer from your audio source\n");
    printf("   float pitch = Yin_getPitch(&yin, audioBuffer);\n");
    printf("   if (pitch > 0) {\n");
    printf("       xil_printf(\"Pitch: %%d Hz\\n\", (int)pitch);\n");
    printf("   }\n\n");
    
    printf("5. Important considerations:\n");
    printf("   - Buffer size should be >= 2x period of lowest frequency\n");
    printf("   - For 44.1kHz, detecting 100Hz needs ~882 samples\n");
    printf("   - Larger buffers = more CPU but better low-freq detection\n");
    printf("   - Threshold 0.10-0.15 works well for most cases\n");
}

int main() {
    printf("Yin Pitch Detection - Usage Examples\n");
    printf("=====================================\n");
    
    // Run examples
    example_basic();
    example_realtime_streaming();
    example_kria_integration();
    
    printf("\n=== Key Parameters ===\n");
    printf("bufferSize: Minimum is ~2x the period of lowest frequency you want to detect\n");
    printf("            - For 100 Hz at 44.1kHz: need >= 882 samples\n");
    printf("            - For 80 Hz at 44.1kHz: need >= 1103 samples\n");
    printf("            - Typical range: 1024-4096 samples\n\n");
    
    printf("threshold:  Controls detection sensitivity\n");
    printf("            - Lower (0.05): Very strict, fewer false positives\n");
    printf("            - Medium (0.15): Balanced (default)\n");
    printf("            - Higher (0.30): Lenient, more detections but less reliable\n");
    
    return 0;
}
