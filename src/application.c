/*
TODO
1. update cur_time with unix time stamp when receiving downlink (adjust for transmission counter array)
2. convert unix timestamp to struct tm + twr_rtc_set_datetime

2a. hold and click animation on bottom row
2b. troubleshoot lora connectivity issues upon restart

3. ubuntu font between 15 and 24 - 20 for temp
4. display buttons (context sensitive to page num)
5. Calibration page with cut status + cancel
6. Lora page + config params at top

7. ABC calibration every 7 days with enable flag at top and other configs

8. Last 2 bytes of downlink can schedule things like calibrate, homing message, cur location / dev name

*/

#include <application.h>
#include <at.h>
#include <lcd_screens.h>

#define SEND_DATA_INTERVAL          (1 * 60 * 1000)
#define MEASURE_INTERVAL            (1 * 60 * 1000)
#define MEASURE_INTERVAL_BAROMETER  (5 * 60 * 1000)
#define MEASURE_INTERVAL_CO2        (2 * 60 * 1000)

#define CALIBRATION_START_DELAY (30 * 1000)
#define CALIBRATION_MEASURE_INTERVAL (30 * 1000)

#define CALIBRATION_NUM_SAMPLES 32 

#define MAX_PAGE_INDEX 2
#define PAGE_INDEX_MENU -1

// LED instance
twr_led_t led;
// Button instance
//twr_button_t button;
// Lora instance
twr_cmwx1zzabz_t lora;
// Accelerometer instance
twr_lis2dh12_t lis2dh12;
twr_dice_t dice;
// PIR instance
//twr_module_pir_t pir;

uint8_t transmission_counter = 0; //for syncing uplinks to downlinks

//leds on lcd display
//twr_led_driver_t *lcd_led;

uint32_t pir_motion_count = 0;

uint8_t lora_recv_buffer[6]; //6 bytes for any received data
uint32_t cur_time = 0; //unix time

static void lcd_page_render();
static void lcd_render_every_60_secs();
void lcd_event_handler(twr_module_lcd_event_t event, void *event_param);
bool at_calibration(void);

static struct
{
    twr_tick_t next_update;
    bool mqtt;

} lcd;

static int page_index = 0;

TWR_DATA_STREAM_FLOAT_BUFFER(sm_voltage_buffer, 8)
TWR_DATA_STREAM_FLOAT_BUFFER(sm_battery_pct_buffer, 8)
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_humidity_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_illuminance_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_pressure_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL_BAROMETER))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_co2_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL_CO2))
TWR_DATA_STREAM_INT_BUFFER(sm_orientation_buffer, 3)

twr_data_stream_t sm_voltage;
twr_data_stream_t sm_battery_pct;
twr_data_stream_t sm_temperature;
twr_data_stream_t sm_humidity;
twr_data_stream_t sm_illuminance;
twr_data_stream_t sm_pressure;
twr_data_stream_t sm_co2;
twr_data_stream_t sm_orientation;

/*
static const struct
{
    char *name0;
    char *format0;
    twr_data_stream_t *value0;
    char *unit0;

    char *name1;
    char *format1;
    twr_data_stream_t *value1;
    char *unit1;

} pages[] = {
    {"Temperature   ", "%.1f", &sm_temperature, "\xb0" "F",
     "CO2           ", "%.0f", &sm_co2, "ppm"},

    {"Battery %     ", "%.0f", &sm_battery_pct, "%",
     "Voltage       ", "%.2f", &sm_voltage, "V"},
};
*/
static const struct {
    void (*renderFunction)();
} sections[] = {
    { &renderMeasurements },
    { &renderCalibration },
    { &renderLora }
};

twr_scheduler_task_id_t battery_measure_task_id;
twr_scheduler_task_id_t lcd_task_id;

enum {
    HEADER_BOOT         = 0x00,
    HEADER_UPDATE       = 0x01,
    HEADER_BUTTON_CLICK = 0x02,
    HEADER_BUTTON_HOLD  = 0x03,

} header = HEADER_BOOT;

