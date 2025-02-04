#include "main.h"

#define PUB_DELAY_MS 1000

static uint32_t last_status_time = 0;
static uint32_t STATUS_PUBLISH_INTERVAL = 8000;

static uint32_t last_time = 0;
err_t mqtt_test_connect(MQTT_CLIENT_T *state);

void dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    MQTT_CLIENT_T *state = (MQTT_CLIENT_T*)callback_arg;
    if (ipaddr) {
        state->remote_addr = *ipaddr;
        DEBUG_printf("DNS resolved: %s\n", ip4addr_ntoa(ipaddr));
    } else {
        DEBUG_printf("DNS resolution failed.\n");
    }
}

void run_dns_lookup(MQTT_CLIENT_T *state) {
    DEBUG_printf("Running DNS lookup for %s...\n", MQTT_SERVER_HOST);
    if (dns_gethostbyname(MQTT_SERVER_HOST, &(state->remote_addr), dns_found, state) == ERR_INPROGRESS) {
        while (state->remote_addr.addr == 0) {
            cyw43_arch_poll();
            sleep_ms(10);
        }
    }
}

static void mqtt_pub_start_cb(void *arg, const char *topic, u32_t tot_len) {
    DEBUG_printf("Incoming message on topic: %s\n", topic);
}

static void mqtt_pub_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    char buffer[BUFFER_SIZE];
    if (len > BUFFER_SIZE) {
        DEBUG_printf("Mensagem é muito grande.\n");
        return;
    }

    // Copia os dados recebidos para o buffer e adiciona o terminador nulo
    memcpy(buffer, data, len);
    buffer[len] = '\0';

    // Converte o ID recebido para inteiro
    int received_id = atoi(buffer);
    DEBUG_printf("Mensagem recebida: %s\n", buffer);

    process_machine_assistance(received_id);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        connection_status_alert(true, "mqtt");
        DEBUG_printf("MQTT connected.\n");
    } else {
        connection_status_alert(false, "mqtt");
        DEBUG_printf("MQTT connection failed: %d\n", status);
    }
}

void mqtt_pub_request_cb(void *arg, err_t err) {
    DEBUG_printf("Publish request status: %d\n", err);
}

void mqtt_sub_request_cb(void *arg, err_t err) {
    DEBUG_printf("Subscription request status: %d\n", err);
}

// Função para publicar a solicitação da máquina no MQTT
err_t mqtt_machine_request(MQTT_CLIENT_T *state) {
    char buffer[BUFFER_SIZE];

    snprintf(buffer, BUFFER_SIZE,
        "A máquina '%s' - ID: %d foi chamada para o atendimento. \nTamanho fila de espera: %d.",
        machines[triggered_machine].name,
        machines[triggered_machine].id,
        waiting_queue);

    return mqtt_publish(state->mqtt_client, "pico_w/machine_requests", buffer, strlen(buffer), 0, 0, mqtt_pub_request_cb, state);
}

// Função para publicar o status de cada máquina
err_t mqtt_publish_machine_status(MQTT_CLIENT_T *state) {
    char buffer[BUFFER_SIZE];
    char formatted_machine_time[9];
    // Percorre todas as máquinas e publica o status de cada uma
    for (int i = 0; i < 3; i++) {
        format_waiting_time(machines[i].waiting_time, formatted_machine_time);
        // Formata o status da máquina com os campos disponíveis
        snprintf(buffer, BUFFER_SIZE, 
            "Máquina %s - ID: %d\nStatus: %s\nTempo de espera: %s", 
            machines[i].name, 
            machines[i].id, 
            machines[i].needs_assistance ? "Aluno precisa de ajuda" : "Aluno não precisa de ajuda",
            formatted_machine_time
        );

        // Publica o status para cada máquina
        mqtt_publish(
            (state)->mqtt_client, 
            "pico_w/machine_requests", 
            buffer, 
            strlen(buffer), 
            0, 
            0, 
            mqtt_pub_request_cb, 
            state
        );
    }

    return ERR_OK;
}

err_t mqtt_publish_machine_assistance(MQTT_CLIENT_T *state) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Evita publicações muito frequentes
    if (now - last_time < PUB_DELAY_MS) return 1;

    if (!process_machine_request()) return 2;
    
    last_time = now;
    // Publica a mensagem no MQTT
    return mqtt_machine_request(state);
}

err_t mqtt_test_connect(MQTT_CLIENT_T *state) {
    struct mqtt_connect_client_info_t ci = {0};
    ci.client_id = "PicoW";
    return mqtt_client_connect(state->mqtt_client, &(state->remote_addr), MQTT_SERVER_PORT, mqtt_connection_cb, state, &ci);
}

void mqtt_run_test(MQTT_CLIENT_T *state) {
    state->mqtt_client = mqtt_client_new();
    if (!state->mqtt_client) {
        DEBUG_printf("Failed to create MQTT client\n");
        return;
    }

    if (mqtt_test_connect(state) == ERR_OK) {
        mqtt_set_inpub_callback(state->mqtt_client, mqtt_pub_start_cb, mqtt_pub_data_cb, NULL);
        mqtt_sub_unsub(state->mqtt_client, "pico_w/machine_assistance", 0, mqtt_sub_request_cb, NULL, 1);

        while (1) {
            cyw43_arch_poll();
            uint32_t now = to_ms_since_boot(get_absolute_time());
            update_machines_waiting_times();


            if (mqtt_client_is_connected(state->mqtt_client)) {
                mqtt_publish_machine_assistance(state);

                // Publicação periódica de status de cada máquina
                if (now - last_status_time >= STATUS_PUBLISH_INTERVAL) {
                    mqtt_publish_machine_status(state);
                    last_status_time = now;
                }

            } else {
                DEBUG_printf("Reconnecting...\n");
                sleep_ms(250);
                mqtt_test_connect(state);
            }
        }
    }
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
                                                  20000);
    if (result == 0) {  // Conexão bem sucedida
        DEBUG_printf("WiFi connected successfully!\n");
        connection_status_alert(true, "wifi");
        return 0;
    } else {  // Falha na conexão
        DEBUG_printf("Failed to connect to WiFi (error: %s)\n", result);
        connection_status_alert(false, "wifi");
        return 2;
    }
}

