#include "main.h"

static bool interrupt_flag = false;

uint32_t waiting_queue = 0;
uint32_t triggered_machine = 0;
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

bool process_machine_assistance(uint32_t received_id) {
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
        return false;
    }
    
    // Verifica se a máquina precisa de assistência
    if (!(machine->needs_assistance)) {
        DEBUG_printf("Máquina %d não precisa de assistência.\n", machine->id);
        return false;
    }

    machine->needs_assistance = false;
    machine->waiting_time = 0;
    waiting_queue--;
    interrupt_flag = false;
    play_bell(2200, 200);
    display_oled_message("REALIZADO", machine->name);
    DEBUG_printf("Máquina %d atualizada. Fila de espera: %d\n", machine->id, waiting_queue);
    return true;
}

bool process_machine_request() {
    // Verifica se a interrupção foi acionada
    if (!interrupt_flag) return false;

    // Verifica se a máquina já precisa de assistência
    if (machines[triggered_machine].needs_assistance) return false;

    waiting_queue++;
    interrupt_flag = false;  // Reset da flag após processamento
    machines[triggered_machine].needs_assistance = true;
    machines[triggered_machine].call_timer = to_ms_since_boot(get_absolute_time());

    // Toca o alarme para indicar o chamado
    play_bell(2000, 200);
    display_oled_message("AJUDA", machines[triggered_machine].name);
    return true;
}

void update_machines_waiting_times() {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (machines[i].needs_assistance) {
            machines[i].waiting_time = now - machines[i].call_timer;
        }
    }
}

void format_waiting_time(uint32_t milliseconds, char* output) {
    uint32_t total_seconds = milliseconds / 1000;
    uint32_t hours = total_seconds / 3600;
    uint32_t remaining = total_seconds % 3600;
    uint32_t minutes = remaining / 60;
    uint32_t seconds = remaining % 60;

    // Format the time string without milliseconds
    sprintf(output, "%02u:%02u:%02u", hours, minutes, seconds);
}









































































