#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

// ======================= CONFIGURAÇÕES =======================
#define WIFI_SSID "redeteste" //coloque o SSID da sua rede
#define WIFI_PASSWORD "rede@#2571" //coloque a senha da sua rede
#define LED_PIN           11
#define BUTTON_PIN        5
#define DEBOUNCE_DELAY_MS 50

// ======================= VARIÁVEIS GLOBAIS =======================
volatile float current_temperature = 0.0f;
#define LOG_BUFFER_SIZE 2048
char button_log_buffer[LOG_BUFFER_SIZE] = "Nenhum evento ainda.<br>";

// ======================= SERVIDOR TCP =======================
#define RESPONSE_BUFFER_SIZE (LOG_BUFFER_SIZE + 1024)
static char http_response_buffer[RESPONSE_BUFFER_SIZE];

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    struct tcp_pcb *client_pcb;
    bool complete;
    uint32_t sent_len; // Para rastrear quantos bytes já foram enviados
} TCP_SERVER_T;

static void tcp_server_close(TCP_SERVER_T *state) {
    if (state->client_pcb) {
        tcp_arg(state->client_pcb, NULL);
        tcp_poll(state->client_pcb, NULL, 0);
        tcp_sent(state->client_pcb, NULL);
        tcp_recv(state->client_pcb, NULL);
        tcp_err(state->client_pcb, NULL);
        tcp_close(state->client_pcb);
        state->client_pcb = NULL;
    }
}

static err_t send_data(void *arg, struct tcp_pcb *tpcb) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    
    // Calcula quanto espaço temos no buffer de envio
    u16_t buffer_space = tcp_sndbuf(tpcb);
    if (buffer_space == 0) {
        return ERR_BUF;
    }

    // Calcula quantos dados ainda faltam para enviar
    uint32_t remaining_len = strlen(http_response_buffer) - state->sent_len;
    if (remaining_len == 0) {
        // Nada mais a enviar, podemos fechar
        state->complete = true;
        tcp_server_close(state);
        return ERR_OK;
    }

    // A quantidade a enviar é o mínimo entre o que falta e o espaço disponível
    u16_t len_to_send = (u16_t)(remaining_len < buffer_space ? remaining_len : buffer_space);

    // Envia o próximo pedaço
    err_t err = tcp_write(tpcb, http_response_buffer + state->sent_len, len_to_send, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("Falha ao escrever no buffer TCP: %s\n", lwip_strerr(err));
        tcp_server_close(state);
        return err;
    }

    // Atualiza o contador de bytes enviados
    state->sent_len += len_to_send;
    
    tcp_output(tpcb);
    
    return ERR_OK;
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    // Esta função continua enviando o resto da mensagem
    // se necessário, em vez de fechar a conexão prematuramente.
    return send_data(arg, tpcb);
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;

    if (!p) {
        tcp_server_close(state);
        return ERR_OK;
    }
    
    tcp_recved(tpcb, p->tot_len);

    // Verificamos se é uma requisição GET válida
    if (p->tot_len >= 3 && strncmp((char*)p->payload, "GET", 3) == 0) {
        pbuf_free(p); // Libera o pbuf da requisição, pois não precisamos mais dele

        // Prepara a resposta no buffer estático
        cyw43_arch_lwip_begin();
        snprintf(http_response_buffer, RESPONSE_BUFFER_SIZE,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html\r\n"
                 "Connection: close\r\n\r\n"
                 "<!DOCTYPE html><html><head><title>Pico W Monitor</title>"
                 "<style>"
                 "body{font-family:Arial,sans-serif;text-align:center;margin-top:50px;background-color:#f0f0f0;}"
                 "h1{color:#333;}.info-box{background-color:white;border-radius:10px;padding:20px;margin:20px auto;max-width:600px;box-shadow:0 4px 8px rgba(0,0,0,0.1);text-align:left;}"
                 ".status{font-size:20px;margin:10px 0;}.temperature{color:#d9534f;}.log-title{font-weight:bold;margin-top:20px;}"
                 ".log-area{background-color:#e9e9e9;border-left:4px solid #5cb85c;padding:10px;margin-top:5px;height:200px;overflow-y:auto;text-align:left;font-family:monospace;}"
                 "</style><meta http-equiv=\"refresh\" content=\"2\"></head><body><h1>Monitoramento Raspberry Pi Pico W</h1>"
                 "<div class=\"info-box\">"
                 "<p class=\"status temperature\">Temperatura Atual: %.2f &deg;C</p>"
                 "<p class=\"log-title\">Registro de Eventos do Botao:</p>"
                 "<div class=\"log-area\">%s</div>"
                 "</div></body></html>",
                 current_temperature, button_log_buffer);
        cyw43_arch_lwip_end();

        // Inicia o processo de envio
        state->sent_len = 0;
        send_data(state, tpcb);

        return ERR_OK;
    }
    
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || newpcb == NULL) {
        return ERR_VAL;
    }
    
    // Se já tivermos um cliente, recusa a nova conexão.
    if (state->client_pcb != NULL) {
        tcp_abort(newpcb);
        return ERR_ABRT;
    }
    
    state->client_pcb = newpcb;
    tcp_arg(state->client_pcb, state);
    tcp_recv(state->client_pcb, tcp_server_recv);
    tcp_sent(state->client_pcb, tcp_server_sent);
    return ERR_OK;
}

