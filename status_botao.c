// Inclusão das bibliotecas necessárias
#include "pico/stdlib.h"           // Funções básicas da Raspberry Pi Pico
#include "hardware/adc.h"          // Controle do conversor analógico-digital (ADC)
#include "pico/cyw43_arch.h"       // Biblioteca para controle do módulo Wi-Fi CYW43
#include <stdio.h>                 // Entrada/saída padrão (printf, etc.)
#include <string.h>                // Funções de manipulação de strings
#include <stdlib.h>                // Funções utilitárias padrão (malloc, calloc, etc.)
#include "lwip/pbuf.h"             // Buffers de pacotes para TCP/IP (lwIP)
#include "lwip/tcp.h"              // Funções e estruturas para conexões TCP
#include "lwip/netif.h"            // Interface de rede (lwIP)

// Definições de constantes
#define WIFI_SSID "Asgard"         // Nome da rede Wi-Fi
#define WIFI_PASSWORD "ma199720@"  // Senha da rede Wi-Fi
#define LED_PIN 11                 // Pino GPIO utilizado para controle do LED
#define BUTTON_PIN 5               // Pino GPIO utilizado para leitura do botão
#define DEBOUNCE_DELAY_MS 25       // Tempo para eliminar efeito de bouncing (oscilações) do botão

volatile float current_temperature = 0.0f;  // Variável global para armazenar a temperatura atual (acesso em interrupções)

// Buffer para armazenar o log de eventos do botão (HTML)
#define LOG_BUFFER_SIZE 2048
char button_log_buffer[LOG_BUFFER_SIZE] = "Nenhum evento ainda.<br>";

// Texto exibindo o status atual do botão
char button_status[64] = "Botao liberado.";

// Buffer para montar a resposta HTTP completa
#define RESPONSE_BUFFER_SIZE (LOG_BUFFER_SIZE + 1024)
static char http_response_buffer[RESPONSE_BUFFER_SIZE];

// Estrutura para armazenar o estado do servidor TCP
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;   // Controle da conexão do servidor
    struct tcp_pcb *client_pcb;   // Controle da conexão com o cliente atual
    bool complete;                // Indica se o envio da resposta foi concluído
    uint32_t sent_len;            // Quantidade de bytes já enviados da resposta
} TCP_SERVER_T;

// Função para encerrar uma conexão TCP com o cliente
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

// Função responsável por enviar partes da resposta HTTP ao cliente
static err_t send_data(void *arg, struct tcp_pcb *tpcb) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    u16_t buffer_space = tcp_sndbuf(tpcb);  // Obtém espaço disponível no buffer TCP
    if (buffer_space == 0) return ERR_BUF;

    uint32_t remaining_len = strlen(http_response_buffer) - state->sent_len;  // Quanto falta enviar
    if (remaining_len == 0) {
        state->complete = true;
        tcp_server_close(state);
        return ERR_OK;
    }

    // Define quantos bytes serão enviados agora (não pode ultrapassar o tamanho do buffer TCP)
    u16_t len_to_send = (u16_t)(remaining_len < buffer_space ? remaining_len : buffer_space);

    err_t err = tcp_write(tpcb, http_response_buffer + state->sent_len, len_to_send, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        tcp_server_close(state);
        return err;
    }

    state->sent_len += len_to_send;  // Atualiza quantidade já enviada
    tcp_output(tpcb);                // Solicita envio imediato dos dados

    return ERR_OK;
}

// Callback chamado quando dados foram enviados com sucesso
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    return send_data(arg, tpcb);
}

// Callback chamado quando o servidor recebe dados de um cliente
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;

    if (!p) {  // Cliente fechou a conexão
        tcp_server_close(state);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len);  // Informa ao TCP que os dados foram processados

    // Verifica se a requisição recebida é do tipo HTTP GET
    if (p->tot_len >= 3 && strncmp((char*)p->payload, "GET", 3) == 0) {
        pbuf_free(p);  // Libera a memória usada pelo pacote recebido

        // Monta a resposta HTTP completa, incluindo temperatura, status do botão e log
        cyw43_arch_lwip_begin();
        snprintf(http_response_buffer, RESPONSE_BUFFER_SIZE,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n\r\n"
            "<!DOCTYPE html><html><head><title>Pico W Monitor</title>"
            "<style>body{font-family:Arial,sans-serif;text-align:center;margin-top:50px;background-color:#f0f0f0;}"
            "h1{color:#333;}.info-box{background-color:white;border-radius:10px;padding:20px;margin:20px auto;max-width:600px;box-shadow:0 4px 8px rgba(0,0,0,0.1);text-align:left;}"
            ".status{font-size:20px;margin:10px 0;}.temperature{color:#d9534f;}"
            ".log-title{font-weight:bold;margin-top:20px;}"
            ".log-area{background-color:#e9e9e9;border-left:4px solid #5cb85c;padding:10px;margin-top:5px;height:200px;overflow-y:auto;text-align:left;font-family:monospace;}"
            "</style><meta http-equiv=\"refresh\" content=\"1\"></head><body><h1>Monitoramento Raspberry Pi Pico W</h1>"
            "<div class=\"info-box\">"
            "<p class=\"status temperature\">Temperatura Atual: %.2f &deg;C</p>"
            "<p class=\"status\">Status do Botao: %s</p>"
            "<p class=\"log-title\">Registro de Eventos do Botao:</p>"
            "<div class=\"log-area\">%s</div>"
            "</div></body></html>",
            current_temperature, button_status, button_log_buffer);
        cyw43_arch_lwip_end();

        state->sent_len = 0;
        send_data(state, tpcb);

        return ERR_OK;
    }

    pbuf_free(p);  // Libera pacote recebido (não era um GET)
    return ERR_OK;
}

