#ifndef _LCD_SCREENS_H
#define _LCD_SCREENS_H

#include <twr.h>

extern struct tm cur_time;

uint8_t reverse_bits(uint8_t bits);
void endian_correct_img_data(twr_image_t *);

void renderDateTime();

void renderBtns();

void renderMeasurements();

void renderCalibration();

void renderLora();


#endif // _LCD_SCREENS_H