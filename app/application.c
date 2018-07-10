#include <application.h>

#define SERVICE_INTERVAL_INTERVAL           (60 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL             (60 * 60 * 1000)
#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL   (15 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE        0.5f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL  (30 * 1000)
#define MOISTURE_PUB_NO_CHANGE_INTEVAL      (60 * 60 * 1000)
#define MOISTURE_PUB_VALUE_CHANGE           5

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

bc_soil_sensor_t soil_sensor;
struct
{
    float value;
    bc_tick_t next_pub;

} soil_sensor_temperature = { .next_pub = 0 };
struct
{
    int value;
    bc_tick_t next_pub;

} soil_sensor_moisture = { .next_pub = 0 };

// Thermometer instance
bc_tmp112_t tmp112;
struct
{
    float value;
    bc_tick_t next_pub;

} tmp112_temperature = { .next_pub = 0 };

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_set_mode(&led, BC_LED_MODE_TOGGLE);
    }
}

void soil_sensor_event_handler(bc_soil_sensor_t *self, bc_soil_sensor_event_t event, void *event_param)
{
    if (event == BC_SOIL_SENSOR_EVENT_UPDATE_THERMOMETER)
    {
        float temperature = NAN;

        if (!bc_soil_sensor_get_temperature_celsius(self, &temperature))
        {
            bc_log_error("bc_soil_sensor_get_temperature_celsius");

            return;
        }

        if ((fabsf(temperature - soil_sensor_temperature.value) >= TEMPERATURE_PUB_VALUE_CHANGE) || (soil_sensor_temperature.next_pub < bc_tick_get()))
        {
            bc_radio_pub_float("soil-sensor/0000000000000000/temperature", &temperature);

            soil_sensor_temperature.value = temperature;

            soil_sensor_temperature.next_pub = bc_tick_get() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
        }

        bc_log_info("soil-sensor temperature %.1f °C", temperature);
    }
    else if (event == BC_SOIL_SENSOR_EVENT_UPDATE_MOISTURE)
    {
        int moisture;

        if (!bc_soil_sensor_get_moisture(self, &moisture))
        {
            bc_log_error("bc_soil_sensor_get_moisture");

            return;
        }

        if ((abs(moisture - soil_sensor_moisture.value) >= MOISTURE_PUB_VALUE_CHANGE) || (soil_sensor_moisture.next_pub < bc_tick_get()))
        {
            bc_radio_pub_int("soil-sensor/0000000000000000/moisture", &moisture);

            soil_sensor_moisture.value = moisture;

            soil_sensor_moisture.next_pub = bc_tick_get() + MOISTURE_PUB_NO_CHANGE_INTEVAL;
        }

        bc_log_info("soil-sensor moisture %d %", moisture);
    }
    else if (event == BC_SOIL_SENSOR_EVENT_ERROR_THERMOMETER)
    {
        bc_log_error("BC_SOIL_SENSOR_EVENT_ERROR_THERMOMETER");
    }
    else if (event == BC_SOIL_SENSOR_EVENT_ERROR_MOISTURE)
    {
        bc_log_error("BC_SOIL_SENSOR_EVENT_ERROR_MOISTURE");
    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;

    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_radio_pub_battery(&voltage);
        }
    }
}

void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    float value;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        if (bc_tmp112_get_temperature_celsius(self, &value))
        {
            if ((fabsf(value - tmp112_temperature.value) >= TEMPERATURE_PUB_VALUE_CHANGE) || (tmp112_temperature.next_pub < bc_tick_get()))
            {
                bc_radio_pub_temperature(BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE, &value);

                tmp112_temperature.value = value;

                tmp112_temperature.next_pub = bc_tick_get() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void switch_to_normal_mode_task(void *param)
{
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_NORMAL_INTERVAL);

    bc_soil_sensor_set_update_interval_all_sensors(&soil_sensor, 60 * 1000);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

void application_init(void)
{
    bc_log_init(BC_LOG_LEVEL_DEBUG, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize soil sensor
    bc_soil_sensor_init(&soil_sensor, 0x00);
    bc_soil_sensor_set_event_handler(&soil_sensor, soil_sensor_event_handler, NULL);
    bc_soil_sensor_set_update_interval_all_sensors(&soil_sensor, 3000);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize thermometer sensor on core module
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_SERVICE_INTERVAL);

    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_pairing_request("kit-wireless-soil-sensor", VERSION);

    bc_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);

    bc_led_pulse(&led, 2000);
}
