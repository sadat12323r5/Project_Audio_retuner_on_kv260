/******************************************************************************
* Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */
/******************************************************************************
* Copyright (C) 2023 Advanced Micro Devices, Inc.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*
 * Simple audio capture to WAV on SD1 using FatFs and AXI DMA (S2MM).
 * - Uses drive "1:" explicitly
 * - Writes 16-bit PCM WAV
 */

#include "xaxidma.h"
#include "xparameters.h"
#include "xil_cache.h"
#include "xstatus.h"
#include "sleep.h"
#include "ff.h"
#include "Yin.h"
#include "phase_voc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*** DMA device ***/
#define DMA_DEV_ID              XPAR_AXIDMA_0_DEVICE_ID
      
/*** AXI GPIO for LED and Switch ***/
#define AXI_GPIO_LED_OFFSET     0x00  // LED channel offset
#define AXI_GPIO_SW_OFFSET      0x08  // Switch channel offset

/*** Audio / file format ***/
#define FS                      48000      // sample rate (Hz) - MUST match your I2S hardware config
#define CHANNELS                1          // mono
#define MIC_BITS                18         // useful MSBs from the I2S mic
#define OUT_BITS                16         // write 16-bit PCM in the WAV

/*** Capture sizing ***/
#define BURST_SAMPLES           256        // per DMA transfer
#define BYTES_PER_SAMPLE        4          // PL streams 32-bit words
#define BURST_BYTES             (BURST_SAMPLES * BYTES_PER_SAMPLE)

#define SECONDS_TO_RECORD       3  // Reduced from 7 to save memory for phase vocoder
#define TOTAL_SAMPLES           (FS * SECONDS_TO_RECORD)

/*** Globals ***/
static XAxiDma AxiDma;
static uint32_t rx32[BURST_SAMPLES] __attribute__((aligned(64)));
static int16_t  pcm16[BURST_SAMPLES];

/*** FatFs globals (must persist while mounted) ***/
static FATFS g_fs;
static const char *DRIVE = "0:";   // <-- SD1

// Pitch detection result structure
typedef struct {
    float pitch;               // Detected pitch in Hz (-1 if not detected)
    float confidence;          // Confidence level (0.0 to 1.0)
    int sampleRate;            // Sample rate of the audio
    int numSamples;            // Number of samples analyzed
    int bufferSize;            // Buffer size used for analysis
    int actualStartSample;     // Actual start sample used
} PitchResult;

/*** Utilities ***/
static inline int16_t to_pcm16(uint32_t w)
{
    // Treat as signed in 32-bit, keep top OUT_BITS bits.
    return (int16_t)((int32_t)w >> (MIC_BITS - OUT_BITS));
}

/*** 44-byte WAV header ***/
static void wav_header(uint8_t *h, uint32_t nsamples, uint32_t fs,
                       uint16_t bits, uint16_t ch)
{
    uint32_t byteRate   = fs * ch * (bits/8);
    uint16_t blockAlign = ch * (bits/8);
    uint32_t dataSize   = nsamples * blockAlign;
    uint32_t riffSize   = 36 + dataSize;

    h[0]='R';h[1]='I';h[2]='F';h[3]='F';
    h[4]= riffSize     &255; h[5]=(riffSize>>8)&255; h[6]=(riffSize>>16)&255; h[7]=(riffSize>>24)&255;
    h[8]='W';h[9]='A';h[10]='V';h[11]='E';
    h[12]='f';h[13]='m';h[14]='t';h[15]=' ';
    h[16]=16; h[17]=0; h[18]=0; h[19]=0;          // fmt chunk size
    h[20]=1;  h[21]=0;                            // PCM
    h[22]=ch; h[23]=0;                            // channels
    h[24]= fs        &255; h[25]=(fs>>8)&255; h[26]=(fs>>16)&255; h[27]=(fs>>24)&255;
    h[28]= byteRate  &255; h[29]=(byteRate>>8)&255; h[30]=(byteRate>>16)&255; h[31]=(byteRate>>24)&255;
    h[32]= blockAlign&255; h[33]=(blockAlign>>8)&255;
    h[34]= bits      &255; h[35]=(bits>>8)&255;
    h[36]='d';h[37]='a';h[38]='t';h[39]='a';
    h[40]= dataSize  &255; h[41]=(dataSize>>8)&255; h[42]=(dataSize>>16)&255; h[43]=(dataSize>>24)&255;
}