twr_scheduler_task_id_t calibration_task_id = 0;
int calibration_counter;

void calibration_task(void *param);

void calibration_start()
{

    calibration_counter = CALIBRATION_NUM_SAMPLES;

    twr_led_set_mode(&led, TWR_LED_MODE_BLINK_FAST);
    //twr_led_set_mode(lcd_led, TWR_LED_MODE_BLINK_FAST);

    calibration_task_id = twr_scheduler_register(calibration_task, NULL, twr_tick_get() + CALIBRATION_START_DELAY);
    twr_atci_printf("$CO2_CALIBRATION: \"START\"");

    twr_scheduler_plan_now(lcd_task_id); //update lcd
}

void calibration_stop()
{
    if (!calibration_task_id)
    {
        return;
    }

    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    twr_scheduler_unregister(calibration_task_id);
    calibration_task_id = 0;

    twr_scheduler_plan_now(lcd_task_id); //update lcd

    twr_module_co2_set_update_interval(MEASURE_INTERVAL_CO2);
    twr_atci_printf("$CO2_CALIBRATION: \"STOP\"");
    
}

void calibration_task(void *param)
{
    (void) param;

    twr_led_set_mode(&led, TWR_LED_MODE_BLINK_SLOW);
   // twr_led_set_mode(lcd_led, TWR_LED_MODE_BLINK_SLOW);

    twr_atci_printf("$CO2_CALIBRATION_COUNTER: \"%d\"", calibration_counter);

    twr_module_co2_set_update_interval(CALIBRATION_MEASURE_INTERVAL);
    twr_module_co2_calibration(TWR_LP8_CALIBRATION_BACKGROUND_FILTERED);

    if (!--calibration_counter)
        calibration_stop();

    twr_scheduler_plan_current_relative(CALIBRATION_MEASURE_INTERVAL);
}

static void lcd_page_render()
{
    twr_atci_printf("LCD RENDER");

    twr_system_pll_enable();

    twr_module_lcd_clear();

    renderDateTime();
    renderBtns();

    sections[page_index].renderFunction();

    /*

    if ((page_index <= MAX_PAGE_INDEX) && (page_index != PAGE_INDEX_MENU))
    {
        twr_module_lcd_set_font(&twr_font_ubuntu_15);
        twr_module_lcd_draw_string(10, 10, pages[page_index].name0, true);

        twr_module_lcd_set_font(&twr_font_ubuntu_28);

        twr_data_stream_get_average(pages[page_index].value0, &avg_val1);

        if (isnan(avg_val1)) {

            twr_module_lcd_set_font(&twr_font_ubuntu_24);

            sprintf(str, "%s", "Loading"); 
            w = twr_module_lcd_draw_string(25, 25, str, true);
        } 
        else {

            twr_module_lcd_set_font(&twr_font_ubuntu_28);

            snprintf(str, sizeof(str), pages[page_index].format0, avg_val1);
            w = twr_module_lcd_draw_string(25, 25, str, true);

            twr_module_lcd_set_font(&twr_font_ubuntu_15);
            w = twr_module_lcd_draw_string(w, 35, pages[page_index].unit0, true);
        }

        twr_module_lcd_set_font(&twr_font_ubuntu_15);
        twr_module_lcd_draw_string(10, 55, pages[page_index].name1, true);

        twr_data_stream_get_average(pages[page_index].value1, &avg_val2);

        if(!strcmp(pages[page_index].name1, "CO2           ") && calibration_task_id) {
            
            twr_module_lcd_set_font(&twr_font_ubuntu_24);

            sprintf(str, "%s", "Calibrating"); 
            w = twr_module_lcd_draw_string(5, 75, str, true);
        }

        else if (isnan(avg_val2)) {
            
            twr_module_lcd_set_font(&twr_font_ubuntu_24);

            sprintf(str, "%s", "Loading"); //LOADING
            w = twr_module_lcd_draw_string(25, 75, str, true);
        } 
        else {

            twr_module_lcd_set_font(&twr_font_ubuntu_28);

            snprintf(str, sizeof(str), pages[page_index].format1, avg_val2);
            w = twr_module_lcd_draw_string(25, 75, str, true);

            twr_module_lcd_set_font(&twr_font_ubuntu_15);
            twr_module_lcd_draw_string(w, 85, pages[page_index].unit1, true);
        }
    }
    */

    /*
    snprintf(str, sizeof(str), "%d/%d", page_index + 1, MAX_PAGE_INDEX + 1);
    twr_module_lcd_set_font(&twr_font_ubuntu_13);
    twr_module_lcd_draw_string(55, 115, str, true);
    */
    twr_system_pll_disable();
}

