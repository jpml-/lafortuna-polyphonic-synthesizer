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

#include "jpml.h"
#include <stdint.h>
#include <avr/interrupt.h>
#include <string.h>

/*
 * INTERNAL METHODS
 * method stubs omitted from jpml.h because there's no reason for them to be in the external API
 */
uint32_t calculate_tempo_32nd(uint16_t bpm);
uint8_t playNoteIfAvailable();
void parse_lf_tag(char* tagstring);
uint16_t string_to_note_length(char* str);

/* from experimentation, the la fortuna can handle a maximum of 3 sound channels reliably */
#define CHANNELS 3 
/* WARNING: the CHANNELS constant is only defined for readability purposes. 
   changing this number will mess up other parts of the code 
   (e.g. the line of code that obtains the lowest free channel is O(1) but only valid under the assumption that there are exactly 3 channels) */

struct Channel{
	uint8_t note; /*either the index of the note in notes.h/note_step, or 255 if the note is off because 0xFF looks like the word OFF*/
	uint8_t wave; /*either SINE, TRIANGLE, SQUARE or SAWTOOTH (0-3 respectively)*/
	uint16_t time_until_release; /*decremented at each tick of the sequencer; when it hits 0, the note stops playing*/
	uint16_t tick; /*current x-position of the wave (loops from 0 to 512)*/
} channels[CHANNELS];


/* wave variables */
volatile uint8_t pwm_in_use = 0; /*flag*/
volatile uint8_t tick_scaler = 0; /*used to throttle the speed of ISR1*/
uint8_t occupied_channels = 0; /*each channel is a flag; hence 0x7 means all 3 channels are currently playing*/

/* sequencer variables */ 
volatile uint32_t bpmCounter = 0; 
uint32_t bpmLimit = 1708; /*how far bpmCounter should count before it counts as a tick (1708 = "Q:1/4=120" by default)*/
uint8_t next_note;
uint32_t length; /*length of the next note to play*/
uint16_t time_until_next_note = 0; /*number of sequencer ticks before the next note is read and played*/
int8_t accidental_shift; /*number of semitones to increase pitch of the next note by*/
char numstring[16]; /*temporary variable to store a string of digits and slashes as they are being read in (used for note length modifiers)*/
int8_t number_mode; /*temporary variable to store the index of the last digit seen in readlinebuffer*/

#define ABC_STOPPED 0
#define ABC_PLAYING 1
#define ABC_FINISHING 2
uint8_t abc_playing = ABC_STOPPED; 
/*abc_playing values:
 *	0 if stopped
 *	1 if playing and still reading from the file
 *	2 if finished reading from the file and the last notes are still playing
 */

#define rest 128
#define natural 64
#define chord 32
uint8_t note_flags = 0; 
/*	set when the previous note is a rest, has a natural modifier, or is part of a chord. stored as follows:
		x      x 		 x 		 xxxxx
		rest | natural | chord | (unused)
	use note_flags the same way you'd use pins
*/

/* song information (from headers) */
char* title; /*title of the current song*/
uint16_t default_note_length = 8; /*base note length if not otherwise specified (1/4 note - eight 32nds - by default)*/
uint8_t key_signature[7] = {A4, B4, C4, D4, E4, F4, G4}; /*key signature of the current song*/

/* file io variables */
FATFS fs; /*filesystem*/
FIL file; /*current file*/
volatile char* readlinebuffer = 0; /*current line of the file being read*/
volatile uint16_t readline_index = 0; /*current position through readlinebuffer*/

/*  initialise the PWM 
 *	credit to: 
 *		github.com/fatcookies/lafortuna-wav-lib 
 *		http://avrpcm.blogspot.co.uk/2010/11/playing-8-bit-pcm-using-any-avr.html
 */
