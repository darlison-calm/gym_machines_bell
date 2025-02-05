#include "main.h"

// Inicializa o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
   // Configura o pino do buzzer para PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obtém o slice e o canal do PWM
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);

    // Configura o PWM para a frequência desejada
    pwm_set_wrap(slice_num, 125000000 / 1000); // Frequência inicial de 1 kHz
    pwm_set_chan_level(slice_num, channel, 125000000 / 2000); // Ciclo de trabalho de 50%
    pwm_set_enabled(slice_num, false); // Inicialmente desabilita o PWM
}

void play_bell(uint32_t frequency, uint32_t duration_ms) {
    // Obtém o slice e o canal do PWM
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint channel = pwm_gpio_to_channel(BUZZER_PIN);

    // Configura o PWM para a frequência desejada
    uint32_t wrap_value = 125000000 / frequency; // Calcula o valor de wrap
    pwm_set_wrap(slice_num, wrap_value);
    pwm_set_chan_level(slice_num, channel, wrap_value / 2); // Ciclo de trabalho de 50%

    pwm_set_enabled(slice_num, true);

    sleep_ms(duration_ms);

    // Desliga o PWM
    pwm_set_enabled(slice_num, false);
}