/*** Mount SD1 into g_fs ***/
//static FRESULT sd_mount_sd1(void)
//{
//    // Be clean: unmount first (in case of prior run)
//    f_mount(NULL, DRIVE, 1);
//    return f_mount(&g_fs, DRIVE, 1);
//}

/*** Open WAV on SD1 and write placeholder header ***/
static int sd_open_wav(FIL *fp, const char *filename,
                       uint32_t nsamples, uint32_t fs,
                       uint16_t bits, uint16_t ch)
{
    FRESULT fr;
    UINT bw;
    uint8_t hdr[44];
    char path[64];

    xil_printf("unmount %s...\r\n", DRIVE);
    f_mount(NULL, DRIVE, 1);

    xil_printf("mount %s...\r\n", DRIVE);
    fr = f_mount(&g_fs, DRIVE, 1);
    xil_printf("f_mount -> %d\r\n", fr);
    if (fr != FR_OK) return -1;

    snprintf(path, sizeof(path), "%s/%s", DRIVE, filename);
    xil_printf("f_open %s...\r\n", path);
    fr = f_open(fp, path, FA_CREATE_ALWAYS | FA_WRITE);
    xil_printf("f_open -> %d\r\n", fr);
    if (fr != FR_OK) return -1;

    wav_header(hdr, nsamples, fs, bits, ch);
    xil_printf("write header 44 bytes...\r\n");
    fr = f_write(fp, hdr, sizeof(hdr), &bw);
    xil_printf("f_write hdr -> fr=%d bw=%u\r\n", fr, (unsigned)bw);
    if (fr != FR_OK || bw != sizeof(hdr)) { f_close(fp); return -1; }

    xil_printf("header ok\r\n");
    return 0;
}

/*** Patch the header with the true sample count ***/
static void sd_fix_header(FIL *fp, uint32_t nsamples, uint32_t fs,
                          uint16_t bits, uint16_t ch)
{
    UINT bw; uint8_t hdr[44];
    wav_header(hdr, nsamples, fs, bits, ch);
    f_lseek(fp, 0);
    f_write(fp, hdr, sizeof(hdr), &bw);
}

/*** Reverses the order of all the bits of an unsigned 16-bit value ***/
uint16_t swap_bits_u16(uint16_t word) {
	uint16_t ret = 0;
	for (int i = 0; i < sizeof(uint16_t) * 8; i++) {
		if ((0b1 << i) & word) {
			ret |= 1 << (sizeof(uint16_t) * 8 - 1 - i);
		}
	}
	return ret;
}

///*** Swaps the endian-ness of an unsigned 16-bit value ***/
// uint16_t swap_endian_u16(uint16_t word) {
// 	return ((word & 0xFF) << 8)  |
// 			((word & 0xFF00) >> 8);
// }

/*** Detect pitch from the saved WAV file on SD card ***/
static int detect_pitch_from_sd(const char *filename, int startSample, int numSamples, float threshold, PitchResult* result)
{
    FRESULT fr;
    FIL fp;
    UINT br;
    char path[64];
    
    // Initialize result
    result->pitch = -1;
    result->confidence = 0;
    result->sampleRate = 0;
    result->numSamples = 0;
    result->bufferSize = 0;
    result->actualStartSample = startSample;
    
    // Open the file from SD card
    snprintf(path, sizeof(path), "%s/%s", DRIVE, filename);
    xil_printf("Opening %s for pitch detection...\r\n", path);
    
    fr = f_open(&fp, path, FA_READ);
    if (fr != FR_OK) {
        xil_printf("Failed to open file for reading: %d\r\n", fr);
        return -1;
    }
    
    // Read WAV header (44 bytes)
    uint8_t hdr[44];
    fr = f_read(&fp, hdr, 44, &br);
    if (fr != FR_OK || br != 44) {
        xil_printf("Failed to read WAV header\r\n");
        f_close(&fp);
        return -1;
    }
    
    // Extract sample rate from header
    result->sampleRate = hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | (hdr[27] << 24);
    xil_printf("Sample rate: %d Hz\r\n", result->sampleRate);
    
    // Auto-determine optimal buffer size if numSamples is 0
    if (numSamples == 0) {
        numSamples = 2048;  // Default buffer size
    }
    
    result->numSamples = numSamples;
    result->bufferSize = numSamples;
    
    // Allocate audio buffer
    int16_t* audioBuffer = (int16_t*)malloc(numSamples * sizeof(int16_t));
    if (!audioBuffer) {
        xil_printf("Memory allocation failed\r\n");
        f_close(&fp);
        return -1;
    }
    
    // Seek to start position (skip header + startSample * 2 bytes)
    f_lseek(&fp, 44 + (startSample * sizeof(int16_t)));
    
    // Read audio data into buffer
    fr = f_read(&fp, audioBuffer, numSamples * sizeof(int16_t), &br);
    if (fr != FR_OK) {
        xil_printf("Failed to read audio data\r\n");
        free(audioBuffer);
        f_close(&fp);
        return -1;
    }
    
    f_close(&fp);
    
    xil_printf("Read %u samples, analyzing pitch...\r\n", br / sizeof(int16_t));
    
    // Initialize Yin for pitch detection
    xil_printf("Initializing Yin algorithm...\r\n");
    Yin yin;
    Yin_init(&yin, numSamples, threshold);
    xil_printf("Yin initialized, detecting pitch...\r\n");
    
    // Detect pitch
    float pitch = Yin_getPitch(&yin, audioBuffer);
    xil_printf("Pitch detection complete: %.2f Hz\r\n", pitch);
    
    result->pitch = pitch;
    result->confidence = Yin_getProbability(&yin);
    
    // Cleanup
    xil_printf("Cleaning up...\r\n");
    free(yin.yinBuffer);
    free(audioBuffer);
    
    return 0;
}