void pwm_init(void) {
    /* use OC1A (RCH) and OC3A (LCH) pin as output */
	DDRB |= _BV(PB5);
	DDRC |= _BV(PC6);

    /* 
    * clear OC1A/OC3A on compare match 
    * set OC1A/OC3A at BOTTOM, non-inverting mode
    * Fast PWM, 8bit
    */
    TCCR1A |= _BV(COM1A1) | _BV(WGM10);
    TCCR3A |= _BV(COM3A1) | _BV(WGM30);
    
    /* 
    * Fast PWM, 8bit
    * Prescaler: clk/1 = 8MHz
    * PWM frequency = 8MHz / (255 + 1) = 31.25kHz
    */
    TCCR1B |= _BV(WGM12) | _BV(CS10);
    TCCR3B |= _BV(WGM32) | _BV(CS30);
    
    /* set initial duty cycle to zero */
    OCR1A = 0;
    OCR3A = 0;
    
    /* Setup Timer1 (RCH, used for waveform generation) */
    TCNT1 = 0;
    TIMSK1 |= _BV(TOIE1);
    
    /* Setup Timer3 (LCH, used to manipulate the clock for sequencing) */
    TCNT3 = 0;
    TIMSK3 |= _BV(TOIE3);

    pwm_in_use = 1;

    sei();
}

/* disable pcm audio generation and undo pin settings
 *	credit to: 
 *		github.com/fatcookies/lafortuna-wav-lib 
 *		http://avrpcm.blogspot.co.uk/2010/11/playing-8-bit-pcm-using-any-avr.html
 */
void pwm_stop(void){
	if(pwm_in_use) {
		TCCR1A &= ~_BV(CS00);
		TCCR1B &= ~_BV(CS10);
		TIMSK1 &= (0<<TOIE1);

		TCCR3A &= ~_BV(CS00);
		TCCR3B &= ~_BV(CS30);
		TIMSK3 &= (0<<TOIE3);

		pwm_in_use = 0;
	}
}

/* generate a waveform from the active notes in the channels */
/* NOTE: if notes start playing at a lower pitch than you were expecting, it likely means that ISR1 is taking too long to compute!*/
ISR(TIMER1_OVF_vect)
{	
	if(pwm_in_use){
		if(!tick_scaler){ /*only generate the waveform once every tick_scaler times the interrupt is raised*/
			int8_t polyphony = 0; /*number of channels playing simultaneously on this tick*/
			int16_t combined_ticks = 0; /*temp variable to accumulate */
			uint8_t i;
			for(i=0;i<CHANNELS;i++){
				if(channels[i].note!=0xFF){
					polyphony++;
					/*advance tick according to pitch*/
					channels[i].tick += note_step[channels[i].note];
					channels[i].tick &= 511;
					/*generate appropriate wave from current tick*/
					if(channels[i].wave==SINE){
						combined_ticks += sine[channels[i].tick >> 1];
					}else if(channels[i].wave==SQUARE){
						combined_ticks += (channels[i].tick & 256) ? 255 : 0;
					}else if(channels[i].wave==TRIANGLE){
						uint8_t x = (channels[i].tick & 255);
						uint8_t y = channels[i].tick & 256 ? -x-1 : x;
						combined_ticks += y;
					}else{
						combined_ticks += channels[i].tick >> 1;
					}
				}
			}
			/*if at least one note just played, update the duty cycle*/
			if(polyphony){
				combined_ticks /= polyphony;
				OCR1A = combined_ticks;
				OCR3A = OCR1A;
			}
		}
		tick_scaler++;
		tick_scaler&=7;
		/*
		 * from experimentation, one real tick every 8 has the best results; hence 7 as a magic number.
		 * decreasing 7 results in a higher quality waveform, but the ISR has a shorter deadline and can't cope with 3 voices anymore
		 * increasing 7 makes the ISR take less time but considerably distorts the waveform
		 */
	}

}

