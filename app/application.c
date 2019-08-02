#include <application.h>

#define MAX_SOIL_SENSORS                    5

#define SERVICE_MODE_INTERVAL               (15 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL             (60 * 60 * 1000)

#define TEMPERATURE_PUB_INTERVAL            (15 * 60 * 1000)
#define TEMPERATURE_PUB_DIFFERENCE          1.0f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (1 * 1000)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL  (10 * 1000)

#define SENSOR_UPDATE_SERVICE_INTERVAL      (15 * 1000)
#define SENSOR_UPDATE_NORMAL_INTERVAL       (5 * 60 * 1000)

// LED instance
bc_led_t led;
// Button instance
bc_button_t button;
// Thermometer instance
bc_tmp112_t tmp112;
// Soil sensor instance
bc_soil_sensor_t soil_sensor;
// Sensors array
bc_soil_sensor_sensor_t sensors[MAX_SOIL_SENSORS];

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    // Counters for button events
    static uint16_t button_click_count = 0;
    static uint16_t button_hold_count = 0;

    if (event == BC_BUTTON_EVENT_CLICK)
    {
        // Pulse LED for 100 milliseconds
        bc_led_pulse(&led, 100);

        // Increment press count
        button_click_count++;

        // Publish button message on radio
        bc_radio_pub_push_button(&button_click_count);
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        // Pulse LED for 250 milliseconds
        bc_led_pulse(&led, 250);

        // Increment hold count
        button_hold_count++;
        // Publish message on radio
        bc_radio_pub_event_count(BC_RADIO_PUB_EVENT_HOLD_BUTTON, &button_hold_count);
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
    // Time of next report
    static bc_tick_t tick_report = 0;

    // Last value used for change comparison
    static float last_published_temperature = NAN;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        float temperature;

        if (bc_tmp112_get_temperature_celsius(self, &temperature))
        {
            // Implicitly do not publish message on radio
            bool publish = false;

            // Is time up to report temperature?
            if (bc_tick_get() >= tick_report)
            {
                // Publish message on radio
                publish = true;
            }

            // Is temperature difference from last published value significant?
            if (fabsf(temperature - last_published_temperature) >= TEMPERATURE_PUB_DIFFERENCE)
            {
                // Publish message on radio
                publish = true;
            }

            if (publish)
            {
                // Publish temperature message on radio
                bc_radio_pub_temperature(BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE, &temperature);

                // Schedule next temperature report
                tick_report = bc_tick_get() + TEMPERATURE_PUB_INTERVAL;

                // Remember last published value
                last_published_temperature = temperature;
            }
        }
    }
}

void soil_sensor_event_handler(bc_soil_sensor_t *self, uint64_t device_address, bc_soil_sensor_event_t event, void *event_param)
{
    static char topic[64];

    if (event == BC_SOIL_SENSOR_EVENT_UPDATE)
    {
        int index = bc_soil_sensor_get_index_by_device_address(self, device_address);

        if (index < 0)
        {
            return;
        }

        float temperature;

        if (bc_soil_sensor_get_temperature_celsius(self, device_address, &temperature))
        {
            snprintf(topic, sizeof(topic), "soil-sensor/%llx/temperature", device_address);

            // Publish temperature message on radio
            bc_radio_pub_float(topic, &temperature);
        }

        uint16_t raw_cap_u16;

        if (bc_soil_sensor_get_cap_raw(self, device_address, &raw_cap_u16))
        {
            snprintf(topic, sizeof(topic), "soil-sensor/%llx/raw", device_address);

            // Publish raw capacitance value message on radio
            int raw_cap = (int)raw_cap_u16;
            bc_radio_pub_int(topic, &raw_cap);

            /*
            // Experimental - send percent moisture value based on sensor calibration
            int moisture;
            bc_soil_sensor_get_moisture(self, device_address, &moisture);
            snprintf(topic, sizeof(topic), "soil-sensor/%llx/moisture", device_address);
            bc_radio_pub_int(topic, &moisture);
            */
        }
    }
    else if (event == BC_SOIL_SENSOR_EVENT_ERROR)
    {
        int error = bc_soil_sensor_get_error(self);
        bc_radio_pub_int("soil-sensor/-/error", &error);
    }
}

void switch_to_normal_mode_task(void *param)
{
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_NORMAL_INTERVAL);

    bc_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_NORMAL_INTERVAL);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

void application_init(void)
{
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize thermometer sensor on core module
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_SERVICE_INTERVAL);

    // Initialize soil sensor
    bc_soil_sensor_init_multiple(&soil_sensor, sensors, 5);
    bc_soil_sensor_set_event_handler(&soil_sensor, soil_sensor_event_handler, NULL);
    bc_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_SERVICE_INTERVAL);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_pairing_request("soil-sensor", VERSION);

    bc_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_MODE_INTERVAL);

    bc_led_pulse(&led, 2000);
}
