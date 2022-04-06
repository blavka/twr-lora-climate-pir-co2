#include "lcd_screens.h"
#include <twr.h>
#include <twr_atci.h>


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