#include "main.h"

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Variáveis globais para o display
static struct render_area frame_area;
static uint8_t ssd[ssd1306_buffer_length];

void init_display_oled() {
    
    // Inicialização do i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();
    
    // Preparar área de renderização para o display
    frame_area.start_column = 0;
    frame_area.end_column = ssd1306_width - 1;
    frame_area.start_page = 0;
    frame_area.end_page = ssd1306_n_pages - 1;
    
    calculate_render_area_buffer_length(&frame_area);
    
    // Zera o display inteiro
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
}

void display_oled_message(char *line1, char *line2) {
    // Limpa o buffer
    memset(ssd, 0, ssd1306_buffer_length);
    
    // Desenha as duas linhas de texto
    ssd1306_draw_string(ssd, 5, 0, line1);
    ssd1306_draw_string(ssd, 5, 8, line2);
    
    // Atualiza o display
    render_on_display(ssd, &frame_area);
}