//scheduler to render lcd every 60 seconds no matter what for clock purpii
static void lcd_render_every_60_secs() {
    
    twr_scheduler_plan_now(lcd_task_id);

    twr_scheduler_plan_current_from_now(60*1000);
}

//button handler
void lcd_event_handler(twr_module_lcd_event_t event, void *event_param)
{
    (void) event_param;

    if (event == TWR_MODULE_LCD_EVENT_LEFT_CLICK)
    {

        /*
        header = HEADER_BUTTON_CLICK;

        
         */
        /*
        if ((page_index != PAGE_INDEX_MENU))
        {
            // Key previous page
            page_index--;
            if (page_index < 0)
            {
                page_index = MAX_PAGE_INDEX;
                menu_item = 0;
            }
        }
        else
        {
            // Key menu down
            menu_item++;
            if (menu_item > 4)
            {
                menu_item = 0;
            }
        }
        */

        page_index--;
        page_index = page_index < 0 ? MAX_PAGE_INDEX : page_index;

        //static uint16_t left_event_count = 0;
        //left_event_count++;
        //twr_radio_pub_event_count(TWR_RADIO_PUB_EVENT_LCD_BUTTON_LEFT, &left_event_count);

        twr_scheduler_plan_now(lcd_task_id);
    }
    else if(event == TWR_MODULE_LCD_EVENT_RIGHT_CLICK)
    {

        /*
        if ((page_index != PAGE_INDEX_MENU) || (menu_item == 0))
        {
            // Key next page
            page_index++;
            if (page_index > MAX_PAGE_INDEX)
            {
                page_index = 0;
            }
            if (page_index == PAGE_INDEX_MENU)
            {
                menu_item = 0;
            }
        }
        */

        page_index++;
        page_index = page_index > MAX_PAGE_INDEX ? 0 : page_index;

        //static uint16_t right_event_count = 0;
        //right_event_count++;

        twr_scheduler_plan_now(lcd_task_id);
    }
    else if(event == TWR_MODULE_LCD_EVENT_LEFT_HOLD)
    {
        
         twr_scheduler_plan_now(0); //should send lora data

        /*
        static int left_hold_event_count = 0;
        left_hold_event_count++;
        twr_radio_pub_int("push-button/lcd:left-hold/event-count", &left_hold_event_count);

        twr_led_pulse(&led, 100);
        */
    }
    else if(event == TWR_MODULE_LCD_EVENT_RIGHT_HOLD)
    {

        at_calibration();
        /*
        static int right_hold_event_count = 0;
        right_hold_event_count++;
        twr_radio_pub_int("push-button/lcd:right-hold/event-count", &right_hold_event_count);

        twr_led_pulse(&led, 100);
        */

    }
    else if(event == TWR_MODULE_LCD_EVENT_BOTH_HOLD)
    {

       

        /*
        static int both_hold_event_count = 0;
        both_hold_event_count++;
        twr_radio_pub_int("push-button/lcd:both-hold/event-count", &both_hold_event_count);

        twr_led_pulse(&led, 100);
        */
    }
}

