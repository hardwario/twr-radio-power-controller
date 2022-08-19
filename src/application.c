#include <application.h>

#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_INTERVAL (1 * 1000)

// LED instance
twr_led_t led;
bool led_state = false;

// Button instance
twr_button_t button;

// Temperature instance
twr_tag_temperature_t temperature;
event_param_t temperature_event_param = { .next_pub = 0 };

// Led strip
static uint32_t _twr_module_power_led_strip_dma_buffer[LED_STRIP_COUNT * LED_STRIP_TYPE * 2];
const twr_led_strip_buffer_t led_strip_buffer =
{
    .type = LED_STRIP_TYPE,
    .count = LED_STRIP_COUNT,
    .buffer = _twr_module_power_led_strip_dma_buffer
};

static struct
{
    enum
    {
        LED_STRIP_SHOW_COLOR = 0,
        LED_STRIP_SHOW_COMPOUND = 1,
        LED_STRIP_SHOW_EFFECT = 2,
        LED_STRIP_SHOW_THERMOMETER = 3

    } show;
    twr_led_strip_t self;
    uint32_t color;
    struct
    {
        uint8_t data[TWR_RADIO_NODE_MAX_COMPOUND_BUFFER_SIZE];
        int length;
    } compound;
    struct
    {
        float temperature;
        int8_t min;
        int8_t max;
        uint8_t white_dots;
        float set_point;
        uint32_t color;

    } thermometer;

    twr_scheduler_task_id_t update_task_id;

} led_strip = { .show = LED_STRIP_SHOW_COLOR, .color = 0 };

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        twr_led_pulse(&led, 100);
    }
}

