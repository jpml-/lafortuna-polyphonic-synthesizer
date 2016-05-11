/*
 * JPML's Polyphonic Music Library
 * Developed for La Fortuna (at90usb1286) @ 8MHz
 *
 * A polyphonic synthesizer and abc player library by jpml1g14.
 *
 * Example file to show how to load and play an ABC Notation music file
 */

#include "jpml.h"
#include <avr/io.h>
#include "lcd/lcd.h"
#include "fatfs/ff.h"

int main(void) {
	/* 8MHz clock, no prescaling (DS, p. 48) */
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;
    
	init_lcd();
	display_color(LIME, 0xA254); /*because 0xA254 is the best colour*/
    clear_screen();
	
    /* draw the portal logo (under the assumption that this will be playing Still Alive) */
	display_string("             ,I??I??IIIII=              \n");
	display_string("          ?II, ???I?III???? :           \n");
	display_string("        ?I?I???= :I??I??II?+ I?+        \n");
	display_string("      I?I?I???II??  I??I???I I???:      \n");
	display_string("    =?IIIII???I?I?II  I?II?? +?IIII     \n");
	display_string("   ????I?IIIII=         ~I??, II?I??    \n");
	display_string("  =?I+,                    I? ????II?   \n");
	display_string("   ,+IIII?I                   II??????  \n");
	display_string(" ??I???II?                    ~?I?II?   \n");
	display_string(" ???I???I                      ?III: ?I \n");
	display_string("?I??III?                       ?II  ?II \n");
	display_string("I??II?:                        II  I?II \n");
	display_string("??I??, I                         :?II?I \n");
	display_string("II??  ?I                        +III?II \n");
	display_string(":?? ,I?I~                      I?????II \n");
	display_string(" ? :?????                     III???I?, \n");
	display_string("  =??I??I                    I???I?I?I  \n");
	display_string("  I????I?, :                ?I?:    ,   \n");
	display_string("   ????II? ?I?              ,???I??I    \n");
	display_string("    IIIII? ???II    ~I?I???II????I?     \n");
	display_string("     ~???? ,???I?I=  ?I?II?II??I?I      \n");
	display_string("       III? I?II???II  I???I?III        \n");
	display_string("         ~? II?I?I??I??  ????I          \n");
	display_string("            ,I????I??II???              \n");
	display_string("                                        \n");

	display_string_xy("_____ ___           ",10*6, 13*7);
    display_string_xy("  |  |__/ /\\/\\ |    ",10*6, 14*7);
    display_string_xy(" _/  |   /    \\|____",10*6, 15*7);
    display_string_xy("                    ", 10*6, 16*7);

    display_move(0, 34*6);

	if(abc_load_file("music.abc")==FR_OK){
		display_string("Now playing:\n");
		display_string(abc_song_title());
		abc_play();
		display_string("Song finished!");
	}
	for(;;);
}