/*** Load WAV file from SD card using FatFs ***/
static AudioBuffer* load_wav_from_sd(const char *filename)
{
    FRESULT fr;
    FIL fp;
    UINT br;
    char path[64];
    uint8_t hdr[44];
    
    xil_printf("Loading WAV file: %s\r\n", filename);
    
    // Open file
    snprintf(path, sizeof(path), "%s/%s", DRIVE, filename);
    fr = f_open(&fp, path, FA_READ);
    if (fr != FR_OK) {
        xil_printf("Failed to open WAV file: %d\r\n", fr);
        return NULL;
    }
    
    // Read WAV header
    fr = f_read(&fp, hdr, 44, &br);
    if (fr != FR_OK || br != 44) {
        xil_printf("Failed to read WAV header\r\n");
        f_close(&fp);
        return NULL;
    }
    
    // Verify WAV format
    if (hdr[0] != 'R' || hdr[1] != 'I' || hdr[2] != 'F' || hdr[3] != 'F' ||
        hdr[8] != 'W' || hdr[9] != 'A' || hdr[10] != 'V' || hdr[11] != 'E') {
        xil_printf("Not a valid WAV file\r\n");
        f_close(&fp);
        return NULL;
    }
    
    // Extract audio parameters
    uint32_t sample_rate = hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | (hdr[27] << 24);
    uint16_t bits_per_sample = hdr[34] | (hdr[35] << 8);
    uint32_t data_size = hdr[40] | (hdr[41] << 8) | (hdr[42] << 16) | (hdr[43] << 24);
    
    if (bits_per_sample != 16) {
        xil_printf("Only 16-bit audio supported\r\n");
        f_close(&fp);
        return NULL;
    }
    
    uint32_t num_samples = data_size / 2;  // 16-bit samples
    
    xil_printf("WAV info: %lu samples, %lu Hz, 16-bit\r\n", 
               (unsigned long)num_samples, (unsigned long)sample_rate);
    
    // Allocate AudioBuffer
    AudioBuffer* audio = (AudioBuffer*)malloc(sizeof(AudioBuffer));
    if (!audio) {
        xil_printf("Failed to allocate AudioBuffer\r\n");
        f_close(&fp);
        return NULL;
    }
    
    audio->data = (float*)malloc(num_samples * sizeof(float));
    if (!audio->data) {
        xil_printf("Failed to allocate audio data\r\n");
        free(audio);
        f_close(&fp);
        return NULL;
    }
    
    audio->length = num_samples;
    audio->sample_rate = sample_rate;
    
    // Read audio data in chunks and convert to float
    #define LOAD_CHUNK_SIZE 1024
    int16_t chunk_buffer[LOAD_CHUNK_SIZE];
    uint32_t samples_read = 0;
    
    while (samples_read < num_samples) {
        uint32_t samples_to_read = num_samples - samples_read;
        if (samples_to_read > LOAD_CHUNK_SIZE) {
            samples_to_read = LOAD_CHUNK_SIZE;
        }
        
        fr = f_read(&fp, chunk_buffer, samples_to_read * sizeof(int16_t), &br);
        if (fr != FR_OK) {
            xil_printf("Failed to read audio data\r\n");
            free(audio->data);
            free(audio);
            f_close(&fp);
            return NULL;
        }
        
        // Convert int16 to float
        for (uint32_t i = 0; i < br / sizeof(int16_t); i++) {
            audio->data[samples_read + i] = (float)chunk_buffer[i] / 32768.0f;
        }
        
        samples_read += br / sizeof(int16_t);
    }
    
    f_close(&fp);
    xil_printf("Successfully loaded %lu samples\r\n", (unsigned long)samples_read);
    
    return audio;
}