void lcd_task(void *param)
{
    if (!twr_module_lcd_is_ready())
    {
        return;
    }

    if (!lcd.mqtt)
    {
        lcd_page_render();
    }
    else
    {
        twr_scheduler_plan_current_relative(500);
    }

    twr_module_lcd_update();
}


/*
void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    if (event == TWR_BUTTON_EVENT_CLICK)
    {
        header = HEADER_BUTTON_CLICK;

        twr_scheduler_plan_now(0);
    }
    else if (event == TWR_BUTTON_EVENT_HOLD)
    {
        if (!calibration_task_id)
        {
            calibration_start();
        }
        else
        {
            calibration_stop();
        }
    }
}
*/

void pir_event_handler(twr_module_pir_t *self, twr_module_pir_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_MODULE_PIR_EVENT_MOTION)
    {
        //twr_led_pulse(&led, 50);
        pir_motion_count++;
    }
}

void co2_module_event_handler(twr_module_co2_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    twr_atci_printf("CO2 MEASUREMENT COMPLETE");

    float value;

    if (twr_module_co2_get_concentration_ppm(&value))
    {
        twr_data_stream_feed(&sm_co2, &value);

        if (calibration_task_id)
        {
            twr_atci_printf("$CO2_CALIBRATION_CO2_VALUE: \"%f\"", value);
        }

        twr_scheduler_plan_now(lcd_task_id); //update lcd
    }
    else
    {
        twr_data_stream_reset(&sm_co2);
    }
}

void climate_module_event_handler(twr_module_climate_event_t event, void *event_param)
{
    float value = NAN;

/*
     //for when climate module arrives
    if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_THERMOMETER)
    {
    twr_module_climate_get_temperature_celsius(&value);


    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        twr_tmp112_get_temperature_celsius(event, &value);

        twr_data_stream_feed(&sm_temperature, &value);
    }
    else */if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_HYGROMETER)
    {
        twr_module_climate_get_humidity_percentage(&value);

        twr_data_stream_feed(&sm_humidity, &value);
    }
    else if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_LUX_METER)
    {
        twr_module_climate_get_illuminance_lux(&value);

        twr_data_stream_feed(&sm_illuminance, &value);
    }
    else if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_BAROMETER)
    {
        twr_module_climate_get_pressure_pascal(&value);

        twr_data_stream_feed(&sm_pressure, &value);
    }

    //twr_scheduler_plan_now(lcd_task_id); //update lcd
}

//co2 temp module handler
void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    float temp = NAN;

    if (event != TWR_TMP112_EVENT_UPDATE)
    {
        return;
    }

    if (twr_tmp112_get_temperature_fahrenheit(self, &temp))
    {
        twr_data_stream_feed(&sm_temperature, &temp);

        twr_scheduler_plan_now(lcd_task_id); //update lcd
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    twr_atci_printf("BATTERY EVENT HAPPENED%d", event);

    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage = NAN;
        twr_module_battery_get_voltage(&voltage);
        twr_data_stream_feed(&sm_voltage, &voltage);

        int battery_pct;
        twr_module_battery_get_charge_level(&battery_pct);
        float battery_pct_float = (float) battery_pct;

        twr_data_stream_feed(&sm_battery_pct, &battery_pct_float); 

        twr_scheduler_plan_now(lcd_task_id); //update lcd    
    }
}

void battery_measure_task(void *param)
{
    if (!twr_module_battery_measure())
    {
        twr_scheduler_plan_current_now();
    }
}

void lis2dh12_event_handler(twr_lis2dh12_t *self, twr_lis2dh12_event_t event, void *event_param)
{
    if (event == TWR_LIS2DH12_EVENT_UPDATE)
    {
        twr_lis2dh12_result_g_t g;

        if (twr_lis2dh12_get_result_g(self, &g))
        {
            twr_dice_feed_vectors(&dice, g.x_axis, g.y_axis, g.z_axis);

            int orientation = (int) twr_dice_get_face(&dice);

            twr_data_stream_feed(&sm_orientation, &orientation);
        }

        //twr_scheduler_plan_now(lcd_task_id); //update lcd
    }
}

