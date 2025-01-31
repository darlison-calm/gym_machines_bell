#include "main.h"
// bool alarme = false;
// bool posicao_js = false;
// bool ledverdestatus = false;

// static MQTT_CLIENT_T* mqtt_client_init(void) {
//     MQTT_CLIENT_T *state = calloc(1, sizeof(MQTT_CLIENT_T));
//     if (!state) {
//         DEBUG_printf("Failed to allocate state\n");
//         return NULL;
//     }
//     return state;
// }




int main() {
    stdio_init_all();
    initialize_machines();
    setup_gpio_interrupts();
    run_main_loop();
    return 0;
    // pinos_start();
    // if (cyw43_arch_init()) {
    //     DEBUG_printf("Failed to initialize WiFi\n");
    //     return 1;
    // }
    // cyw43_arch_enable_sta_mode();

    // if (cyw43_arch_wifi_connect_timeout_ms("teste", "12345678", CYW43_AUTH_WPA2_AES_PSK, 30000)) {
    //     DEBUG_printf("Failed to connect to WiFi\n");
    //     return 1;
    // } else {
    //     DEBUG_printf("WiFi connected successfully!\n");
    // }   

    // MQTT_CLIENT_T *state = mqtt_client_init();
    // run_dns_lookup(state);
    // mqtt_run_test(state);

    // cyw43_arch_deinit();
    // return 0;
}
