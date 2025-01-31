#include "main.h"
uint32_t waiting_queue = 0;
uint32_t last_debounce_timer[MAX_MACHINES]; 
MACHINE machines[MAX_MACHINES];

static uint triggered_machine = 0;
static volatile bool interrupt_flag = false;

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
         // Verifica se já precisa de assistência
        if (machines[i].needs_assistance) continue;

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

void run_main_loop() {
    while(true) {
        if (interrupt_flag) {
            process_machine_request();
        }
        sleep_ms(10);
    }
}







































static uint32_t last_debounce_time[3] = {0, 0, 0};
uint adc_x_raw;
uint adc_y_raw;
uint brightness = 0;
bool increasing = true; 


static const char *gpio_irq_str[] = {
        "LEVEL_LOW",  // 0x1
        "LEVEL_HIGH", // 0x2
        "EDGE_FALL",  // 0x4
        "EDGE_RISE"   // 0x8
};

void pinos_start()
{
    gpio_init(LED_PIN_R);
    gpio_init(LED_PIN_B);
    gpio_init(LED_PIN_G);
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);
    gpio_set_function(LED_PIN_R, GPIO_FUNC_PWM);
    gpio_set_function(LED_PIN_G, GPIO_FUNC_PWM);
    gpio_set_function(LED_PIN_B, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(LED_PIN_R);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, true);
    slice_num = pwm_gpio_to_slice_num(LED_PIN_B);
    pwm_init(slice_num, &config, true);
    slice_num = pwm_gpio_to_slice_num(LED_PIN_G);
    pwm_init(slice_num, &config, true);
    

    gpio_init(BUTTON6_PIN);
    gpio_set_dir(BUTTON6_PIN, GPIO_IN);
    gpio_pull_up(BUTTON6_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON6_PIN,GPIO_IRQ_EDGE_FALL, true, &gpio5_callback);
    
    gpio_init(BUTTON5_PIN);
    gpio_set_dir(BUTTON5_PIN, GPIO_IN);
    gpio_pull_up(BUTTON5_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON5_PIN,GPIO_IRQ_EDGE_FALL, true, &gpio5_callback);

    gpio_init(BUTTONJS_PIN);
    gpio_set_dir(BUTTONJS_PIN, GPIO_IN);
    gpio_pull_up(BUTTONJS_PIN);
    gpio_set_irq_enabled_with_callback(BUTTONJS_PIN,GPIO_IRQ_EDGE_FALL, true, &gpio5_callback);
}

void gpio_event_string(char *buf, uint32_t events) {
    for (uint i = 0; i < 4; i++) {
        uint mask = (1 << i);
        if (events & mask) {
            // Copy this event string into the user string
            const char *event_str = gpio_irq_str[i];
            while (*event_str != '\0') {
                *buf++ = *event_str++;
            }
            events &= ~mask;

            // If more events add ", "
            if (events) {
                *buf++ = ',';
                *buf++ = ' ';
            }
        }
    }
    *buf++ = '\0';
}

void gpio5_callback(uint gpio, uint32_t events) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (gpio == 5 && (now - last_debounce_time[0] > DEBOUNCE_DELAY_MS)) 
    {
        last_debounce_time[0] = now;
        
    }
    if (gpio == 6 && (now - last_debounce_time[1] > DEBOUNCE_DELAY_MS)) 
    {
        last_debounce_time[1] = now;
       
    }
    if (gpio == 22 && (now - last_debounce_time[2] > DEBOUNCE_DELAY_MS)) 
    {
        last_debounce_time[2] = now;
        printf("posicao_js: %u\n", posicao_js);
    }
}

// void gpio_callback(uint gpio_pin) {
//     for (int i = 0; i < MAX_MACHINES; i++) {
//         uint32_t now = to_ms_since_boot(get_absolute_time());
//         if (machines[i].gpio_pin == gpio_pin && !machines[i].attended) {
//             waiting_queue++;
//             printf("A máquina '%s' foi chamada para o atendimento.\n", machines[i].name);
//             printf("Numero de pessoas na fila:  %i", waiting_queue);
//             machines[i].needs_assistance = true; // Marca a máquina como atendida
//         }
//     }
// }


void js()
{
    adc_select_input(0);
    adc_x_raw = adc_read();
    printf("Posicao eixo X: %u\n",adc_x_raw);
    adc_select_input(1);
    adc_y_raw = adc_read();
    printf("Posicao eixo Y: %u\n",adc_y_raw);
    sleep_ms(50);
}

void setup_pwm(uint gpio_pin) {
    // Configurar o GPIO como saída de PWM
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);

    // Obter o slice de PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(gpio_pin);

    // Configurar o PWM com o padrão
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);  // Ajustar divisor de clock (frequência do PWM)
    pwm_init(slice_num, &config, true);
}

void update_pwm(uint gpio_pin) {
    // Atualizar o duty cycle baseado no brilho atual
    pwm_set_gpio_level(LED_PIN_R, brightness);
    printf("brilho_led_vermelho: %u\n",brightness);
    // Atualizar o valor do brilho
    if (increasing) 
    {
        brightness = brightness+400;
        if (brightness >= PWM_STEPS) 
        {
            increasing = false; // Começar a diminuir
        }
    }
    else 
    {
        brightness = brightness-400;
        if (brightness == 0) {
            increasing = true; // Começar a aumentar
        }
    }
}

void pwm_led(uint gpio_pin, uint brilho)
{
    if(gpio_pin == LED_PIN_B)
    {
        pwm_set_gpio_level(LED_PIN_B, brilho);
    }
    else if (gpio_pin == LED_PIN_G)
    {
        pwm_set_gpio_level(LED_PIN_G, brilho);
    }
}




