/*** Save AudioBuffer to WAV file on SD card using FatFs ***/
static int save_wav_to_sd(const char *filename, AudioBuffer* audio)
{
    FRESULT fr;
    FIL fp;
    UINT bw;
    uint8_t hdr[44];
    char path[64];
    
    xil_printf("Saving WAV file: %s\r\n", filename);
    
    // Create WAV header
    wav_header(hdr, audio->length, audio->sample_rate, 16, 1);
    
    // Open file directly (filesystem already mounted)
    // Use 8.3 filename format for FatFs compatibility
    snprintf(path, sizeof(path), "%s/%s", DRIVE, filename);
    fr = f_open(&fp, path, FA_WRITE | FA_CREATE_ALWAYS);
    
    if (fr != FR_OK) {
        xil_printf("Failed to create output WAV file (error %d)\r\n", fr);
        return -1;
    }
    
    // Write header
    xil_printf("Writing WAV header...\r\n");
    fr = f_write(&fp, hdr, sizeof(hdr), &bw);
    if (fr != FR_OK || bw != sizeof(hdr)) {
        xil_printf("Failed to write header: fr=%d bw=%u\r\n", fr, (unsigned)bw);
        f_close(&fp);
        return -1;
    }
    
    xil_printf("Header written, writing audio data...\r\n");
    
    // Write audio data in chunks (file is already open from sd_open_wav)
    #define SAVE_CHUNK_SIZE 1024
    int16_t chunk_buffer[SAVE_CHUNK_SIZE];
    uint32_t samples_written = 0;
    
    while (samples_written < audio->length) {
        uint32_t samples_to_write = audio->length - samples_written;
        if (samples_to_write > SAVE_CHUNK_SIZE) {
            samples_to_write = SAVE_CHUNK_SIZE;
        }
        
        // Convert float to int16
        for (uint32_t i = 0; i < samples_to_write; i++) {
            float sample = audio->data[samples_written + i];
            // Clamp to [-1, 1] and convert to int16
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            chunk_buffer[i] = (int16_t)(sample * 32767.0f);
        }
        
        fr = f_write(&fp, chunk_buffer, samples_to_write * sizeof(int16_t), &bw);
        if (fr != FR_OK || bw != samples_to_write * sizeof(int16_t)) {
            xil_printf("f_write failed: fr=%d (bw=%u vs %u)\r\n",
                       fr, (unsigned)bw, (unsigned)(samples_to_write * sizeof(int16_t)));
            f_close(&fp);
            return -1;
        }
        
        samples_written += samples_to_write;
        
        if (samples_written % 10240 == 0) {
            xil_printf("  Written %lu / %lu samples\r\n", 
                       (unsigned long)samples_written, (unsigned long)audio->length);
        }
    }
    
    // Close file
    f_close(&fp);
    xil_printf("Successfully saved %lu samples to %s/%s\r\n", 
               (unsigned long)samples_written, DRIVE, filename);
    
    return 0;
}

