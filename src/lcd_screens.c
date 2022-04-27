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

extern bool is_connected, is_joined;
extern int32_t snr;
extern int32_t rssi;
extern uint8_t margin;
extern uint8_t gateway_count;

extern twr_cmwx1zzabz_t lora;

extern int page_index;

extern twr_scheduler_task_id_t calibration_task_id;

uint8_t connected_img_data[] = {0x7E, 0x00, 0x03, 0xFF, 0xC0, 0x0F, 0x81, 0xF0, 0x3C, 0x00, 0x3C, 0x70, 0x00, 0x0E, 0x60, 0xFF, 0x06, 0x03, 0xFF, 0xC0, 0x0F, 0x00, 0xF0, 0x0C, 0x00, 0x30, 0x08, 0x3C, 0x10, 0x00, 0xFF, 0x00, 0x01, 0xC3, 0x80, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x18, 0x00};
twr_image_t connected_img = { connected_img_data, 24, 18, 54 };

uint8_t disconnected_img_data[] = {0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x99, 0x00, 0x07, 0x99, 0xE0, 0x1E, 0x18, 0x78, 0x38, 0x18, 0x1C, 0x60, 0x18, 0x06, 0x40, 0xDB, 0x02, 0x03, 0xDB, 0xC0, 0x0F, 0x18, 0xF0, 0x0C, 0x18, 0x30, 0x00, 0x18, 0x00, 0x00, 0xDB, 0x00, 0x01, 0xDB, 0x80, 0x00, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x18, 0x00};
twr_image_t disconnected_img = { disconnected_img_data, 24, 21, 63 };

bool converted_images = false;

uint8_t reverse_bits(uint8_t bits) {

    uint8_t r = 0;
    uint8_t byte_len = 8;

    while (byte_len--) {
        r = (r << 1) | (bits & 1);
        bits >>= 1;
    }
    return r;
}

void endian_correct_img_data(twr_image_t *img) {

    uint8_t *img_data = (uint8_t *) img->data;

    //bits
    for(int i=0; i<img->dataSize; i++)
        img_data[i] = reverse_bits(img->data[i]);

    //bytes
    for(int i=0; i<img->height; i++) {

        uint8_t tmp;

        //swap 2nd and third places
        tmp = img->data[i*3+1];
        img_data[i*3+1] = img->data[i*3+2];
        img_data[i*3+2] = tmp;

        //swap 1st and 2nd places
        tmp = img->data[i*3];
        img_data[i*3] = img->data[i*3+1];
        img_data[i*3+1] = tmp;
    }
}

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

    if(page_index == 0) { //main measurement summary page

        //hold left text
        if(is_connected)
            strcpy(hold_string, "send data");
        else if(is_joined)
            strcpy(hold_string, "");
        else
            strcpy(hold_string, "reset conn");

        twr_module_lcd_draw_string(22, 116, hold_string, true);

        //hold right text
        strcpy(hold_string, "calibrate");

        twr_module_lcd_draw_string(69, 116, hold_string, true);
    }
    else if(page_index == 1) { //lora
        
        //hold left text
        if(is_connected)
            strcpy(hold_string, "test conn");
        else
            strcpy(hold_string, "");

        twr_module_lcd_draw_string(22, 116, hold_string, true);

        //hold right text
        strcpy(hold_string, "reset conn");

        twr_module_lcd_draw_string(69, 116, hold_string, true);
    }
    else if(page_index == 2) { //calibration

        //left btn
        if(!calibration_task_id)
            strcpy(hold_string, "calibrate");
        else if(is_joined)
            strcpy(hold_string, "stop cal");

        twr_module_lcd_draw_string(22, 116, hold_string, true);
    }
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
        twr_module_lcd_draw_string(25, 55, "Loading CO2", true);
    }
    else {

        twr_module_lcd_set_font(&twr_font_ubuntu_33);
        snprintf(str, sizeof(str), "%.0f", avg_val);
        twr_module_lcd_draw_string(25, 42, str, true);

        twr_module_lcd_set_font(&twr_font_ubuntu_15);
        twr_module_lcd_draw_string(61, 71, "CO2 ppm", true);
    }

    //battery lower left
    avg_val = NAN;
    twr_data_stream_get_average(&sm_battery_pct, &avg_val);

    if(!isnan(avg_val)) {

        //big rectangle
        twr_module_lcd_draw_rectangle(7, 98, 25, 108, true);

        //small line on right
        twr_module_lcd_draw_line(27, 100, 27, 106, true);

        //10 lines inside (for loop)
        int num_lines = (int) (avg_val / 6.666); //15 lines max
        for (int i=1; i <= num_lines; i++) {

            twr_module_lcd_draw_line(8+i, 100, 8+i, 106, true);
        }
    }

    //lora wifi symbol status on lower right
    if(!converted_images) {

        endian_correct_img_data(&connected_img);
        endian_correct_img_data(&disconnected_img);

        converted_images = true;
    }

    if(is_connected)
        twr_module_lcd_draw_image(100, 91, &connected_img);
    else
        twr_module_lcd_draw_image(100, 92, &disconnected_img);
}

