#include "main.h"

uint32_t waiting_queue = 0;
uint32_t triggered_machine = 0;
static bool interrupt_flag = false;


uint32_t last_debounce_timer[MAX_MACHINES] = {0};
MACHINE machines[MAX_MACHINES] = {
    {"Mesa Flexora", false, BUTTON_PIN_A, 1},
    {"Agachamento no Hack", false, BUTTON_PIN_B, 2},
    {"Leg Press", false, BUTTON_PIN_JS, 3}
};

void init_leds() {
    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT); 
    gpio_init(LED_PIN_B);
    gpio_set_dir(LED_PIN_B, GPIO_OUT); 
}

void play_alarm(uint32_t frequency, uint32_t duration_ms) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);  // Configura o pino do buzzer para PWM
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);  // Obtém o slice do PWM
    uint channel = pwm_gpio_to_channel(BUZZER_PIN);  // Obtém o canal do PWM

    // Configura o PWM para a frequência desejada
    uint32_t wrap_value = 125000000 / frequency;  // Calcula o valor de wrap
    pwm_set_wrap(slice_num, wrap_value);
    pwm_set_chan_level(slice_num, channel, wrap_value / 2);  // Ciclo de trabalho de 50%
    pwm_set_enabled(slice_num, true);  // Habilita o PWM

    sleep_ms(duration_ms);  // Mantém o tom por um determinado tempo

    pwm_set_enabled(slice_num, false);  // Desliga o PWM

    // Configura o pino como saída e define o nível como 0 (baixo)
    gpio_set_function(BUZZER_PIN, GPIO_IN);  // Altera a função do pino para entrada, desabilitando o PWM, removendo o chiado depois do som
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

void process_machine_assistance(uint32_t received_id) {
    // Encontra a máquina correspondente ao ID recebido
    MACHINE *machine = NULL;
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (machines[i].id == received_id) {
            machine = &machines[i];  
            break;
        }
    }
    
    // Verifica se a máquina foi encontrada
    if (machine == NULL) {
        return;
    }
    
    // Verifica se a máquina precisa de assistência
    if (!(machine->needs_assistance)) {
        DEBUG_printf("Máquina %d não precisa de assistência.\n", machine->id);
        return;
    }
    
    machine->needs_assistance = false;
    waiting_queue--;
    interrupt_flag = false;
    
    DEBUG_printf("Máquina %d atualizada. Fila de espera: %d\n", machine->id, waiting_queue);
    return;
}

bool process_machine_request() {
    // Verifica se a interrupção foi acionada
    if (!interrupt_flag) return false;

    // Verifica se a máquina já precisa de assistência
    if (machines[triggered_machine].needs_assistance) return false;

    waiting_queue++;
    interrupt_flag = false;  // Reset da flag após processamento
    machines[triggered_machine].needs_assistance = true;

    // Toca o alarme para indicar o chamado
    play_alarm(220, 300);

    return true;
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

// Alerta status de conexão WIFI/MQTT pelo LED
void connection_status_alert(bool success, const char* connection_type) {
    // Reseta todos os LEDs inicialmente
    gpio_put(LED_PIN_G, 0);
    gpio_put(LED_PIN_B, 0);
    gpio_put(LED_PIN_R, 0);

    // Tratamento de conexão com sucesso
    if (success) {
        // Define o LED de acordo com o tipo de conexão
        int led_pin = LED_PIN_G; // Padrão para conexão desconhecida
        
        if (strcmp(connection_type, "wifi") == 0) {
            led_pin = LED_PIN_B;
        } else if (strcmp(connection_type, "mqtt") == 0) {
            led_pin = LED_PIN_G;
        }

        // Padrão de pulso para conexão bem-sucedida
        for (int i = 0; i < 3; i++) {
            gpio_put(led_pin, 1);
            sleep_ms(500);
            gpio_put(led_pin, 0);
            sleep_ms(500);
        }
        return;
    }

    // Tratamento de falha de conexão - padrão de alerta com LED vermelho
    for (int i = 0; i < 5; i++) {
        gpio_put(LED_PIN_R, 1);
        sleep_ms(200);
        gpio_put(LED_PIN_R, 0);
        sleep_ms(200);
    }
}








































