void lora_callback(twr_cmwx1zzabz_t *self, twr_cmwx1zzabz_event_t event, void *event_param)
{
    if (event == TWR_CMWX1ZZABZ_EVENT_ERROR)
    {
        twr_led_set_mode(&led, TWR_LED_MODE_BLINK_FAST);

        twr_atci_printf("$LORA ERROR");
    }
    else if (event == TWR_CMWX1ZZABZ_EVENT_SEND_MESSAGE_START)
    {
        twr_led_set_mode(&led, TWR_LED_MODE_ON);

        twr_scheduler_plan_relative(battery_measure_task_id, 20);

        twr_atci_printf("$MESSAGE SENT START");
    }
    else if (event == TWR_CMWX1ZZABZ_EVENT_SEND_MESSAGE_DONE)
    {
        twr_led_set_mode(&led, TWR_LED_MODE_OFF);
        twr_atci_printf("$MESSAGE SEND DONE");
    }
    else if (event == TWR_CMWX1ZZABZ_EVENT_READY)
    {
        twr_led_set_mode(&led, TWR_LED_MODE_OFF);
    }
    else if (event == TWR_CMWX1ZZABZ_EVENT_JOIN_SUCCESS)
    {
        twr_atci_printf("$JOIN_OK");
    }
    else if (event == TWR_CMWX1ZZABZ_EVENT_JOIN_ERROR)
    {
        twr_atci_printf("$JOIN_ERROR");
    }

    else if(event == TWR_CMWX1ZZABZ_EVENT_MESSAGE_RECEIVED) {

        twr_cmwx1zzabz_get_received_message_data(self, lora_recv_buffer, 6);

        twr_atci_printf("$RECEIVED DATA");

        //todo show data
    }

    //twr_scheduler_plan_now(lcd_task_id); //update lcd
}

bool at_send(void)
{
    twr_scheduler_plan_now(0);

    return true;
}

bool at_calibration(void)
{
    if (calibration_task_id)
    {
       calibration_stop();
    }
    else
    {
        calibration_start();
    }

    return true;
}

bool at_status(void)
{
    float value_avg = NAN;

    static const struct {
        twr_data_stream_t *stream;
        const char *name;
        int precision;
    } values[] = {
            {&sm_voltage, "Voltage", 1},
            {&sm_battery_pct, "Battery Pct", 1},
            {&sm_temperature, "Temperature", 1},
            {&sm_humidity, "Humidity", 1},
            {&sm_illuminance, "Illuminance", 1},
            {&sm_pressure, "Pressure", 0},
            {&sm_co2, "CO2", 0},
    };

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
    {
        value_avg = NAN;

        if (twr_data_stream_get_average(values[i].stream, &value_avg))
        {
            twr_atci_printf("$STATUS: \"%s\",%.*f", values[i].name, values[i].precision, value_avg);
        }
        else
        {
            twr_atci_printf("$STATUS: \"%s\",", values[i].name);
        }
    }

    int orientation;

    if (twr_data_stream_get_median(&sm_orientation, &orientation))
    {
        twr_atci_printf("$STATUS: \"Orientation\",%d", orientation);
    }
    else
    {
        twr_atci_printf("$STATUS: \"Orientation\",", orientation);
    }

    twr_atci_printf("$STATUS: \"PIR Motion count\",%d", pir_motion_count);

    return true;
}

_Bool link_check(void) {
    if(twr_cmwx1zzabz_link_check(&lora))
        twr_atci_printf("LINK GOOD");
    else
        twr_atci_printf("LINK BAD");

    return true;
}

