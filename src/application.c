#include <application.h>
#include <at.h>

#define SEND_DATA_INTERVAL          (15 * 60 * 1000)
#define MEASURE_INTERVAL            (1 * 60 * 1000)
#define MEASURE_INTERVAL_BAROMETER  (5 * 60 * 1000)
#define MEASURE_INTERVAL_CO2        (5 * 60 * 1000)

#define CALIBRATION_START_DELAY (15 * 60 * 1000)
#define CALIBRATION_MEASURE_INTERVAL (2 * 60 * 1000)

// LED instance
twr_led_t led;
// Button instance
twr_button_t button;
// Lora instance
twr_cmwx1zzabz_t lora;
// Accelerometer instance
twr_lis2dh12_t lis2dh12;
twr_dice_t dice;
// PIR instance
twr_module_pir_t pir;

uint32_t pir_motion_count = 0;

TWR_DATA_STREAM_FLOAT_BUFFER(sm_voltage_buffer, 8)
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_humidity_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_illuminance_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_pressure_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL_BAROMETER))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_co2_buffer, (SEND_DATA_INTERVAL / MEASURE_INTERVAL_BAROMETER))
TWR_DATA_STREAM_INT_BUFFER(sm_orientation_buffer, 3)

twr_data_stream_t sm_voltage;
twr_data_stream_t sm_temperature;
twr_data_stream_t sm_humidity;
twr_data_stream_t sm_illuminance;
twr_data_stream_t sm_pressure;
twr_data_stream_t sm_co2;
twr_data_stream_t sm_orientation;

twr_scheduler_task_id_t battery_measure_task_id;

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
    calibration_counter = 32;

    twr_led_set_mode(&led, TWR_LED_MODE_BLINK_FAST);
    calibration_task_id = twr_scheduler_register(calibration_task, NULL, twr_tick_get() + CALIBRATION_START_DELAY);
    twr_atci_printf("$CO2_CALIBRATION: \"START\"");
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

    twr_module_co2_set_update_interval(MEASURE_INTERVAL_CO2);
    twr_atci_printf("$CO2_CALIBRATION: \"STOP\"");
}

void calibration_task(void *param)
{
    (void) param;

    twr_led_set_mode(&led, TWR_LED_MODE_BLINK_SLOW);

    twr_atci_printf("$CO2_CALIBRATION_COUNTER: \"%d\"", calibration_counter);

    twr_module_co2_set_update_interval(CALIBRATION_MEASURE_INTERVAL);
    twr_module_co2_calibration(TWR_LP8_CALIBRATION_BACKGROUND_FILTERED);

    calibration_counter--;

    if (calibration_counter == 0)
    {
        calibration_stop();
    }

    twr_scheduler_plan_current_relative(CALIBRATION_MEASURE_INTERVAL);
}

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

    float value;

    if (twr_module_co2_get_concentration_ppm(&value))
    {
        twr_data_stream_feed(&sm_co2, &value);

        if (calibration_task_id)
        {
            twr_atci_printf("$CO2_CALIBRATION_CO2_VALUE: \"%f\"", value);
        }
    }
    else
    {
        twr_data_stream_reset(&sm_co2);
    }
}

void climate_module_event_handler(twr_module_climate_event_t event, void *event_param)
{
    float value = NAN;

    if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_THERMOMETER)
    {
        twr_module_climate_get_temperature_celsius(&value);

        twr_data_stream_feed(&sm_temperature, &value);
    }
    else if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_HYGROMETER)
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
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage = NAN;

        twr_module_battery_get_voltage(&voltage);

        twr_data_stream_feed(&sm_voltage, &voltage);
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
    }
}

