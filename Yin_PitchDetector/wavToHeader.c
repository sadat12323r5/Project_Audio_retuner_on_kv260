/*
 * wavToHeader.c - Converts a WAV file to a C header file (audioData.h)
 * 
 * Usage: ./wavToHeader <input.wav> <start_frame> <num_samples> <output_bits>
 * Example: ./wavToHeader input.wav 7000 1500 10
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// WAV file header structure
typedef struct {
    char riff[4];              // "RIFF"
    uint32_t fileSize;         // File size - 8
    char wave[4];              // "WAVE"
    char fmt[4];               // "fmt "
    uint32_t fmtSize;          // Size of fmt chunk
    uint16_t audioFormat;      // Audio format (1 = PCM)
    uint16_t numChannels;      // Number of channels
    uint32_t sampleRate;       // Sample rate
    uint32_t byteRate;         // Byte rate
    uint16_t blockAlign;       // Block align
    uint16_t bitsPerSample;    // Bits per sample
} WavHeader;

typedef struct {
    char chunkId[4];           // "data"
    uint32_t chunkSize;        // Size of data
} DataChunk;

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <input.wav> <start_frame> <num_samples> <output_bits>\n", argv[0]);
        printf("Example: %s input.wav 7000 1500 10\n", argv[0]);
        return 1;
    }

    const char *inputFile = argv[1];
    int startFrame = atoi(argv[2]);
    int numSamples = atoi(argv[3]);
    int outputBits = atoi(argv[4]);

    // Open WAV file
    FILE *wavFile = fopen(inputFile, "rb");
    if (!wavFile) {
        printf("Error: Could not open file %s\n", inputFile);
        return 1;
    }

    // Read WAV header
    WavHeader header;
    fread(&header, sizeof(WavHeader), 1, wavFile);

    // Verify it's a valid WAV file
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        printf("Error: Not a valid WAV file\n");
        fclose(wavFile);
        return 1;
    }

    printf("WAV File Information:\n");
    printf("  Channels: %d\n", header.numChannels);
    printf("  Sample Rate: %d Hz\n", header.sampleRate);
    printf("  Bits per Sample: %d\n", header.bitsPerSample);
    printf("  Audio Format: %d (1=PCM)\n", header.audioFormat);

    // Find data chunk
    DataChunk dataChunk;
    while (fread(&dataChunk, sizeof(DataChunk), 1, wavFile) == 1) {
        if (strncmp(dataChunk.chunkId, "data", 4) == 0) {
            break;
        }
        // Skip this chunk
        fseek(wavFile, dataChunk.chunkSize, SEEK_CUR);
    }

    int totalFrames = dataChunk.chunkSize / (header.numChannels * header.bitsPerSample / 8);
    printf("  Total Frames: %d\n", totalFrames);
    printf("\nExtraction Parameters:\n");
    printf("  Start Frame: %d\n", startFrame);
    printf("  Number of Samples: %d\n", numSamples);
    printf("  Output Bits: %d\n", outputBits);

    // Validate parameters
    if (startFrame < 0 || startFrame >= totalFrames) {
        printf("Error: start_frame out of range (0-%d)\n", totalFrames - 1);
        fclose(wavFile);
        return 1;
    }

    if (startFrame + numSamples > totalFrames) {
        printf("Warning: Adjusting numSamples to fit within file\n");
        numSamples = totalFrames - startFrame;
    }

    // Allocate buffer for audio data
    int16_t *audioData = (int16_t *)malloc(numSamples * sizeof(int16_t));
    if (!audioData) {
        printf("Error: Memory allocation failed\n");
        fclose(wavFile);
        return 1;
    }

    // Seek to start position
    int bytesPerFrame = header.numChannels * header.bitsPerSample / 8;
    fseek(wavFile, startFrame * bytesPerFrame, SEEK_CUR);

    // Read audio data
    int16_t actualMin = 0, actualMax = 0;
    for (int i = 0; i < numSamples; i++) {
        int16_t sample = 0;
        
        if (header.bitsPerSample == 16) {
            // Read sample(s) - if stereo, only take first channel
            fread(&sample, sizeof(int16_t), 1, wavFile);
            
            // Skip remaining channels
            if (header.numChannels > 1) {
                fseek(wavFile, (header.numChannels - 1) * sizeof(int16_t), SEEK_CUR);
            }
        } else if (header.bitsPerSample == 8) {
            uint8_t sample8;
            fread(&sample8, sizeof(uint8_t), 1, wavFile);
            sample = (int16_t)(sample8 - 128) * 256; // Convert 8-bit to 16-bit
            
            // Skip remaining channels
            if (header.numChannels > 1) {
                fseek(wavFile, header.numChannels - 1, SEEK_CUR);
            }
        } else {
            printf("Error: Unsupported bits per sample: %d\n", header.bitsPerSample);
            free(audioData);
            fclose(wavFile);
            return 1;
        }

        audioData[i] = sample;
        
        // Track min/max
        if (sample < actualMin) actualMin = sample;
        if (sample > actualMax) actualMax = sample;
    }

    fclose(wavFile);

    printf("\nOriginal Sample Range: min=%d, max=%d\n", actualMin, actualMax);

    // Scale the samples to the desired bit depth
    int actualMaxAbs = (abs(actualMin) > actualMax) ? abs(actualMin) : actualMax;
    if (actualMaxAbs == 0) actualMaxAbs = 1; // Avoid division by zero

    int maxDesired = (1 << (outputBits - 1)); // 2^(bits-1) for signed values
    
    int16_t scaledMin = 0, scaledMax = 0;
    for (int i = 0; i < numSamples; i++) {
        float normalized = (float)audioData[i] / actualMaxAbs;
        audioData[i] = (int16_t)(normalized * maxDesired);
        
        if (audioData[i] < scaledMin) scaledMin = audioData[i];
        if (audioData[i] > scaledMax) scaledMax = audioData[i];
    }

    printf("Scaled Sample Range: min=%d, max=%d\n", scaledMin, scaledMax);

    // Write to audioData.h
    FILE *outFile = fopen("audioData.h", "w");
    if (!outFile) {
        printf("Error: Could not create audioData.h\n");
        free(audioData);
        return 1;
    }

    fprintf(outFile, "#define NUM_SAMPLES %d \n\n", numSamples);
    fprintf(outFile, "int16_t audio[%d] = {\n", numSamples);

    for (int i = 0; i < numSamples - 1; i++) {
        fprintf(outFile, "   %d,\n", audioData[i]);
    }
    fprintf(outFile, "   %d};\n", audioData[numSamples - 1]);

    fclose(outFile);
    free(audioData);

    printf("\naudioData.h generated successfully!\n");
    
    return 0;
}