void application_init(void)
{
    twr_data_stream_init(&sm_voltage, 1, &sm_voltage_buffer);
    twr_data_stream_init(&sm_battery_pct, 1, &sm_battery_pct_buffer);
    twr_data_stream_init(&sm_temperature, 1, &sm_temperature_buffer);
    twr_data_stream_init(&sm_humidity, 1, &sm_humidity_buffer);
    twr_data_stream_init(&sm_illuminance, 1, &sm_illuminance_buffer);
    twr_data_stream_init(&sm_pressure, 1, &sm_pressure_buffer);
    twr_data_stream_init(&sm_co2, 1, &sm_co2_buffer);
    twr_data_stream_init(&sm_orientation, 1, &sm_orientation_buffer);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_BLINK);

    // Initialize button
    //twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    //twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize climate module
    /*
    twr_module_climate_init();
    twr_module_climate_set_event_handler(climate_module_event_handler, NULL);
    twr_module_climate_set_update_interval_thermometer(MEASURE_INTERVAL);
    twr_module_climate_set_update_interval_hygrometer(MEASURE_INTERVAL);
    twr_module_climate_set_update_interval_lux_meter(MEASURE_INTERVAL);
    twr_module_climate_set_update_interval_barometer(MEASURE_INTERVAL_BAROMETER);
    */

    // Initialize PIR Module
    /*
    twr_module_pir_init(&pir);
    twr_module_pir_set_event_handler(&pir, pir_event_handler, NULL);
    */

    //initialize temp monitor from co2 board
    static twr_tmp112_t temperature;
    twr_tmp112_init(&temperature, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&temperature, tmp112_event_handler, NULL);
    twr_tmp112_set_update_interval(&temperature, MEASURE_INTERVAL);      

    // Initilize CO2
    twr_module_co2_init();
    twr_module_co2_set_update_interval(MEASURE_INTERVAL_CO2);
    twr_module_co2_set_event_handler(co2_module_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    battery_measure_task_id = twr_scheduler_register(battery_measure_task, NULL, 2020);

    //initalize LCD + button
    //memset(&values, 0xff, sizeof(values)); //??????
    twr_module_lcd_init();
    twr_module_lcd_set_event_handler(lcd_event_handler, NULL);
    twr_module_lcd_set_button_hold_time(1000);   
    lcd_task_id = twr_scheduler_register(lcd_task, NULL, 2020);
    lcd_render_every_60_secs();

    //const lcd_led = twr_module_lcd_get_led_driver();
    //twr_led_init(lcd_led);

    twr_dice_init(&dice, TWR_DICE_FACE_UNKNOWN);

    twr_lis2dh12_init(&lis2dh12, TWR_I2C_I2C0, 0x19);
    twr_lis2dh12_set_resolution(&lis2dh12, TWR_LIS2DH12_RESOLUTION_8BIT);

    twr_lis2dh12_set_event_handler(&lis2dh12, lis2dh12_event_handler, NULL);
    twr_lis2dh12_set_update_interval(&lis2dh12, MEASURE_INTERVAL);

    // Initialize lora module + set variables
    twr_cmwx1zzabz_init(&lora, TWR_UART_UART1);
    twr_cmwx1zzabz_set_event_handler(&lora, lora_callback, NULL);
    twr_cmwx1zzabz_set_class(&lora, TWR_CMWX1ZZABZ_CONFIG_CLASS_A);

    //3 key variables - TODO expose through constants at top
    twr_cmwx1zzabz_set_mode(&lora, TWR_CMWX1ZZABZ_CONFIG_MODE_OTAA);
    twr_cmwx1zzabz_set_nwk_public(&lora, 1);
    twr_cmwx1zzabz_set_band(&lora, TWR_CMWX1ZZABZ_CONFIG_BAND_US915);

    //join lora network
    twr_cmwx1zzabz_join(&lora);

    //TODO set link variable to result of this + regularly perform link checks or success of send

    

    // Initialize AT command interface
    at_init(&led, &lora);
    static const twr_atci_command_t commands[] = {
            AT_LORA_COMMANDS,
            {"$SEND", at_send, NULL, NULL, NULL, "Immediately send packet"},
            {"$CALIBRATION", at_calibration, NULL, NULL, NULL, "Perform Co2 calibration"},
            {"$STATUS", at_status, NULL, NULL, NULL, "Show status"},
            {"$LINKCHECK", link_check, NULL, NULL, NULL, "Show status"},
            AT_LED_COMMANDS,
            TWR_ATCI_COMMAND_CLAC,
            TWR_ATCI_COMMAND_HELP
    };
    twr_atci_init(commands, TWR_ATCI_COMMANDS_LENGTH(commands));

    twr_scheduler_plan_current_relative(10 * 1000);
}

