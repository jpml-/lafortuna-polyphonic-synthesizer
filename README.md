# JPML's Polyphonic Music Library!
A polyphonic synthesizer and ABC notation file player library for the LaFortuna by jpml1g14.  
The LaFortuna is an embedded device custom-built by the University of Southampton whose processor is an AT90USB1286 running at 8MHz. The left audio channel is connected to OC3A; the right audio channel is connected to OC1A.

### Features
* 3 audio channels - plays up to 3 notes simultaneously!  
* Each channel can play tones using one of four waveforms: Sine, Triangle, Square, Sawtooth  
* Can play most abc notation files downloaded from the internet out-of-the-box  
  * See this project's page on the notes wiki for some mp3s of music played by the LaFortuna and the accompanying files to play them.  

### Usage  
To play an ABC file:  
* Load the file from the SD card with abc_load_file(filename)  
* Play the file with abc_play() 
  * (NOTE: this loops until the song is finished as abc-file parsing is too complex for an ISR, so make sure this task is either managed by a scheduler or is the only non-ISR routine if you intend to use it as background music)  
* Stop playback with abc_stop(), or wait for the song to end and it will stop on its own  
* Check whether the song is playing or not with abc_is_playing()  

If you really hate ABC notation you can manipulate the synthesizer yourself:  
* Initialise the speakers and timers with pwm_init()  
* Verify that the above are initialised with pwm_is_in_use()  
* Use channel_play(note, duration) to play a note on the first channel not currently in use; or replace the note on channel 3 if they're all in use  
* Use channel_stop(channel) to stop the note on the given channel  
* Use channel_set_wave(channel, wave) to change the waveform of tones played by the given channel  
* When you're finished, call pwm_stop()  

If you want to do weird things to a song while it's playing, the following methods are also exposed:  
* changeKey(keystring) changes the key signature  
* set_tempo(bpm) changes the tempo of the song (in crotchets per minute)  

### La Fortuna ABC Notation
Music files understood by this library are a subset of standard ABC notation, plus one extra operation. ABC files consist of a header with metainformation and details on how to play the song, followed by the body of the song which consists mostly of notes. Here is an exhaustive list of all elements of ABC notation understood by this implementation:  

* Comments begin with '%' and can appear anywhere in the file.  

#### Header
Headers are composed of multiple lines. The general format of a line is:  

- x:y  

where x is a single letter and y is the rest of the line. Types of header lines accepted by this library:

The following lines *must* appear before the song body, else they will either be ignored or cause playback errors:
* "T:x" - Set the title of the song to be x  
* "L:n/d" - Set the default length of a note to be n/d (e.g. a crotchet is 1/4. if this is not specified, a crotchet is used)  
* "Q:n/d=t" - Set the song speed such that notes of length n/d are played t times per minute. E.g. "Q:1/2=120" means 120BPM. You can omit 'n/d=' - if you do so, a note length of 1/4 will be assumed.  

The following lines can appear anywhere in the song body as well as in the header - if they appear in the song body, they will take effect when all notes that appeared before them have been played:  

 * "K:xy" - Set the key signature of the song to be 'xy', where x is a capital letter from A-G inclusive, and y is either '#', 'b', or nothing.  
   * N.B. Only major keys are recognised, but there is a 1:1 mapping from minor keys to major keys, as with all other sets of key signatures, so this can be easily changed in the ABC file.  
 * "I:lf-" - Custom instructions specific to this implementation of ABC notation on LaFortuna. There is currently only one such instruction:  
   * "I:lf-wave:cw" Set the waveform of channel c to be w, where c is an integer from 0-2 inclusive representing the channel to set the waveform of, and w is as follows:  
      * 0 = Sine wave  
      * 1 = Triangle wave  
      * 2 = Square wave  
      * 3 = Sawtooth wave  

Any line not corresponding to this format is taken to be the start of the body and the end of the header (with the exception of comments)  