void renderLora() {

    char status_string[50];
    char status_val[40];
    char status_val2[30];

    twr_module_lcd_set_font(&twr_font_ubuntu_24);
    twr_module_lcd_draw_string(5, 12, "LoRaWAN", true);

    //TODO 2d barcode

    //status strings
    twr_module_lcd_set_font(&twr_font_ubuntu_11);

    //Dev EUI
    twr_cmwx1zzabz_get_deveui(&lora, status_val);
    snprintf(status_string, sizeof(status_string), "DevEUI %s", status_val);
    twr_module_lcd_draw_string(5, 40, status_string, true);

    //App EUI
    twr_cmwx1zzabz_get_appeui(&lora, status_val);
    snprintf(status_string, sizeof(status_string), "AppEUI %s", status_val);
    twr_module_lcd_draw_string(5, 50, status_string, true);

    //App Key
    twr_module_lcd_draw_string(5, 60, "AppKey", true);

    twr_cmwx1zzabz_get_appkey(&lora, status_val);

    strncpy(status_val2, status_val, 20);
    status_val2[20] = '\0';
    snprintf(status_string, sizeof(status_string), "%s", status_val2);
    twr_module_lcd_draw_string(5, 70, status_string, true);

    strncpy(status_val2, &status_val[20], 12);
    status_val2[12] = '\0';
    snprintf(status_string, sizeof(status_string), "%s", status_val2);
    twr_module_lcd_draw_string(5, 80, status_string, true);    

    if(!is_connected && !is_joined) {
        
        twr_module_lcd_set_font(&twr_font_ubuntu_15);
        twr_module_lcd_draw_string(5, 90, "Disconnected", true);
    }
    else if(is_joined && !is_connected) {

        twr_module_lcd_set_font(&twr_font_ubuntu_15);
        twr_module_lcd_draw_string(5, 90, "Connecting", true);  
    }
    else {

        if(snr && rssi) {

            twr_module_lcd_set_font(&twr_font_ubuntu_11);
            
            snprintf(status_string, sizeof(status_string), "RSSI %d  SNR %d", 
                rssi, snr);

            twr_module_lcd_draw_string(5, 90, status_string, true);
        }

        if(margin && gateway_count) {

            twr_module_lcd_set_font(&twr_font_ubuntu_11);
            
            snprintf(status_string, sizeof(status_string), "Gateways %d  Margin %d", 
               gateway_count, margin);

            twr_module_lcd_draw_string(5, 1000, status_string, true);
        }        
    }
}

void renderCalibration() {

    char status_string[50];
    char status_val[30];

    twr_module_lcd_set_font(&twr_font_ubuntu_15);
    twr_module_lcd_draw_string(5, 12, "Calibration", true);
}