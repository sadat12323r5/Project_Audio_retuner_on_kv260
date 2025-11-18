/******************************************************************************
* Full Audio Capture + Playback with DMA Status Debug
******************************************************************************/

#include "xaxidma.h"
#include "xparameters.h"
#include "xil_cache.h"
#include "xstatus.h"
#include "sleep.h"
#include "ff.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DMA_DEV_ID              XPAR_AXIDMA_0_DEVICE_ID
#define FS                      48000
#define CHANNELS                1
#define MIC_BITS                18
#define OUT_BITS                16

#define BURST_SAMPLES           256
#define BYTES_PER_SAMPLE        4
#define BURST_BYTES             (BURST_SAMPLES * BYTES_PER_SAMPLE)

#define SECONDS_TO_RECORD       7
#define TOTAL_SAMPLES           (FS * SECONDS_TO_RECORD)

static XAxiDma AxiDma;
static FATFS g_fs;
static const char *DRIVE = "0:";

static uint32_t rx32[BURST_SAMPLES] __attribute__((aligned(64)));
static int16_t  pcm16[BURST_SAMPLES];

static uint32_t tx32[BURST_SAMPLES * 2] __attribute__((aligned(64)));

// ------------------ UTIL FUNCS ------------------
static inline int16_t to_pcm16(uint32_t w)
{
    return (int16_t)((int32_t)w >> (MIC_BITS - OUT_BITS));
}

static inline uint32_t i2s_word_from_pcm16(int16_t s)
{
    return (uint32_t)((int32_t)s << 16);
}

uint16_t swap_bits_u16(uint16_t word)
{
    uint16_t ret = 0;
    for (int i = 0; i < 16; i++)
        if (word & (1 << i))
            ret |= (1 << (15 - i));
    return ret;
}

// ------------------ WAV HEADER UTILS ------------------
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
    h[16]=16; h[17]=0; h[18]=0; h[19]=0;
    h[20]=1;  h[21]=0;
    h[22]=ch; h[23]=0;
    h[24]= fs        &255; h[25]=(fs>>8)&255; h[26]=(fs>>16)&255; h[27]=(fs>>24)&255;
    h[28]= byteRate  &255; h[29]=(byteRate>>8)&255; h[30]=(byteRate>>16)&255; h[31]=(byteRate>>24)&255;
    h[32]= blockAlign&255; h[33]=(blockAlign>>8)&255;
    h[34]= bits      &255; h[35]=(bits>>8)&255;
    h[36]='d';h[37]='a';h[38]='t';h[39]='a';
    h[40]= dataSize  &255; h[41]=(dataSize>>8)&255; h[42]=(dataSize>>16)&255; h[43]=(dataSize>>24)&255;
}