// Função para inicializar o servidor
bool init_tcp_server(void) {
    // Aloca memória para o estado do nosso servidor.
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
        return false;
    }

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        free(state);
        return false;
    }

    err_t err = tcp_bind(pcb, NULL, 80);
    if (err) {
        free(state);
        return false;
    }

    state->server_pcb = tcp_listen(pcb);
    if (!state->server_pcb) {
        free(state);
        tcp_close(pcb);
        return false;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    return true;
}

// Função para ler temperatura
void read_internal_temperature() {
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw_value * conversion_factor;
    current_temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
}

// Função principal
int main() {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar.\n");
        return -1;
    }
    printf("Conectado com sucesso!\n");
    printf("IP do dispositivo: %s\n", ipaddr_ntoa(netif_ip4_addr(netif_default)));
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, false); 

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN); 

    adc_init();
    adc_set_temp_sensor_enabled(true);

    if (!init_tcp_server()) {
        printf("Falha ao iniciar servidor web\n");
        return -1;
    }
    printf("Servidor web iniciado na porta 80\n");

    bool last_steady_state = true;
    bool last_flicker_state = true;
    uint32_t last_debounce_time = 0;

    while (true) {
        cyw43_arch_poll();

        bool current_flicker_state = gpio_get(BUTTON_PIN);

        if (current_flicker_state != last_flicker_state) {
            last_debounce_time = to_ms_since_boot(get_absolute_time());
            last_flicker_state = current_flicker_state;
        }

        if ((to_ms_since_boot(get_absolute_time()) - last_debounce_time) > DEBOUNCE_DELAY_MS) {
            if (current_flicker_state != last_steady_state) {
                last_steady_state = current_flicker_state;

                bool button_pressed_now = !last_steady_state;

                if (button_pressed_now) {
                    uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
                    uint32_t current_time_s = current_time_ms / 1000;
                    
                    char new_entry[100];
                    snprintf(new_entry, sizeof(new_entry), "Botao pressionado no segundo %lu.<br>", current_time_s);
                    
                    //Protege o acesso ao buffer compartilhado
                    cyw43_arch_lwip_begin();
                    if (strstr(button_log_buffer, "Nenhum evento") != NULL) {
                        strcpy(button_log_buffer, "");
                    }
                    if (strlen(button_log_buffer) + strlen(new_entry) < LOG_BUFFER_SIZE) {
                        strcat(button_log_buffer, new_entry);
                    } else { // Se o log estiver cheio, reinicia para evitar corromper a memória
                         strcpy(button_log_buffer, new_entry);
                    }
                    cyw43_arch_lwip_end();
                    
                    printf("Log adicionado: %s", new_entry);
                    gpio_put(LED_PIN, 1);
                } else {
                    printf("Botao liberado\n");
                    gpio_put(LED_PIN, 0);
                }
            }
        }
        
        read_internal_temperature();
        sleep_ms(10);
    }
}