/*play a note on the lowest free channel available, or replace the note in channel 3 if they're all taken*/
void channel_play(uint8_t note, uint8_t duration){
	if(~occupied_channels){
		int8_t free_channel = (occupied_channels & 1) * (1 + (occupied_channels & 3)/3); /*calculates (MSB+1) if <3 channels occupied; else MSB if all occupied*/
		channels[free_channel].note=note;
		channels[free_channel].time_until_release=duration;
		channels[free_channel].tick=0;
		occupied_channels |= (1 << free_channel);
	}
}

/*stop the note on the given channel*/
void channel_stop(uint8_t channel){
	channels[channel].note = 0xFF;
	occupied_channels &= ~(1 << channel);
}

/*get whether the PWM is currently being used by the sequencer*/
uint8_t pwm_is_in_use(void) {
	return pwm_in_use;
}

/*set the waveform of the given channel*/
void channel_set_wave(uint8_t channel, uint8_t wave){
	channels[channel].wave=wave;
}

/* left audio channel timer is also used to increment the sequencer clock */
ISR(TIMER3_OVF_vect)
{
	bpmCounter++;
} 

/* set the tempo according to the given BPM */
void set_tempo(uint16_t bpm){
	bpmLimit = calculate_tempo_32nd(bpm);
}

/* figure out to what number the timer 3 interrupt should count to before advancing by 1/32nd of a bar,
 * given bpm in the form of number of crotchets (1/4-notes) per bar
 */
uint32_t calculate_tempo_32nd(uint16_t bpm){
	return 153750/bpm;
}

/*mount and read the header of a given abc file, ready to be played*/
FRESULT abc_load_file(char* filename){
	/*initialise all channels*/
	uint8_t i;
    for(i=0;i<CHANNELS;i++){
    	channels[i].note=0xFF;
    	channels[i].wave=SINE;
    	channels[i].time_until_release=0;
    	channels[i].tick=0;
    }
	time_until_next_note=0;
    /*mount and open the file*/
	f_mount(&fs, "", 0);
	FRESULT result = f_open(&file, filename, FA_READ);
	/*if the file exists and can be read, read the entire header*/
	if(result == FR_OK){
		if(readlinebuffer) free(readlinebuffer);
		readlinebuffer=malloc(sizeof(char)*LINE_BUFFER_SIZE);
		/*read lines of the file*/
		while(readlinebuffer=f_gets(readlinebuffer, LINE_BUFFER_SIZE, &file)){
			/*if the current line is a valid header line*/
			if(readlinebuffer[1]==':' || readlinebuffer[0]=='%' || readlinebuffer[1]=='%'){
				switch(readlinebuffer[0]){
					case('T'):	/*title*/
						if(title) free(title);
						title = malloc(sizeof(char)*strlen(readlinebuffer+2));
						strcpy(title, readlinebuffer+2);
						break;
					case('L'): /*unit note length*/
						default_note_length = string_to_note_length(readlinebuffer+2);
						break;
					case('Q'): /*tempo*/
						i=0;
						/*read the note length into a string*/
						char note_length_string[16];
						for(i=0;i<15;i++){
							if(readlinebuffer[i+2]=='=' || readlinebuffer[i+2]=='\0'){
								break;
							}else{
								note_length_string[i]=readlinebuffer[i+2];
							}
						}
						note_length_string[i]='\0';
						/*calculate the note length from the string*/
						uint16_t note_length;
						if(readlinebuffer[i+2]=='='){
							note_length = string_to_note_length(note_length_string);
							i++;	
						}else{
							/*if there's no = sign in the string, then no note length was specified, so default to 1/4*/
							note_length = 8;
							i=0;
						}
						/*read the tempo from the rest of the string*/
						uint16_t tempo = atoi(readlinebuffer+i+2);
						/*adjust tempo to be its equivalent for Q:1/4=tempo*/
						if(note_length<8){
							while(note_length<8){
								note_length <<= 1;
								tempo >>= 1;
							}
						}else if(note_length>8){
							while(note_length>8){
								note_length >>= 1;
								tempo <<= 1;
							}
						}
						/*finally set the tempo*/
						bpmLimit = calculate_tempo_32nd(tempo);
						break;
					case('K'): /*key signature*/
						changeKey(readlinebuffer+2);
						break;
					case('I'): /*'instruction' (used to change a channel's waveform)*/
						parse_lf_tag(readlinebuffer+2);
						break;
					default:; /*ignore: either unknown, not supported, or nothing to do for it*/
				}
			}else{ /*finished reading the header*/
				break;
			}
		}
	}
	return result;
}