static int sd_open_wav(FIL *fp, const char *filename,
                       uint32_t nsamples, uint32_t fs,
                       uint16_t bits, uint16_t ch)
{
    FRESULT fr;
    UINT bw;
    uint8_t hdr[44];
    char path[64];

    f_mount(NULL, DRIVE, 1);
    fr = f_mount(&g_fs, DRIVE, 1);
    if (fr != FR_OK) return -1;

    snprintf(path, sizeof(path), "%s/%s", DRIVE, filename);
    fr = f_open(fp, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return -1;

    wav_header(hdr, nsamples, fs, bits, ch);
    fr = f_write(fp, hdr, sizeof(hdr), &bw);
    if (fr != FR_OK || bw != sizeof(hdr)) { f_close(fp); return -1; }

    return 0;
}

static void sd_fix_header(FIL *fp, uint32_t nsamples, uint32_t fs,
                          uint16_t bits, uint16_t ch)
{
    UINT bw; uint8_t hdr[44];
    wav_header(hdr, nsamples, fs, bits, ch);
    f_lseek(fp, 0);
    f_write(fp, hdr, sizeof(hdr), &bw);
}

// ---------------------------------------------------------
//                       MAIN
// ---------------------------------------------------------
int main(void)
{
    xil_printf("Audio capture start...\r\n");

    XAxiDma_Config *Cfg = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!Cfg) return XST_FAILURE;
    if (XAxiDma_CfgInitialize(&AxiDma, Cfg) != XST_SUCCESS) return XST_FAILURE;
    if (XAxiDma_HasSg(&AxiDma)) return XST_FAILURE;

    FIL f; UINT bw;
    if (sd_open_wav(&f, "rec.wav", TOTAL_SAMPLES, FS, OUT_BITS, CHANNELS) != 0)
        return XST_FAILURE;

    xil_printf("Recording %d s @ %d Hz...\r\n", SECONDS_TO_RECORD, FS);

    uint32_t samples_written = 0;
    while (samples_written < TOTAL_SAMPLES)
    {
        Xil_DCacheFlushRange((UINTPTR)rx32, BURST_BYTES);
        XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)rx32, BURST_BYTES,
                               XAXIDMA_DEVICE_TO_DMA);

        while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA));

        Xil_DCacheInvalidateRange((UINTPTR)rx32, BURST_BYTES);

        for (int i = 0; i < BURST_SAMPLES; i++)
            pcm16[i] = swap_bits_u16(to_pcm16(rx32[i]));

        uint32_t chunk = BURST_SAMPLES;
        if (samples_written + chunk > TOTAL_SAMPLES)
            chunk = TOTAL_SAMPLES - samples_written;

        f_write(&f, pcm16, chunk * sizeof(int16_t), &bw);
        samples_written += chunk;
    }

    sd_fix_header(&f, samples_written, FS, OUT_BITS, CHANNELS);
    f_close(&f);
    xil_printf("Saved %s/rec.wav\n", DRIVE);


    // ===========================================================
    //                         PLAYBACK
    // ===========================================================

    xil_printf("\n======== PLAYBACK ========\n");

    // HARD RESET DMA BEFORE PLAYBACK
    xil_printf("Resetting DMA...\n");
    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma));

    // Re-init
    Cfg = XAxiDma_LookupConfig(DMA_DEV_ID);
    XAxiDma_CfgInitialize(&AxiDma, Cfg);

    // Clear MM2S + S2MM status registers
    XAxiDma_WriteReg(AxiDma.RegBase, XAXIDMA_TX_OFFSET + XAXIDMA_SR_OFFSET, 0xFFFFFFFF);
    XAxiDma_WriteReg(AxiDma.RegBase, XAXIDMA_RX_OFFSET + XAXIDMA_SR_OFFSET, 0xFFFFFFFF);


    FIL fplay;
    UINT br;
    if (f_open(&fplay, "0:/rec.wav", FA_READ) != FR_OK)
    {
        xil_printf("Failed to reopen WAV!\n");
        return XST_FAILURE;
    }

    f_lseek(&fplay, 44);
    xil_printf("Playback starting...\n");

    u32 first_sr_before = 0;
    u32 first_sr_after  = 0;
    int first_status    = 0;
    int first_taken     = 0;   // boolean flag

    while (1)
    {
    	if (f_read(&fplay, pcm16, BURST_SAMPLES * sizeof(int16_t), &br) != FR_OK ||
    	        br == 0) break;

    	    int samples = br / (int)sizeof(int16_t);

    	    for (int i = 0; i < samples; i++)
    	    {
    	        uint32_t w = i2s_word_from_pcm16(pcm16[i]);
    	        tx32[2*i]   = w;
    	        tx32[2*i+1] = w;
    	    }

    	    int tx_words = samples * 2;

    	    Xil_DCacheFlushRange((UINTPTR)tx32, tx_words * sizeof(uint32_t));

    	    int status = XAxiDma_SimpleTransfer(&AxiDma,
    	                                        (UINTPTR)tx32,
    	                                        tx_words * sizeof(uint32_t),
    	                                        XAXIDMA_DMA_TO_DEVICE);

    	    /* Only record the FIRST transfer's SR + status */
    	    if (!first_taken) {
    	        first_sr_before = XAxiDma_ReadReg(AxiDma.RegBase,
    	                                          XAXIDMA_TX_OFFSET + XAXIDMA_SR_OFFSET);
    	        first_status    = status;

    	        while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE))
    	            ;

    	        first_sr_after  = XAxiDma_ReadReg(AxiDma.RegBase,
    	                                          XAXIDMA_TX_OFFSET + XAXIDMA_SR_OFFSET);
    	        first_taken = 1;
    	    } else {
    	        while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE))
    	            ;
    	    }

    	    if (status != XST_SUCCESS) {
    	        xil_printf("TX DMA transfer setup failed (%d)\r\n", status);
    	        break;
    	    }
    }
    xil_printf("\r\n--- FIRST MM2S DEBUG ---\r\n");
    xil_printf("MM2S SR first before: 0x%08lx\r\n", (unsigned long)first_sr_before);
    xil_printf("MM2S SR first after : 0x%08lx (status=%d)\r\n",
               (unsigned long)first_sr_after, first_status);
    f_close(&fplay);
    f_mount(NULL, DRIVE, 1);

    xil_printf("Playback done.\n");
    return 0;
}


