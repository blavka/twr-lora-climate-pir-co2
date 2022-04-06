#ifndef _LCD_SCREENS_H
#define _LCD_SCREENS_H

#include <twr.h>

extern uint32_t cur_time;

void renderDateTime();

void renderBtns();

void renderMeasurements();

void renderCalibration();

void renderLora();


#endif // _LCD_SCREENS_H