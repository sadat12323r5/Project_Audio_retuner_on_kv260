# Makefile to build Test_Yin program
# --- macros
CC=gcc
CFLAGS=  -g -Wall
OBJECTS= Test_Yin.o Yin.o
WAV_OBJECTS= wav_pitch_detection.o Yin.o

# --- targets
all:    Test_Yin wavToHeader wav_pitch_detection

Test_Yin:   $(OBJECTS) 
	$(CC) -o Test_Yin  $(OBJECTS) $(LIBS)

wavToHeader: wavToHeader.c
	$(CC) $(CFLAGS) -o wavToHeader wavToHeader.c -lm

wav_pitch_detection: $(WAV_OBJECTS)
	$(CC) -o wav_pitch_detection $(WAV_OBJECTS) -lm
        
wav_pitch_detection.o: wav_pitch_detection.c Yin.h
	$(CC) $(CFLAGS) -c wav_pitch_detection.c

Test_Yin.o: Test_Yin.c Yin.h
	$(CC) $(CFLAGS) -c Test_Yin.c
       
Yin.o: Yin.c Yin.h
	$(CC) $(CFLAGS) -c Yin.c 


# --- remove binary and executable files
clean:
	rm -f Test_Yin wavToHeader wav_pitch_detection $(OBJECTS) $(WAV_OBJECTS)