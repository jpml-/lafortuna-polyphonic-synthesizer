/*
 * JPML's Polyphonic Music Library
 * Developed for La Fortuna (at90usb1286) @ 8MHz
 *
 * A polyphonic synthesizer and abc player library by jpml1g14.
 *
 * notes.h contains data to do with note generation; separated out from jpml.h for readability 
 */


#ifndef _JPML_NOTES_H
#define _JPML_NOTES_H

/* lookup table: amount to increment channel note timer by on each tick of ISR1
 * hand-calculated in excel based on the clock frequency.
 * sounds good enough for the note range typically found in music but is much less accurate at lower pitches...
 */
static uint16_t note_step[88] = {
	3, 4, 4, /*A0-B0*/
	4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 8, /*C1-B1*/
	8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 16, /*C2-B2*/
	17, 18, 19, 20, 21, 22, 24, 25, 27, 28, 30, 32, /*C3-B3*/
	34, 36, 38, 40, 43, 45, 48, 51, 54, 57, 61, 64, /*C4-B4*/
	68, 72, 76, 81, 86, 91, 96, 102, 108, 115, 122, 129, /*C5-B5*/
	137, 145, 153, 163, 172, 183, 193, 205, 217, 230, 244, 258, /*C6-B6*/
	274, 290, 307, 326, 345, 366, 387, 411, 435, 461, 488, 517, /*C7-B7*/
	548 /*C8*/
};

/* readable constants for each note's index in the above note_step array */
#define A0 	0
#define Bb0 1
#define B0 	2
#define C1 	3
#define Db1	4
#define D1	5
#define Eb1	6
#define E1	7
#define F1	8
#define Gb1	9
#define G1	10
#define Ab1	11
#define A1	12
#define Bb1	13
#define B1	14
#define C2	15
#define Db2	16
#define D2	17
#define Eb2	18
#define E2	19
#define F2	20
#define Gb2	21
#define G2	22
#define Ab2	23
#define A2	24
#define Bb2	25
#define B2	26
#define C3	27
#define Db3	28
#define D3	29
#define Eb3	30
#define E3	31
#define F3	32
#define Gb3	33
#define G3	34
#define Ab3	35
#define A3	36
#define Bb3	37
#define B3	38
#define C4	39
#define Db4	40
#define D4	41
#define Eb4	42
#define E4	43
#define F4	44
#define Gb4	45
#define G4	46
#define Ab4	47
#define A4	48
#define Bb4	49
#define B4	50
#define C5	51
#define Db5	52
#define D5	53
#define Eb5	54
#define E5	55
#define F5	56
#define Gb5	57
#define G5	58
#define Ab5	59
#define A5	60
#define Bb5	61
#define B5	62
#define C6	63
#define Db6	64
#define D6	65
#define Eb6	66
#define E6	67
#define F6	68
#define Gb6	69
#define G6	70
#define Ab6	71
#define A6	72
#define Bb6	73
#define B6	74
#define C7	75
#define Db7	76
#define D7 	77
#define Eb7	78
#define E7 	79
#define F7 	80
#define Gb7	81
#define G7 	82
#define Ab7	83
#define A7 	84
#define Bb7	85
#define B7 	86

/* the following five arrays are indexed in the order {0, 1, 2, 3, 4, 5, 6} -> {A, B, C, D, E, F, G} */

static uint8_t C_MAJOR[7] = {A4, B4, C4, D4, E4, F4, G4}; /*c major scale, from which all other key signatures are constructed*/

/*indexes of c major to decrement to create a flattened key signature */
static uint8_t Cb_MAJOR[7] = {1, 4, 0, 3, 6, 2, 5}; 

/*indexes of c major to increment to create a sharpened key signature */
static uint8_t Cs_MAJOR[7] = {5, 2, 6, 3, 0, 4, 1};

/*which index of the flat array to start iterating downwards from. in order: Ab, Bb, Cb, Db, Eb, F, Gb*/
static uint8_t flat_signatures[7] = {3, 1, 6, 4, 2, 0, 5}; 

/*which index of the flat array to start iterating downwards from. in order: A, B, C#, D, E, F#, G*/
static uint8_t sharp_signatures[7] = {2, 4, 6, 1, 3, 5, 0};

/*e.g. to construct the Bb key signature:
	let key = C_MAJOR
	let x = flat_signatures[1] (1 because B is the second key alphabetically); this gives 1
	while x > 0 {
		key[Cb_MAJOR[x]]--
		x--
	}
*/

#endif /* _JPML_NOTES_H */