void temperature_tag_event_handler(twr_tag_temperature_t *self, twr_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == TWR_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        if (twr_tag_temperature_get_temperature_celsius(self, &value))
        {
            if ((fabs(value - param->value) >= TEMPERATURE_PUB_VALUE_CHANGE) || (param->next_pub < twr_scheduler_get_spin_tick()))
            {
                twr_radio_pub_temperature(param->channel, &value);

                param->value = value;
                param->next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void twr_radio_node_on_state_get(uint64_t *id, uint8_t state_id)
{
    (void) id;

    if (state_id == TWR_RADIO_NODE_STATE_POWER_MODULE_RELAY)
    {
        bool state = twr_module_power_relay_get_state();

        twr_radio_pub_state(TWR_RADIO_PUB_STATE_POWER_MODULE_RELAY, &state);
    }
    else if (state_id == TWR_RADIO_NODE_STATE_LED)
    {
        twr_radio_pub_state(TWR_RADIO_PUB_STATE_LED, &led_state);
    }
}

void twr_radio_node_on_state_set(uint64_t *id, uint8_t state_id, bool *state)
{
    (void) id;

    if (state_id == TWR_RADIO_NODE_STATE_POWER_MODULE_RELAY)
    {
        twr_module_power_relay_set_state(*state);

        twr_radio_pub_state(TWR_RADIO_PUB_STATE_POWER_MODULE_RELAY, state);
    }
    else if (state_id == TWR_RADIO_NODE_STATE_LED)
    {
        led_state = *state;

        twr_led_set_mode(&led, led_state ? TWR_LED_MODE_ON : TWR_LED_MODE_OFF);

        twr_radio_pub_state(TWR_RADIO_PUB_STATE_LED, &led_state);
    }
}

void led_strip_update_task(void *param)
{
    (void) param;

    if (!twr_led_strip_is_ready(&led_strip.self))
    {
        twr_scheduler_plan_current_now();

        return;
    }

    twr_led_strip_write(&led_strip.self);

    twr_scheduler_plan_current_relative(250);
}

void led_strip_fill(void)
{
    if (led_strip.show == LED_STRIP_SHOW_COLOR)
    {
        twr_led_strip_fill(&led_strip.self, led_strip.color);
    }
    else if (led_strip.show == LED_STRIP_SHOW_COMPOUND)
    {
        int from = 0;
        int to;
        uint8_t *color;

        for (int i = 0; i < led_strip.compound.length; i += 5)
        {
            color = led_strip.compound.data + i + 1;
            to = from + led_strip.compound.data[i];

            for (;(from < to) && (from < LED_STRIP_COUNT); from++)
            {
                twr_led_strip_set_pixel_rgbw(&led_strip.self, from, color[3], color[2], color[1], color[0]);
            }

            from = to;
        }
    }
    else if (led_strip.show == LED_STRIP_SHOW_THERMOMETER)
    {
        twr_led_strip_thermometer(&led_strip.self, led_strip.thermometer.temperature, led_strip.thermometer.min, led_strip.thermometer.max, led_strip.thermometer.white_dots, led_strip.thermometer.set_point, led_strip.thermometer.color);
    }
}

void twr_radio_node_on_led_strip_color_set(uint64_t *id, uint32_t *color)
{
    (void) id;

    twr_led_strip_effect_stop(&led_strip.self);

    led_strip.color = *color;

    led_strip.show = LED_STRIP_SHOW_COLOR;

    led_strip_fill();

    twr_scheduler_plan_now(led_strip.update_task_id);
}

void twr_radio_node_on_led_strip_brightness_set(uint64_t *id, uint8_t *brightness)
{
    (void) id;

    twr_led_strip_set_brightness(&led_strip.self, *brightness);

    led_strip_fill();

    twr_scheduler_plan_now(led_strip.update_task_id);
}

void twr_radio_node_on_led_strip_compound_set(uint64_t *id, uint8_t *compound, size_t length)
{
    (void) id;

    twr_led_strip_effect_stop(&led_strip.self);

    memcpy(led_strip.compound.data, compound, length);

    led_strip.compound.length = length;

    led_strip.show = LED_STRIP_SHOW_COMPOUND;

    led_strip_fill();

    twr_scheduler_plan_now(led_strip.update_task_id);
}

void twr_radio_node_on_led_strip_effect_set(uint64_t *id, twr_radio_node_led_strip_effect_t type, uint16_t wait, uint32_t *color)
{
    (void) id;

    switch (type) {
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_TEST:
        {
            twr_led_strip_effect_test(&led_strip.self);
            break;
        }
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_RAINBOW:
        {
            twr_led_strip_effect_rainbow(&led_strip.self, wait);
            break;
        }
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_RAINBOW_CYCLE:
        {
            twr_led_strip_effect_rainbow_cycle(&led_strip.self, wait);
            break;
        }
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_THEATER_CHASE_RAINBOW:
        {
            twr_led_strip_effect_theater_chase_rainbow(&led_strip.self, wait);
            break;
        }
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_COLOR_WIPE:
        {
            twr_led_strip_effect_color_wipe(&led_strip.self, *color, wait);
            break;
        }
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_THEATER_CHASE:
        {
            twr_led_strip_effect_theater_chase(&led_strip.self, *color, wait);
            break;
        }
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_STROBOSCOPE:
        {
            twr_led_strip_effect_stroboscope(&led_strip.self, *color, wait);
            break;
        }
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_ICICLE:
        {
            twr_led_strip_effect_icicle(&led_strip.self, *color, wait);
            break;
        }
        case TWR_RADIO_NODE_LED_STRIP_EFFECT_PULSE_COLOR:
        {
            twr_led_strip_effect_pulse_color(&led_strip.self, *color, wait);
            break;
        }
        default:
            return;
    }

    led_strip.show = LED_STRIP_SHOW_EFFECT;
}

void twr_radio_node_on_led_strip_thermometer_set(uint64_t *id, float *temperature, int8_t *min, int8_t *max, uint8_t *white_dots, float *set_point, uint32_t *set_point_color)
{
    (void) id;

    twr_led_strip_effect_stop(&led_strip.self);

    led_strip.thermometer.temperature = *temperature;
    led_strip.thermometer.min = *min;
    led_strip.thermometer.max = *max;
    led_strip.thermometer.white_dots = *white_dots;

    if (set_point != NULL)
    {
        led_strip.thermometer.set_point = *set_point;
        led_strip.thermometer.color = *set_point_color;
    }
    else
    {
        led_strip.thermometer.set_point = *min - 1;
    }

    led_strip.show = LED_STRIP_SHOW_THERMOMETER;

    led_strip_fill();

    twr_scheduler_plan_now(led_strip.update_task_id);
}

void application_init(void)
{
    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    twr_radio_init(TWR_RADIO_MODE_NODE_LISTENING);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_scan_interval(&button, 20);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize temperature
    temperature_event_param.channel = TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    twr_tag_temperature_init(&temperature, TWR_I2C_I2C0, TWR_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);
    twr_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_INTERVAL);
    twr_tag_temperature_set_event_handler(&temperature, temperature_tag_event_handler, &temperature_event_param);

    // Initialize power module
    twr_module_power_init();

    // Initialize led-strip on power module
    twr_led_strip_init(&led_strip.self, twr_module_power_get_led_strip_driver(), &led_strip_buffer);

    led_strip.update_task_id = twr_scheduler_register(led_strip_update_task, NULL, 0);

    twr_radio_pairing_request("power-controller", FW_VERSION);

    twr_led_pulse(&led, 2000);
}