/* if a note has been read and is waiting ot be played, play it */
uint8_t playNoteIfAvailable(void){
	if(note_flags & rest || (next_note!=0xFF)){ /*if either the next note is a rest, or the pitch of the next note is known*/
		/*set the time until another note is read and played as follows:
		 *if the next note is a rest, use the rest's length
		 *else
		 *	if the next note's length is shorter than the current time until another note is read, use that
		 *  else leave time_until_next_note as it is
		*/
		time_until_next_note = note_flags & rest ? length : (length < time_until_next_note ? length : time_until_next_note);
		/*play the note, accounting for sharp/flat signs before it*/
		channel_play(next_note + accidental_shift, length); 
		if(!(note_flags & chord)){
			return 1;
		}else{ 
			/*if the note is part of a chord, reset temporary variables for this node*/
			next_note = 0xFF; 
			accidental_shift = 0; 
			note_flags &= ~(rest | natural);
			length=default_note_length; 
		} 
	}
	return 0;	
} 

/*try to play the next note; if a note played and it wasn't part of a chord, break from the while loop*/
#define playNoteOrBreak if(playNoteIfAvailable()) break;

/*play an entire abc file that has already been loaded*/
void abc_play(void){
	pwm_init();
	abc_playing = ABC_PLAYING;

	uint8_t i=0;
	while(abc_playing){
		if(bpmCounter>bpmLimit){ /*if the timer is large enough to count as a tick*/
			cli();
			/*update the timers of notes currently playing; and stop any that have counted down to 0*/
			for(i=0;i<CHANNELS;i++){
				if(!channels[i].time_until_release){
					channels[i].note=0xFF;
					occupied_channels &= ~(1<<i);
				}else{
					channels[i].time_until_release--;
				}
				/*if all notes have finished, and no more will be read in, then stop the song*/
				if(!occupied_channels && abc_playing==ABC_FINISHING) abc_stop();
			}
			/*if it's time to play the next note, do so:*/
			if(abc_playing==ABC_PLAYING && !time_until_next_note){
				/*initialise all temporary variables*/
				time_until_next_note=0xFF;
				note_flags = 0;
				accidental_shift = 0;
				numstring[0]='\0';
				length = default_note_length;
				next_note = 0xFF;
				number_mode = 0;
				/*continuously read characters from the file and play them accordingly*/
				while(1){
					/*if the current character is either a number or a forward slash, then it represents a note length*/
					if(readlinebuffer[readline_index] >= '/' && readlinebuffer[readline_index] <= '9'){
						/*store the current character in a string, to deal with later when the entire number has been read*/
						if(!number_mode) number_mode=readline_index;
						numstring[readline_index-number_mode]=readlinebuffer[readline_index];
						numstring[readline_index-number_mode+1]='\0';
					}else{
						if(number_mode){ /*if numbers were being read, but the current character isn't a number, deal with that number:*/
							number_mode = 0;
							/*convert numstring to a note length and scale it according to the default note length for this song*/
							length = (string_to_note_length(numstring) * default_note_length) >> 5; 
							if(length==0) length=1; /*don't skip the entire song if the note length is too small*/
							if(note_flags & rest) length*=1.5; /*rests finish much faster than notes; this counters that*/
							readline_index--; /*undo the increment later so we can read this character again*/
						}else if(readlinebuffer[readline_index]>='A' && readlinebuffer[readline_index]<='G'){
							/*capital letters are used for notes G4 and under*/
							playNoteOrBreak
							if(note_flags & natural){
								next_note = C_MAJOR[readlinebuffer[readline_index] - 'A'];
							}else{
								next_note = key_signature[readlinebuffer[readline_index] - 'A'];
							}
						}else if(readlinebuffer[readline_index]>='a' && readlinebuffer[readline_index]<='g'){
							/*lowercase letters are used for notes A5 and up*/
							playNoteOrBreak
							if(note_flags & natural){
								next_note = C_MAJOR[readlinebuffer[readline_index] - 'a'] + 12;
							}else{
								next_note = key_signature[readlinebuffer[readline_index] - 'a']+12;
							}
						}else if(readlinebuffer[readline_index]==','){
							/*commas after a note decrease its pitch by an octave*/
							if(next_note!=0xFF) next_note-=12;
						}else if(readlinebuffer[readline_index]=='\''){
							/*apostrophes after a note increase its pitch by an octave*/
							if(next_note!=0xFF) next_note+=12;
						}else if(readlinebuffer[readline_index]=='_'){
							/*underscores before a note decrease its pitch by a semitone*/
							playNoteOrBreak
							accidental_shift--;
						}else if(readlinebuffer[readline_index]=='^'){
							/*circumflexes before a note increase its pitch by a semitone*/
							playNoteOrBreak
							accidental_shift++;
						}else if(readlinebuffer[readline_index]=='='){
							/*equals signs before a note naturalise it (i.e. the note is taken from the C Major key rather than the song's current key)*/
							playNoteOrBreak
							note_flags |= natural;
						}else if(readlinebuffer[readline_index]=='\0' || readlinebuffer[readline_index]=='%'){
							/*if the end of a line or start of a comment is reached, read the next line*/
							do{
								readlinebuffer=f_gets(readlinebuffer, LINE_BUFFER_SIZE, &file);
								readline_index=-1; /*it'll be incremented to 0 momentarily*/
								/*some header things can also appear in the middle of music. deal with these:*/
								if(readlinebuffer[1]==':'){
									switch(readlinebuffer[0]){
										case('K'): /*key signature*/
											changeKey(readlinebuffer+2);
											break;
										case('I'): /*'instruction' (used to change a channel's waveform)*/
											parse_lf_tag(readlinebuffer+2);
											break;
										default:;
									}
								}
							}while(readlinebuffer && readlinebuffer[1]==':'); /*keep reading until there isn't a header-like line*/
							if(!readlinebuffer){ /*if nothing was read, prepare to stop playback*/
								abc_playing = ABC_FINISHING;
								break;
							}
						}else if(readlinebuffer[readline_index]==' ' || readlinebuffer[readline_index]=='|'){
							/*spaces or bars are never part of a note; so try to play a note if one has already been loaded*/
							playNoteOrBreak
						}else if(readlinebuffer[readline_index]=='['){
							/*start of a chord (notes played simultaneously appear in square brackets)*/
							playNoteOrBreak
							note_flags |= chord;
						}else if(readlinebuffer[readline_index]==']'){
							/*end of a chord*/
							if(note_flags & chord) note_flags &= ~chord;
							readline_index++;
							playNoteOrBreak
						}else if(readlinebuffer[readline_index]=='z' || readlinebuffer[readline_index]=='x'){
							/*z and x indicate rests - i.e. a period of silence instead of a note*/
							playNoteOrBreak
							else note_flags |= rest;
						}else if(readlinebuffer[readline_index]=='-'){
							/*ignore ties*/
						}else{ /*not part of a note*/
							playNoteOrBreak
						}
					}
					readline_index++;
				}
			}else{
				time_until_next_note--;
			}
			bpmCounter=0;
			sei();
		}
	}

}