void lora_callback(twr_cmwx1zzabz_t *self, twr_cmwx1zzabz_event_t event, void *event_param)
{
    if (event == TWR_CMWX1ZZABZ_EVENT_ERROR)
    {
        twr_led_set_mode(&led, TWR_LED_MODE_BLINK_FAST);
    }
    else if (event == TWR_CMWX1ZZABZ_EVENT_SEND_MESSAGE_START)
    {
        twr_led_set_mode(&led, TWR_LED_MODE_ON);

        twr_scheduler_plan_relative(battery_measure_task_id, 20);
    }
    else if (event == TWR_CMWX1ZZABZ_EVENT_SEND_MESSAGE_DONE)
    {
        twr_led_set_mode(&led, TWR_LED_MODE_OFF);
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

#if defined(GIT_VERSION) && defined(BUILD_DATE)
static bool at_version()
{
    twr_atci_printfln("$VER: %s built on %s", GIT_VERSION, BUILD_DATE);
    return true;
}
#endif

void application_init(void)
{
    twr_data_stream_init(&sm_voltage, 1, &sm_voltage_buffer);
    twr_data_stream_init(&sm_temperature, 1, &sm_temperature_buffer);
    twr_data_stream_init(&sm_humidity, 1, &sm_humidity_buffer);
    twr_data_stream_init(&sm_illuminance, 1, &sm_illuminance_buffer);
    twr_data_stream_init(&sm_pressure, 1, &sm_pressure_buffer);
    twr_data_stream_init(&sm_co2, 1, &sm_co2_buffer);
    twr_data_stream_init(&sm_orientation, 1, &sm_orientation_buffer);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_ON);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize climate module
    twr_module_climate_init();
    twr_module_climate_set_event_handler(climate_module_event_handler, NULL);
    twr_module_climate_set_update_interval_thermometer(MEASURE_INTERVAL);
    twr_module_climate_set_update_interval_hygrometer(MEASURE_INTERVAL);
    twr_module_climate_set_update_interval_lux_meter(MEASURE_INTERVAL);
    twr_module_climate_set_update_interval_barometer(MEASURE_INTERVAL_BAROMETER);

    // Initialize PIR Module
    twr_module_pir_init(&pir);
    twr_module_pir_set_event_handler(&pir, pir_event_handler, NULL);

    // Initilize CO2
    twr_module_co2_init();
    twr_module_co2_set_update_interval(MEASURE_INTERVAL_CO2);
    twr_module_co2_set_event_handler(co2_module_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    battery_measure_task_id = twr_scheduler_register(battery_measure_task, NULL, 2020);

    twr_dice_init(&dice, TWR_DICE_FACE_UNKNOWN);

    twr_lis2dh12_init(&lis2dh12, TWR_I2C_I2C0, 0x19);
    twr_lis2dh12_set_resolution(&lis2dh12, TWR_LIS2DH12_RESOLUTION_8BIT);

    twr_lis2dh12_set_event_handler(&lis2dh12, lis2dh12_event_handler, NULL);
    twr_lis2dh12_set_update_interval(&lis2dh12, MEASURE_INTERVAL);

    // Initialize lora module
    twr_cmwx1zzabz_init(&lora, TWR_UART_UART1);
    twr_cmwx1zzabz_set_event_handler(&lora, lora_callback, NULL);
    twr_cmwx1zzabz_set_mode(&lora, TWR_CMWX1ZZABZ_CONFIG_MODE_ABP);
    twr_cmwx1zzabz_set_class(&lora, TWR_CMWX1ZZABZ_CONFIG_CLASS_A);

    // Initialize AT command interface
    at_init(&led, &lora);
    static const twr_atci_command_t commands[] = {
            AT_LORA_COMMANDS,
            {"$SEND", at_send, NULL, NULL, NULL, "Immediately send packet"},
            {"$CALIBRATION", at_calibration, NULL, NULL, NULL, "Immediately send packet"},
            {"$STATUS", at_status, NULL, NULL, NULL, "Show status"},
            #if defined(GIT_VERSION) && defined(BUILD_DATE)
            {"$VER", at_version, NULL, NULL, NULL, "Show firmware version and build date"},
            #endif
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

    int orientation;

    if (twr_data_stream_get_median(&sm_orientation, &orientation))
    {
        buffer[2] = orientation;
    }

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

    buffer[10] = pir_motion_count >> 24;
    buffer[11] = pir_motion_count >> 16;
    buffer[12] = pir_motion_count >> 8;
    buffer[13] = pir_motion_count;

    float co2_avg = NAN;

    twr_data_stream_get_average(&sm_co2, &co2_avg);

    if (!isnan(co2_avg))
    {
        uint16_t value = co2_avg;
        buffer[14] = value >> 8;
        buffer[15] = value;
    }

    twr_cmwx1zzabz_send_message(&lora, buffer, sizeof(buffer));

    static char tmp[sizeof(buffer) * 2 + 1];
    for (size_t i = 0; i < sizeof(buffer); i++)
    {
        sprintf(tmp + i * 2, "%02x", buffer[i]);
    }

    twr_atci_printf("$SEND: %s", tmp);

    header = HEADER_UPDATE;

    twr_scheduler_plan_current_relative(SEND_DATA_INTERVAL);
}
