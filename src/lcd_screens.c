#include "lcd_screens.h"
#include <twr.h>
#include <twr_atci.h>

extern twr_data_stream_t sm_voltage;
extern twr_data_stream_t sm_battery_pct;
extern twr_data_stream_t sm_temperature;
extern twr_data_stream_t sm_humidity;
extern twr_data_stream_t sm_illuminance;
extern twr_data_stream_t sm_pressure;
extern twr_data_stream_t sm_co2;
extern twr_data_stream_t sm_orientation;

void renderDateTime() {

    //https://tower.hardwario.com/en/latest/firmware/how-to-rtc-clock/

    //TODO Also show dev name at top

    char date_string[60];// = "Sun Dec 31 2022 21:00";

    char days_of_week[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    struct tm datetime;
    twr_rtc_get_datetime(&datetime);

    sprintf(date_string, "%s %s %d %d %02d:%02d", days_of_week[datetime.tm_wday], months[datetime.tm_mon], datetime.tm_mday, (1900+ datetime.tm_year), datetime.tm_hour, datetime.tm_min);

    twr_module_lcd_set_font(&twr_font_ubuntu_11);
    twr_module_lcd_draw_string(5, 1, date_string, true);
} 

void renderBtns() {

    //top line for button section
    twr_module_lcd_draw_line(0, 113, 127, 113, true);

    //middle line
    twr_module_lcd_draw_line(63, 114, 63, 127, true);

    //half lines
    twr_module_lcd_draw_line(16, 114, 16, 127, true); //left
    twr_module_lcd_draw_line(112, 114, 112, 127, true); //right

    //triangle left
    twr_module_lcd_draw_line(10, 117, 10, 125, true);
    twr_module_lcd_draw_line(9, 118, 9, 124, true);
    twr_module_lcd_draw_line(8, 119, 8, 123, true);
    twr_module_lcd_draw_line(7, 120, 7, 122, true);
    twr_module_lcd_draw_pixel(6, 121, true);

    //triangle right
    twr_module_lcd_draw_line(118, 117, 118, 125, true);
    twr_module_lcd_draw_line(119, 118, 119, 124, true);
    twr_module_lcd_draw_line(120, 119, 120, 123, true);
    twr_module_lcd_draw_line(121, 120, 121, 122, true);
    twr_module_lcd_draw_pixel(122, 121, true);

    //hold btns
    char hold_string[20];
    twr_module_lcd_set_font(&twr_font_ubuntu_11);

    //hold left text
    //TODO logic for left hold string

    strcpy(hold_string, "send data");
    twr_module_lcd_draw_string(22, 116, hold_string, true);

    //hold right text
    //TODO logic for right hold string
    
    strcpy(hold_string, "calibrate");
    twr_module_lcd_draw_string(69, 116, hold_string, true);
}

void renderMeasurements() {

    char str[32];

    float avg_val = NAN;

    //temp
    twr_data_stream_get_average(&sm_temperature, &avg_val);
    twr_module_lcd_set_font(&twr_font_ubuntu_24);
    
    if(isnan(avg_val)) {
        
        snprintf(str, sizeof(str), "--%s", "\xb0" " F");
        twr_module_lcd_draw_string(7, 17, str, true);
    }
    else { 

        snprintf(str, sizeof(str), "%.1f%s", avg_val, "\xb0" " F");
        twr_module_lcd_draw_string(7, 17, str, true);
    }

    //TODO humidity top right




    //co2
    avg_val = NAN;
    twr_data_stream_get_average(&sm_co2, &avg_val);

    if(isnan(avg_val)) {

        twr_module_lcd_set_font(&twr_font_ubuntu_15);
        twr_module_lcd_draw_string(25, 55, "Loading Co2", true);
    }
    else {

        twr_module_lcd_set_font(&twr_font_ubuntu_33);
        snprintf(str, sizeof(str), "%.0f", avg_val);
        twr_module_lcd_draw_string(25, 45, str, true);

        twr_module_lcd_set_font(&twr_font_ubuntu_15);
        twr_module_lcd_draw_string(61, 80, "CO2 ppm", true);
    }

    //battery lower left
    avg_val = NAN;
    twr_data_stream_get_average(&sm_battery_pct, &avg_val);

    if(!isnan(avg_val)) {

        //big rectangle
        twr_module_lcd_draw_rectangle(7, 83, 25, 93, true);

        //small line on right
        twr_module_lcd_draw_line(27, 85, 27, 91, true);

        //10 lines inside (for loop)
        int num_lines = (int) (avg_val / 6.666); //15 lines max
        for (int i=1; i <= num_lines; i++) {

            twr_module_lcd_draw_line(8+i, 85, 8+i, 91, true);
        }
    }
}

void renderCalibration() {

    twr_module_lcd_set_font(&twr_font_ubuntu_24);
    twr_module_lcd_draw_string(2, 35, "Calibrate Stub", true);
}

void renderLora() {

    twr_module_lcd_set_font(&twr_font_ubuntu_24);
    twr_module_lcd_draw_string(5, 35, "LoRa Stub", true);
}