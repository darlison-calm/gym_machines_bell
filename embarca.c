#include "main.h"
uint32_t waiting_queue = 0;
uint32_t last_debounce_timer[MAX_MACHINES]; 
MACHINE machines[MAX_MACHINES];

err_t mqtt_test_connect(MQTT_CLIENT_T *state);

static uint triggered_machine = 0;
static volatile bool interrupt_flag = false;

#define PUB_DELAY_MS 1000
static uint32_t counter = 0;
static uint32_t last_time = 0;

void init_leds() {
    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT); 
}

// Função para inicializar as máquinas
void initialize_machines() {
  
    // Nomes das máquinas e os pinos GPIO correspondentes
    const char *names[] = {"Mesa Flexora", "Agachamento no Hack", "Leg Press"};
    int gpio_pins[] = {5, 6, 22};
    // Inicializa as máquinas
    for (int i = 0; i < MAX_MACHINES; i++) {
        strcpy(machines[i].name, names[i]);      // Atribui o nome da máquina
        machines[i].gpio_pin = gpio_pins[i];     // Atribui o pino GPIO
        machines[i].needs_assistance = false; 
        last_debounce_timer[i] = 0;
    }
}

void play_alarm(uint32_t frequency, uint32_t duration_ms) {
    gpio_set_function(10, GPIO_FUNC_PWM);  // Configura o pino do buzzer para PWM
    uint slice_num = pwm_gpio_to_slice_num(10);  // Obtém o slice do PWM
    uint channel = pwm_gpio_to_channel(10);  // Obtém o canal do PWM

    // Configura o PWM para a frequência desejada
    uint32_t wrap_value = 125000000 / frequency;  // Calcula o valor de wrap
    pwm_set_wrap(slice_num, wrap_value);
    pwm_set_chan_level(slice_num, channel, wrap_value / 2);  // Ciclo de trabalho de 50%
    pwm_set_enabled(slice_num, true);  // Habilita o PWM

    sleep_ms(duration_ms);  // Mantém o tom por um determinado tempo

    pwm_set_enabled(slice_num, false);  // Desliga o PWM

    // Configura o pino como saída e define o nível como 0 (baixo)
    gpio_set_function(10, GPIO_IN);  // Altera a função do pino para entrada, desabilitando o PWM, removendo o chiado depois do som
}



void handle_machine_interrupt(uint gpio, uint32_t events) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Verifica qual máquina acionou a interrupção
    for (int i = 0; i < MAX_MACHINES; i++) {  
        
        // Verifica se é o pino correto
        if (gpio != machines[i].gpio_pin) continue;
        
        // Verifica debounce
        if (now - last_debounce_timer[i] <= DEBOUNCE_DELAY_MS) continue;

        last_debounce_timer[i] = now; // Atualiza o tempo de debounce
        triggered_machine = i;
        interrupt_flag = true;
        break; // Sai do loop após encontrar a máquina correta        
    }
}
// Função para configurar as interrupções GPIO
void setup_gpio_interrupts() {
    for (int i = 0; i < MAX_MACHINES; i++) {
        gpio_init(machines[i].gpio_pin);  // Inicializa o pino GPIO
        gpio_set_dir(machines[i].gpio_pin, GPIO_IN);  // Define o pino como entrada
        gpio_pull_up(machines[i].gpio_pin);  // Habilita o pull-up (evita flutuação de pino)
        gpio_set_irq_enabled_with_callback(machines[i].gpio_pin, GPIO_IRQ_EDGE_FALL, true, &handle_machine_interrupt);
    }
}

void process_machine_request() {
    waiting_queue++;
    machines[triggered_machine].needs_assistance = true;
    printf("A máquina '%s' foi chamada para o atendimento.\n", 
    machines[triggered_machine].name);
    printf("Número de pessoas na fila: %u\n", waiting_queue);
    
    play_alarm(220, 300);
    interrupt_flag = false;  // Reset da flag após processamento
    
}

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

// * MEXER DEPOIS *
// static void mqtt_pub_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
//     char buffer[BUFFER_SIZE];
//     if (len < BUFFER_SIZE) {
//         memcpy(buffer, data, len);
//         buffer[len] = '\0';
//         DEBUG_printf("Message received: %s\n", buffer);
//         if (strcmp(buffer, "acender") == 0) {
//             pwm_led(LED_PIN_B, 2000);
//         } else if (strcmp(buffer, "apagar") == 0) {
//             pwm_led(LED_PIN_B, 0);
//         }
//     } else {
//         DEBUG_printf("Message too large, discarding.\n");
//     }
// }

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        connection_status_alert(true);
        DEBUG_printf("MQTT connected.\n");
    } else {
         connection_status_alert(false);
        DEBUG_printf("MQTT connection failed: %d\n", status);
    }
}

void mqtt_pub_request_cb(void *arg, err_t err) {
    DEBUG_printf("Publish request status: %d\n", err);
}

void mqtt_sub_request_cb(void *arg, err_t err) {
    DEBUG_printf("Subscription request status: %d\n", err);
}

err_t mqtt_test_publish(MQTT_CLIENT_T *state) {
    // Obtém o tempo atual em milissegundos desde a inicialização
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Evita publicações muito frequentes
    if (now - last_time < PUB_DELAY_MS) return 1;
    
    // Verifica se a interrupção foi acionada
    if(!interrupt_flag) return 2;

    // Verifica se já precisa de assistência
    if (machines[triggered_machine].needs_assistance) return 3; 

    // Atualiza os estados globais
    last_time = now;
    waiting_queue++;
    counter += 1;
    interrupt_flag = false; // Reset da flag após processamento
    machines[triggered_machine].needs_assistance = true;
    // Toca o alarme para indicar o chamado
    play_alarm(220, 300);
    
    char buffer[BUFFER_SIZE];
    
    // Publica uma mensagem sobre a máquina chamada   
    snprintf(buffer, BUFFER_SIZE, "A máquina '%s' foi chamada para o atendimento.", 
                machines[triggered_machine].name);
    mqtt_publish(state->mqtt_client, "pico_w/test", buffer, strlen(buffer), 0, 0, mqtt_pub_request_cb, state);

    // Publica o status da máquina
    snprintf(buffer, BUFFER_SIZE, "{\"machine\": \"%s\", \"needs_assistance\": true, \"queue\": %u}",
            machines[triggered_machine].name, waiting_queue);
    return mqtt_publish(state->mqtt_client, "pico_w/test", buffer, strlen(buffer), 0, 0, mqtt_pub_request_cb, state);
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
         // mqtt_set_inpub_callback(state->mqtt_client, mqtt_pub_start_cb, mqtt_pub_data_cb, NULL);
        // mqtt_sub_unsub(state->mqtt_client, "pico_w/recv", 0, mqtt_sub_request_cb, NULL, 1);

        while (1) {
            cyw43_arch_poll();
            if (mqtt_client_is_connected(state->mqtt_client)) {
                mqtt_test_publish(state);
            } else {
                DEBUG_printf("Reconnecting...\n");
                sleep_ms(250);
                mqtt_test_connect(state);
            }
        }
    }
}

void connection_status_alert(bool success) {
    if (success) {
        gpio_put(LED_PIN_G, 1);  
        gpio_put(LED_PIN_R, 0); 
        sleep_ms(3000);
        gpio_put(LED_PIN_G, 0);  
    } else {
        gpio_put(LED_PIN_R, 1); 
        gpio_put(LED_PIN_G, 0);
        sleep_ms(3000);
        gpio_put(LED_PIN_R, 0);  
    }
}







































