// Callback chamado quando uma nova conexão TCP é aceita
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || newpcb == NULL) return ERR_VAL;

    if (state->client_pcb != NULL) {  // Só aceita um cliente por vez
        tcp_abort(newpcb);
        return ERR_ABRT;
    }

    state->client_pcb = newpcb;     // Guarda controle da conexão do cliente
    tcp_arg(state->client_pcb, state);
    tcp_recv(state->client_pcb, tcp_server_recv);
    tcp_sent(state->client_pcb, tcp_server_sent);

    return ERR_OK;
}

// Inicializa o servidor TCP na porta 80
bool init_tcp_server(void) {
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) return false;

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) { free(state); return false; }

    if (tcp_bind(pcb, NULL, 80)) { free(state); return false; }

    state->server_pcb = tcp_listen(pcb);
    if (!state->server_pcb) { free(state); tcp_close(pcb); return false; }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    return true;
}

// Função que lê a temperatura interna do chip da Pico
void read_internal_temperature() {
    adc_select_input(4);  // Canal 4 é o sensor interno
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);  // Conversão de valor ADC para tensão
    float voltage = raw_value * conversion_factor;
    current_temperature = 27.0f - (voltage - 0.706f) / 0.001721f;  // Fórmula de conversão para Celsius
}

int main() {
    stdio_init_all();  // Inicializa entrada e saída padrão (USB, UART)

    if (cyw43_arch_init()) return -1;  // Inicializa o módulo Wi-Fi
    cyw43_arch_enable_sta_mode();      // Configura como Station (cliente Wi-Fi)
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) return -1;

    printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));  // Exibe o IP no console

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);  // Habilita resistor interno de pull-up no botão

    adc_init();
    adc_set_temp_sensor_enabled(true);  // Ativa o sensor de temperatura interna

    if (!init_tcp_server()) return -1;  // Inicializa o servidor TCP

    // Variáveis para controle do debounce e do log
    bool last_steady_state = true;
    bool last_flicker_state = true;
    uint32_t last_debounce_time = 0;
    uint32_t last_status_time = 0;

    while (true) {
        cyw43_arch_poll();  // Processa eventos de rede

        // Lê o estado atual do botão (com possibilidade de ruído/bouncing)
        bool current_flicker_state = gpio_get(BUTTON_PIN);

        // Detecta transições no estado do botão para iniciar contagem do debounce
        if (current_flicker_state != last_flicker_state) {
            last_debounce_time = to_ms_since_boot(get_absolute_time());
            last_flicker_state = current_flicker_state;
        }

        // Se passou tempo suficiente desde a última mudança (debounce concluído)
        if ((to_ms_since_boot(get_absolute_time()) - last_debounce_time) > DEBOUNCE_DELAY_MS) {
            if (current_flicker_state != last_steady_state) {
                last_steady_state = current_flicker_state;

                bool button_pressed_now = !last_steady_state;  // Botão pressionado quando LOW

                if (button_pressed_now) {
                    uint32_t current_time_s = to_ms_since_boot(get_absolute_time()) / 1000;

                    char new_entry[100];
                    snprintf(new_entry, sizeof(new_entry), "Botao pressionado no segundo %lu.<br>", current_time_s);

                    cyw43_arch_lwip_begin();
                    if (strstr(button_log_buffer, "Nenhum evento") != NULL) strcpy(button_log_buffer, "");
                    if (strlen(button_log_buffer) + strlen(new_entry) < LOG_BUFFER_SIZE)
                        strcat(button_log_buffer, new_entry);
                    else
                        strcpy(button_log_buffer, new_entry);
                    cyw43_arch_lwip_end();

                    gpio_put(LED_PIN, 1);  // Liga o LED quando botão pressionado
                } else {
                    gpio_put(LED_PIN, 0);  // Desliga o LED quando botão liberado
                }
            }
        }

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_status_time >= 1000) {
            last_status_time = now;
            snprintf(button_status, sizeof(button_status), "Botao %s", gpio_get(BUTTON_PIN) ? "liberado." : "pressionado!");
        }

        read_internal_temperature();  // Atualiza a temperatura interna
        sleep_ms(10);                 // Pequeno atraso para evitar uso excessivo de CPU
    }
}
