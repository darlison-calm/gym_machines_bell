#include "main.h"

static MQTT_CLIENT_T* mqtt_client_init(void) {
    MQTT_CLIENT_T *state = calloc(1, sizeof(MQTT_CLIENT_T));
    if (!state) {
        DEBUG_printf("Failed to allocate state\n");
        return NULL;
    }
    return state;
}

int main() {
    stdio_init_all();
    init_leds();
    init_display_oled();
    pwm_init_buzzer(BUZZER_PIN);
    setup_gpio_interrupts();
    int wifi_status = initialize_wifi(WIFI_SSID, WIFI_PASSWORD);

    if (wifi_status != 0) {
        return wifi_status;
    }

    MQTT_CLIENT_T *state = mqtt_client_init();
    run_dns_lookup(state);
    mqtt_run_test(state);

    cyw43_arch_deinit();
    return 0;
}

