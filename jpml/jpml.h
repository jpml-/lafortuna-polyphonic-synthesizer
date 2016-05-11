/*
 * JPML's Polyphonic Music Library
 * Developed for La Fortuna (at90usb1286) @ 8MHz
 *
 * A polyphonic synthesizer and abc player library by jpml1g14.
 *
 * Plays music from ABC notation files by generating mono PCM audio on pins OC3A (left audio channel) and OC1A (right audio channel)
 *
 * Waveform generation is done entirely on ISR1. 
 * ISR3 is used to increment the clock used for note on/off timing. 
 * ABC file parsing turned out to be too complex for an ISR, so unfortunately abc_play() blocks until the song is complete.
 *
 * Built upon the LaFortuna WAV audio library by arp1g13:
 * 	github.com/fatcookies/lafortuna-wav-lib
 * which itself was adapted from:
 * 	http://avrpcm.blogspot.co.uk/2010/11/playing-8-bit-pcm-using-any-avr.html
 *
 * Credit also to ChaN for his Fat-FS implementation:
 * 	elm-chan.org/fsw/ff/00index_e.html
 *
 */
 
#ifndef _JPML_H
#define _JPML_H

/*dependencies*/
#include <stdint.h>
#include "ff.h"
#include "notes.h"

#define LINE_BUFFER_SIZE 1024 /*reduce this only if you can guarantee that lines in your ABC files will be less than 1024 characters in length*/

/*wave constants*/
#define SINE 0
#define TRIANGLE 1
#define SQUARE 2
#define SAWTOOTH 3
/*lookup table for 256-point sine wave*/
static uint8_t sine[256] = {
	128,131,134,137,140,143,146,149,
	152,155,158,162,165,167,170,173,
	176,179,182,185,188,190,193,196,
	198,201,203,206,208,211,213,215,
	218,220,222,224,226,228,230,232,
	234,235,237,238,240,241,243,244,
	245,246,248,249,250,250,251,252,
	253,253,254,254,254,255,255,255,
	255,255,255,255,254,254,254,253,
	253,252,251,250,250,249,248,246,
	245,244,243,241,240,238,237,235,
	234,232,230,228,226,224,222,220,
	218,215,213,211,208,206,203,201,
	198,196,193,190,188,185,182,179,
	176,173,170,167,165,162,158,155,
	152,149,146,143,140,137,134,131,
	128,124,121,118,115,112,109,106,
	103,100,97,93,90,88,85,82,
	79,76,73,70,67,65,62,59,
	57,54,52,49,47,44,42,40,
	37,35,33,31,29,27,25,23,
	21,20,18,17,15,14,12,11,
	10,9,7,6,5,5,4,3,
	2,2,1,1,1,0,0,0,
	0,0,0,0,1,1,1,2,
	2,3,4,5,5,6,7,9,
	10,11,12,14,15,17,18,20,
	21,23,25,27,29,31,33,35,
	37,40,42,44,47,49,52,54,
	57,59,62,65,67,70,73,76,
	79,82,85,88,90,93,97,100,
	103,106,109,112,115,118,121,124,
};

/*
 * SYNTHESIZER FUNCTIONS
 * use these to manually manipulate the synthesizer yourself if you don't fancy playing abc files.
 * if you only want to play abc files, you can ignore these.
 * see the abc-playing bits of jpml.c for example usage
 */
void pwm_init(); /*initialise timers 1 and 3*/
void pwm_stop(); /*undo pwm_init*/
uint8_t pwm_is_in_use(); /*0 when stopped or not initialised, non-zero otherwise*/
void channel_play(uint8_t note, uint8_t duration); /*play a note on the first available channel*/
void channel_stop(uint8_t channel); /*stop the current note*/
void channel_set_wave(uint8_t channel, uint8_t wave); /*change the wave of the given channel*/

/*
 * ABC FUNCTIONS
 * use these to manage playing abc notation files
 */
FRESULT abc_load_file(char* filename); /*load a given abc file and parse its header*/
void abc_play(); /*play the currently loaded song*/
void abc_stop(); /*stop playing a song*/
uint8_t abc_is_playing();
char* abc_song_title(); /*get the title of the currently loaded song*/

/*
 * SONG FUNCTIONS
 * use these to set a song's parameters independently of how the abc file specifies them
 */
void changeKey(char* keystring); /*set the key signature of the current song (e.g. "Eb", "C#")*/
void set_tempo(uint16_t bpm); /*set how many crotchets (1/4-notes) should be played per minute*/

#endif /* _JPML_H */