void application_task(void)
{
    if (!twr_cmwx1zzabz_is_ready(&lora))
    {
        twr_scheduler_plan_current_relative(100);

        return;
    }

    static uint8_t buffer[16];

    memset(buffer, 0xff, sizeof(buffer));

    buffer[0] = header;

    float voltage_avg = NAN;

    twr_data_stream_get_average(&sm_voltage, &voltage_avg);

    if (!isnan(voltage_avg))
    {
        buffer[1] = ceil(voltage_avg * 10.f);
    }

    
    float battery_pct_avg = NAN;

    twr_data_stream_get_average(&sm_battery_pct, &battery_pct_avg);
    
    if (!isnan(battery_pct_avg))
    {
        buffer[2] = (int) battery_pct_avg;
    }

    /*
    int orientation;

    if (twr_data_stream_get_median(&sm_orientation, &orientation))
    {
        buffer[2] = orientation;
    }
    */

    float temperature_avg = NAN;

    twr_data_stream_get_average(&sm_temperature, &temperature_avg);

    if (!isnan(temperature_avg))
    {
        int16_t temperature_i16 = (int16_t) (temperature_avg * 10.f);

        buffer[3] = temperature_i16 >> 8;
        buffer[4] = temperature_i16;
    }

    float humidity_avg = NAN;

    twr_data_stream_get_average(&sm_humidity, &humidity_avg);

    if (!isnan(humidity_avg))
    {
        buffer[5] = humidity_avg * 2;
    }

    float illuminance_avg = NAN;

    twr_data_stream_get_average(&sm_illuminance, &illuminance_avg);

    if (!isnan(illuminance_avg))
    {
        if (illuminance_avg > 65534)
        {
            illuminance_avg = 65534;
        }

        uint16_t value = (uint16_t) illuminance_avg;
        buffer[6] = value >> 8;
        buffer[7] = value;
    }

    float pressure_avg = NAN;

    twr_data_stream_get_average(&sm_pressure, &pressure_avg);

    if (!isnan(pressure_avg))
    {
        uint16_t value = pressure_avg / 2.f;
        buffer[8] = value >> 8;
        buffer[9] = value;
    }

    float co2_avg = NAN;

    twr_data_stream_get_average(&sm_co2, &co2_avg);

    if (!isnan(co2_avg))
    {
        uint16_t value = (int) co2_avg;
        buffer[10] = value >> 8;
        buffer[11] = value;
    }

    //monotonic counter
    buffer[12] = transmission_counter++;

    if(!twr_cmwx1zzabz_send_message(&lora, buffer, sizeof(buffer)))
        twr_atci_printf("PACKET NOT SENT");

    static char tmp[sizeof(buffer) * 2 + 1];
    for (size_t i = 0; i < sizeof(buffer); i++)
    {
        sprintf(tmp + i * 2, "%02x", buffer[i]);
    }

    twr_atci_printf("$SEND: %s", tmp);

    header = HEADER_UPDATE;

    twr_scheduler_plan_current_relative(SEND_DATA_INTERVAL);
}