uint8_t abc_is_playing(){
	return abc_playing;
}

/*stop playback of an abc file*/
void abc_stop(void) {
	abc_playing=0;
	pwm_stop();
}

/*get the title of the song currently loaded*/
char* abc_song_title(void){
	return title;
}

/*convert a string of the form "n/d" for ints n,d into a number representing the number of 1/32nds of a bar for that interval*/
uint16_t string_to_note_length(char *str){
	uint8_t i = 0;
	uint8_t fraction = 0; /*number of slashes read so far*/
	uint8_t no_numbers_set=1;
	char current_number_string[4]; 
	uint8_t current_number_position=0; /*position in the above string*/
	uint32_t n=1, d=1; /*numerator, denominator respectively*/
	/*parse the entire string input string*/
	while(str[i]!='\0'){
		if(str[i]=='/'){
			fraction++;
			current_number_string[current_number_position]='\0';
			/*numerator can now be converted and stored*/
			n=atoi(current_number_string);
			if(!n) n=1;
			current_number_position=0;
		}else if(str[i] >= '0' && str[i] <= '9'){
			/*store digits in a string as they are being read*/
			no_numbers_set=0;
			current_number_string[current_number_position++]=str[i];
		}
		i++;
	}
	current_number_string[current_number_position]='\0';
	if(fraction){
		if(no_numbers_set){
			/*a lone slash is shorthand for 1/2; similarly "//"" = 1/4, "///" = 1/8, etc. */
			n=1;
			d=1 << fraction;
		}else{ 
			/*convert and store the denominator if there is one*/
			d=atoi(current_number_string);
		}
	}else{
		/*convert and store the numerator*/
		n=atoi(current_number_string);
	}
	n*=32; /*lengths are expressed as multiples of 1/32nd of a bar*/
	return n/d;
}

