#include <application.h>

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

bc_soil_sensor_t soil_sensor;

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_set_mode(&led, BC_LED_MODE_TOGGLE);
    }
}

void soil_sensor_event_handler(bc_soil_sensor_t *self, bc_soil_sensor_event_t event, void *event_param)
{
    if (event == BC_SOIL_SENSOR_EVENT_ERROR_THERMOMETER)
    {
        bc_log_error("BC_SOIL_SENSOR_EVENT_ERROR_THERMOMETER");
    }
    else if (event == BC_SOIL_SENSOR_EVENT_ERROR_MOISTURE)
    {
        bc_log_error("BC_SOIL_SENSOR_EVENT_ERROR_MOISTURE");
    }
    else if (event == BC_SOIL_SENSOR_EVENT_UPDATE_THERMOMETER)
    {
        float temperature = NAN;

        bc_soil_sensor_get_temperature_celsius(self, &temperature);

        bc_log_info("temperature %.2f °C", temperature);
    }
    else if (event == BC_SOIL_SENSOR_EVENT_UPDATE_MOISTURE)
    {
        float moisture = NAN;

        bc_soil_sensor_get_moisture(self, &moisture);

        bc_log_info("moisture %.1f %", moisture);
    }

}

void application_init(void)
{
    bc_log_init(BC_LOG_LEVEL_DEBUG, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    bc_soil_sensor_init(&soil_sensor, 0x00);
    bc_soil_sensor_set_event_handler(&soil_sensor, soil_sensor_event_handler, NULL);
    bc_soil_sensor_set_update_interval_all_sensors(&soil_sensor, 1000);
}
