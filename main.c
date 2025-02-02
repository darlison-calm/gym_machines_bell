#include "main.h"

int initialize_wifi(const char* ssid, const char* password);

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
    initialize_machines();
    init_leds();
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

int initialize_wifi(const char* ssid, const char* password) {
     // Tenta inicializar o hardware WiFi
    if (cyw43_arch_init()) {
        DEBUG_printf("Failed to initialize WiFi\n");
        return 1;
    }
    // Configura modo Station (cliente)
    cyw43_arch_enable_sta_mode();
    
    // Tenta conectar ao WiFi (retorna 0 em caso de sucesso)
    int result = cyw43_arch_wifi_connect_timeout_ms(ssid, 
                                                  password, 
                                                  CYW43_AUTH_WPA2_AES_PSK, 
                                                  10000);
    if (result == 0) {  // Conexão bem sucedida
        DEBUG_printf("WiFi connected successfully!\n");
        connection_status_alert(true);
        return 0;
    } else {  // Falha na conexão
        DEBUG_printf("Failed to connect to WiFi (error: %s)\n", result);
        connection_status_alert(false);
        return 2;
    }
}