/*change the song's current key signature*/
void changeKey(char *keystring){
	uint8_t i, j;
	j=0;
	/*remove any leading spaces or other junk in the way of the first letter*/
	while(keystring[j]<'A' || keystring[j] > 'G'){
		j++;
		if(keystring[j]=='\0') return; /*ignore if no letters*/
	}
	/*initialise the key at C Major*/
	for(i=0;i<7;i++){
		key_signature[i]=C_MAJOR[i];
	}
	uint8_t key = keystring[j+0]; /*a letter from A-G*/
	uint8_t modifier = keystring[j+1]; /*either b for flat, # for sharp, or something we don't care about for natural*/
	if(key=='C'&&modifier!='b'&&modifier!='#') return; /*C Major*/
	/*(see notes.h for an explanation of the algorithm below)*/
	if(modifier=='b' || (key=='F' && modifier!='#')){ 
		/*this key is generated by flattening notes*/
		for(i = flat_signatures[key-'A'];i!=0xFF;i--){
			key_signature[Cb_MAJOR[i]]--;
		}
	}else{
		/*this key is generated by sharpening notes*/
		for(i = sharp_signatures[key-'A'];i!=0xFF;i--){
			key_signature[Cs_MAJOR[i]]++;
		}
	}
}

/*interpret an "I:" tag; currently can only change the waveform*/
void parse_lf_tag(char *tagstring){
	/*wave change instructions are of the form "lf-wave:xy" where 0<=x<=2 and 0<=y<=3*/
	/*the first 8 characters are always the same; verify this:*/
	char lf_wave[8] = "lf-wave:";
	int8_t i=0;
	for(i=0;i<8;i++){
		if(lf_wave[i]!=tagstring[i]) return;
	}
	/*seek through the line for the first two numbers on it*/
	/*first number represents the channel*/
	while(tagstring[i]<'0' || tagstring[i]>'9'){
		i++;
		if(tagstring[i]=='\0') return;
	}
	int8_t channel = tagstring[i]-'0';
	i++;
	/*second nubmer represents the waveform to use*/
	while(tagstring[i]<'0' || tagstring[i]>'9'){
		i++;
		if(tagstring[i]=='\0') return;
	}
	int8_t wave = tagstring[i]-'0';
	/*update accordingly*/
	channels[channel].wave=wave;
}
