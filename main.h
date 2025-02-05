#include <time.h>
#include <stdio.h>
#include <string.h>
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "lwip/apps/mqtt.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt_priv.h"
#include "hardware/structs/rosc.h"

#define MAX_MACHINES 3
#define WIFI_SSID "teste"
#define WIFI_PASSWORD "12345678"

/* MACROS PI PICO */
#define LED_PIN_G 11
#define LED_PIN_B 12
#define LED_PIN_R 13
#define BUTTON_PIN_A 5
#define BUTTON_PIN_B 6
#define BUTTON_PIN_JS 22
#define DEBOUNCE_DELAY_MS 700
#define FADE_STEP_DELAY 100
#define BUZZER_PIN 10
/* END */

/* MACROS MQTT */
#define DEBUG_printf printf
#define MQTT_SERVER_HOST "192.168.1.11"
#define MQTT_SERVER_PORT 1883
#define MQTT_TLS 0
#define BUFFER_SIZE 256
/* END*/

/*VARIAVEIS*/
typedef struct MQTT_CLIENT_T_ {
    ip_addr_t remote_addr;
    mqtt_client_t *mqtt_client;
    u32_t received;
    u32_t counter;
    u32_t reconnect;
} MQTT_CLIENT_T;

typedef struct MACHINE {
    char name[30];
    bool needs_assistance;
    int gpio_pin;
    int id;
    uint32_t call_timer; // Momento em que a assistência foi solicitada
    uint32_t waiting_time; // Tempo de espera armazenado
} MACHINE;

extern MACHINE machines[MAX_MACHINES];
extern uint32_t waiting_queue;
extern uint32_t triggered_machine;
/* END */

/* FUNÇOES */
void setup_gpio_interrupts();
void pwm_init_buzzer(uint pin);
void play_bell(uint32_t frequency, uint32_t duration_ms);
void handle_machine_interrupt(uint gpio, uint32_t events);

void init_leds();
void connection_status_alert(bool success, const char* connection_type);
int initialize_wifi(const char* ssid, const char* password);
bool process_machine_request();
bool process_machine_assistance(uint32_t received_id);
void update_machines_waiting_times();
void format_waiting_time(uint32_t milliseconds, char* output);

static MQTT_CLIENT_T* mqtt_client_init(void);
void run_dns_lookup(MQTT_CLIENT_T *state);
void mqtt_run_test(MQTT_CLIENT_T *state);


/* END */