int main(void)
{
    xil_printf("\r\n=== Audio Tuner - Interactive Mode ===\r\n");
    xil_printf("State 0: LED OFF - Ready\r\n");
    xil_printf("State 1: LED ON - Press SW1 to start recording\r\n");
    xil_printf("State 2: LED OFF - Recording\r\n");
    xil_printf("State 3: LED SLOW BLINK - Pitch detection\r\n");
    xil_printf("State 4: LED MEDIUM BLINK - Phase vocoder processing\r\n");
    xil_printf("State 5: LED DOUBLE BLINK - Complete!\r\n\r\n");

    int state = 0;
    int prev_sw = 0;  // For edge detection
    u32 samples_written = 0;
    FIL f;
    UINT bw;
    int status;
    
    // -------- Init DMA (simple mode) --------
    XAxiDma_Config *Cfg = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!Cfg) {
        xil_printf("No DMA config found.\r\n");
        return XST_FAILURE;
    }
    status = XAxiDma_CfgInitialize(&AxiDma, Cfg);
    if (status != XST_SUCCESS) {
        xil_printf("DMA init failed (%d).\r\n", status);
        return status;
    }
    if (XAxiDma_HasSg(&AxiDma)) {
        xil_printf("Scatter-Gather DMA detected; expecting Simple mode.\r\n");
        return XST_FAILURE;
    }

    xil_printf("System initialized. Press SW1 to advance states...\r\n");
    
    // Main state machine loop
    while(1) {
        u32 sw = (Xil_In32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_SW_OFFSET) & 0x01);
        
        // Detect button press (rising edge)
        if (sw == 1 && prev_sw == 0) {
            state++;
            xil_printf("\r\n>>> Button pressed! State: %d\r\n\r\n", state);
            usleep(200000);  // Debounce delay
        }
        prev_sw = sw;
        
        // State 0: Ready (LED OFF)
        if (state == 0) {
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 0);
        }
        // State 1: Waiting to start recording (LED ON)
        else if (state == 1) {
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 1);
        }
        // State 2: Recording (LED OFF)
        else if (state == 2) {
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 0);
            xil_printf("Opening %s/rec.wav ...\r\n", DRIVE);
            if (sd_open_wav(&f, "rec.wav", TOTAL_SAMPLES, FS, OUT_BITS, CHANNELS) != 0) {
                xil_printf("Failed to open WAV on %s\r\n", DRIVE);
                return XST_FAILURE;
            }
            
            xil_printf("*** RECORDING %d seconds @ %d Hz ***\r\n", SECONDS_TO_RECORD, FS);
            samples_written = 0;
            
            // Recording loop
            while (samples_written < TOTAL_SAMPLES) {
                // 1) kick S2MM for one burst
                Xil_DCacheFlushRange((UINTPTR)rx32, BURST_BYTES);
                status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)rx32, BURST_BYTES, XAXIDMA_DEVICE_TO_DMA);
                if (status != XST_SUCCESS) {
                    xil_printf("DMA transfer setup failed.\r\n");
                    break;
                }

                // 2) wait for completion
                while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) { /* spin */ }

                // 3) see fresh data
                Xil_DCacheInvalidateRange((UINTPTR)rx32, BURST_BYTES);

                // 4) convert to 16-bit PCM (and swap endianess)
                for (int i = 0; i < BURST_SAMPLES; ++i) {
                    pcm16[i] = swap_bits_u16(to_pcm16(rx32[i]));
                }

                // 5) write to SD (respect final partial chunk)
                uint32_t chunk = BURST_SAMPLES;
                if (samples_written + chunk > TOTAL_SAMPLES)
                    chunk = TOTAL_SAMPLES - samples_written;

                FRESULT fr = f_write(&f, pcm16, chunk * sizeof(int16_t), &bw);
                if (fr != FR_OK || bw != chunk * sizeof(int16_t)) {
                    xil_printf("f_write short fr=%d (bw=%u vs %u)\r\n",
                               fr, (unsigned)bw, (unsigned)(chunk*sizeof(int16_t)));
                    break;
                }

                samples_written += chunk;
            }
            
            // Patch header and close
            sd_fix_header(&f, samples_written, FS, OUT_BITS, CHANNELS);
            f_close(&f);
            xil_printf("Saved %s/rec.wav (%lu samples).\r\n", DRIVE, (unsigned long)samples_written);
            
            // Auto-advance to next state
            state++;
        }
        // State 3: Pitch detection (LED slow blink)
        else if (state == 3) {
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 1);
            usleep(500000);  // 500ms on
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 0);
            usleep(500000);  // 500ms off
            
            // Run pitch detection once
            static int pitch_done = 0;
            if (!pitch_done) {
                xil_printf("\r\n=== Starting Pitch Detection ===\r\n");
                PitchResult result;
                int startSample = 22050;
                int numSamples = 1024;
                float threshold = 0.15;
                
                if (detect_pitch_from_sd("rec.wav", startSample, numSamples, threshold, &result) == 0) {
                    xil_printf("\n=== Pitch Detection Results ===\r\n");
                    xil_printf("Sample Rate:      %d Hz\r\n", result.sampleRate);
                    xil_printf("Start Sample:     %d\r\n", result.actualStartSample);
                    xil_printf("Samples Analyzed: %d\r\n", result.numSamples);
                    
                    if (result.pitch > 0) {
                        xil_printf("\nPitch Detected!\r\n");
                        int freq_int = (int)result.pitch;
                        int freq_dec = (int)((result.pitch - freq_int) * 100);
                        xil_printf("  Frequency:   %d.%02d Hz\r\n", freq_int, freq_dec);
                        
                        int conf_int = (int)(result.confidence * 100);
                        xil_printf("  Confidence:  %d%%\r\n", conf_int);
                        
                        // Calculate musical note
                        if (result.pitch > 20 && result.pitch < 4200) {
                            const char* notes[] = {"C", "C#", "D", "D#", "E", "F",
                                                   "F#", "G", "G#", "A", "A#", "B"};
                            float log2_val = logf(result.pitch / 440.0f) / logf(2.0f);
                            int midiNote = (int)(12 * log2_val + 69);
                            int noteIndex = midiNote % 12;
                            int octave = (midiNote / 12) - 1;
                            xil_printf("  Musical Note: %s%d\r\n", notes[noteIndex], octave);
                        }
                    } else {
                        xil_printf("\nNo pitch detected.\r\n");
                    }
                } else {
                    xil_printf("Error: Failed to detect pitch from WAV file\r\n");
                }
                pitch_done = 1;
                state++;  // Auto-advance
            }
        }
        // State 4: Phase vocoder (LED medium blink)
        else if (state == 4) {
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 1);
            usleep(50000);  // 50ms on
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 0);
            usleep(50000);  // 50ms off
            
            // Run phase vocoder once
            static int vocoder_done = 0;
            if (!vocoder_done) {
                xil_printf("\r\n=== Starting Phase Vocoder Pitch Shift ===\r\n");
                
                float pitch_shift_ratio = 1.2f;
                AudioBuffer* input_audio = load_wav_from_sd("rec.wav");
                
                if (!input_audio) {
                    xil_printf("Failed to load input audio file\r\n");
                } else {
                    xil_printf("Loaded audio: %d samples at %d Hz\r\n", 
                               input_audio->length, input_audio->sample_rate);
                    
                    xil_printf("Applying phase vocoder pitch shift (ratio: 1.20)...\r\n");
                    AudioBuffer* output_audio = phase_vocoder_pitch_shift(input_audio, pitch_shift_ratio);
                    
                    if (!output_audio) {
                        xil_printf("Phase vocoder processing failed (out of memory)\r\n");
                    } else {
                        xil_printf("Phase vocoder complete: %d output samples\r\n", output_audio->length);
                        
                        xil_printf("Saving pitch-shifted audio...\r\n");
                        if (save_wav_to_sd("shifted.wav", output_audio) == 0) {
                            xil_printf("Successfully saved pitch-shifted audio as 0:/shifted.wav!\r\n");
                        } else {
                            xil_printf("Failed to save pitch-shifted audio\r\n");
                        }
                        
                        free_audio_buffer(output_audio);
                    }
                    
                    free_audio_buffer(input_audio);
                }
                vocoder_done = 1;
                state++;  // Auto-advance
            }
        }
        // State 5: Complete! (LED double blink then solid)
        else if (state == 5) {
            // Double blink
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 1);
            usleep(200000);
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 0);
            usleep(200000);
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 1);
            usleep(200000);
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 0);
            usleep(1000000);  // 1 second pause
            
            // Stay with LED on solid
            Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + AXI_GPIO_LED_OFFSET, 1);
            
            // Print completion message once
            static int done_printed = 0;
            if (!done_printed) {
                xil_printf("\r\n*** PROCESSING COMPLETE! ***\r\n");
                xil_printf("Files generated:\r\n");
                xil_printf("  - 0:/rec.wav (original recording)\r\n");
                xil_printf("  - 0:/shifted.wav (pitch shifted 1.2x)\r\n");
                xil_printf("\r\nPress SW1 to reset and record again...\r\n");
                done_printed = 1;
            }
        }
        // State 6+: Reset to state 0
        else if (state >= 6) {
            // Unmount and remount filesystem for clean state
            f_mount(NULL, DRIVE, 1);
            state = 0;
            xil_printf("\r\n=== System Reset ===\r\n");
        }
    }  // End of while(1) state machine loop
    
    return 0;
}

