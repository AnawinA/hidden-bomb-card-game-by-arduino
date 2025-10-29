// Variables.h
#ifndef VARIABLES_H
#define VARIABLES_H
#include <avr/pgmspace.h>

extern const unsigned char PROGMEM bombBitmap[];
extern const unsigned char bombBitmap_2 [] PROGMEM; 
extern const unsigned char epd_bitmap_game_icon [] PROGMEM;
extern const unsigned char epd_bitmap_roulette [] PROGMEM;
extern const unsigned char epd_bitmap_chest [] PROGMEM;
extern const unsigned char epd_bitmap_defuse [];
extern const unsigned char epd_bitmap_shield [] PROGMEM;
extern const unsigned char epd_bitmap_glass [] PROGMEM;
extern const unsigned char epd_bitmap_arrow [] PROGMEM;
extern const unsigned char epd_bitmap_coin [] PROGMEM;
extern const unsigned char epd_bitmap_boom [] PROGMEM;
extern const unsigned char epd_bitmap_boom_hide [] PROGMEM;
enum ItemName {NONE, COIN, DEFUSE, PREDICT, SHIELD};


#endif // VARIABLES_H