#### Body
Lines in the body can either be one of the selected header lines as noted in the above section, or they can be a sequence of notes which will be played in order from left-to-right. E.g. here is a valid C-major scale in abc notation:  
* C D E F G a b c  

(In standard ABC notation, groups of notes are separated by bars via the pipe character '|', and notes within bars are separated by spaces. However, this implementation doesn't care and will accept any arbitrary presence or absence of spaces and bars as they have no effect on playback.)  

Lines must not exceed 1024 characters in length.  

Capital letters represent notes in the fourth octave - e.g. 'C' represents middle C (C4). Lowercase letters represent notes in the fifth octave.  

Any note can be modified in the following ways (let n be a note):  
* "nm/d" sets the length (duration) of note n to be (m/d) times the default length. m, / and d are all optional. *Note: the shortest length understood by this implementation is 1/32nd of a bar.* Examples:  
  * n2 causes note n to be played for double the default note length  
  * n/2 causes note n to be played for half the default note length  
  * n3/8 causes note n to be played for three-eighths of the default note length  
  * n/ causes note n to be played for half the default note length  
  * n// causes note n to be played for a quarter of the default note length. Slashes can keep being added in this way to continually halve note length.  
  * n on its own without a note length will cause it to be played for the default note length  
* "n," sets the pitch to be one octave lower than n's default pitch. Examples:  
  * "C," plays the note C3  
  * "C,," plays the note C2. Commas can keep being added in this way to continually lower pitch by an octave.  
* "n'" sets the pitch to be one octave higher than n's default pitch. Examples:  
  * "c'" plays the note C6.  
  * "c''" plays the note C7. Apostrophes can keep being added in this way to continually raise pitch by an octave.  
* "_n" sets the pitch to be one semitone lower than n's default pitch. Examples:  
  * "_E" plays the note Eb4  
  * "__E" plays the note D4. Underscores can keep being added in this way to continually lower pitch by a semitone.  
* "^n" sets the pitch to be one semitone higher than n's default pitch. Examples:  
  * "^D" plays the note Eb4  
  * "^^D" plays the note E4. Circumflexes can keep being added in this way to continually raise pitch by a semitone.  
* "=n" indicates that the song's key signature should be ignored when playing note n. E.g. If, in a song with a key of Eb Major, the string "=E" is encountered, then the note E4 will be played instead of Eb4.  
* "[pqr]" plays the notes p, q and r simultaneously as a chord. Examples:  
  * [cg] plays the notes C5 and G5 simultaneously.  
  * [ce/g] plays the notes C5 and G5 for the default note length, and E5 for half of it. When E5 has finished playing, the next note will be played, and C5 and G5 will continue playing for half the default note length.  
  * *NB:* [cegc'] plays only the notes C5, E5 and C6. This library is limited to 3 simultaneous voices - if more notes are specified than can be played, the note in the last voice is overwritten before any sound comes out.  
* "x" or "z" indicate rests. Their length can be modified as with regular notes, and they can be used as part of a chord to cause the next note to play before a note inside the chord has finished. Examples:  
  * "x" causes a silence that lasts for the default note length.  
  * "z/2" causes a silence that lasts for half the default note length.  
  * "[cgz/2]e/2" plays C5 and G5 simultaneously for half the default note length, then C5, E5 and G5 for the other half of the default note length.  

All other characters in the notes body are ignored.  

## Credit and Dependencies
Credit and thanks to the following for code included in this archive:  
- arp1g13; I used code from his LaFortuna WAV audio library as inspiration and as a starting point to see how to get audio out of the LaFortuna: github.com/fatcookies/lafortuna-wav-lib  
  - This library was itself adapted from: http://avrpcm.blogspot.co.uk/2010/11/playing-8-bit-pcm-using-any-avr.html  
- ChaN for his Fat-FS implementation: elm-chan.org/fsw/ff/00index_e.html  
- Klaus Peter-Zauner for the Universal Makefile.  
- Steve Gunn for the LCD library - this is _not_ a dependency of the JPML library but it is used in example.c.  