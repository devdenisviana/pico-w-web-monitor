# 📶 Servidor Web em Raspberry Pi Pico W para Monitoramento de Eventos

Este projeto transforma um **Raspberry Pi Pico W** em um **servidor web embarcado** para **monitoramento em tempo real** de temperatura e eventos de botão.

### 🔧 Funcionalidade principal:

* Exibe a **temperatura interna** do microcontrolador.
* Registra **cada pressionamento do botão** com **timestamp**.
* Página web atualiza automaticamente a cada segundo.
* Um **LED** pisca quando o botão é pressionado.

![Exemplo da Interface Web](https://i.imgur.com/LjhkjAr.png)

---

## ✨ Funcionalidades

✅ **Servidor Web HTTP:** Página web hospedada na porta 80.
✅ **Conexão Wi-Fi:** Conecta-se automaticamente à sua rede.
✅ **Log de Eventos:** Registra pressionamentos de botão com tempo decorrido.
✅ **Monitoramento de Temperatura:** Leitura contínua do sensor interno.
✅ **Feedback Visual (LED):** LED acende durante o pressionamento do botão.
✅ **Debounce de Botão:** Evita leituras falsas ou múltiplas.
✅ **Comunicação TCP Segura:** Fragmenta a resposta quando o log de eventos fica grande.

---

## 🛠️ Hardware Necessário

* 1x Raspberry Pi Pico W
* 1x LED (qualquer cor)
* 1x Resistor (\~330Ω)
* 1x Botão (push-button)
* 1x Protoboard
* Fios jumper

---

## 🔌 Montagem do Circuito

| **Componente** | **Conexão**       |
| -------------- | ----------------- |
| **LED (+)**    | GPIO 11 (Pino 15) |
| **LED (-)**    | Resistor -> GND   |
| **Botão 1**    | GPIO 5 (Pino 7)   |
| **Botão 2**    | GND               |

---

## 🚀 Configuração e Compilação

### 1️⃣ Configurar Wi-Fi

No arquivo `.c`:

```c
#define WIFI_SSID "SeuSSID"
#define WIFI_PASSWORD "SuaSenha"
```

### 2️⃣ Arquivo `CMakeLists.txt` Exemplo:

```cmake
cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(pico_w_web_monitor C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    seu_arquivo.c
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_enable_sdk_section(${PROJECT_NAME} "pico_cyw43_arch_lwip_threadsafe_background")

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_adc
)

pico_add_extra_outputs(${PROJECT_NAME})
```

### 3️⃣ Compilação:

```bash
mkdir build
cd build
cmake ..
make
```

---

## 💻 Como Usar

1️⃣ **Gravar no Pico W:**
Pressione e segure **BOOTSEL** → conecte ao PC → copie o `.uf2` para o dispositivo.

2️⃣ **Descobrir o IP:**
Abra um terminal serial para visualizar o IP. Exemplo de mensagem exibida após a conexão:

```
IP Address: 192.168.1.XX
```

⚠️ **Importante:** O endereço IP exibido **depende da sua rede**. Você verá o IP no terminal após a conexão.

3️⃣ **Acessar via Navegador:**
Digite o IP no navegador do seu celular ou computador conectado à mesma rede.

**Exemplo:**

```
http://192.168.1.25
```

4️⃣ **Teste:**
Pressione o botão → veja o LED acender → a página exibe o evento e atualiza automaticamente.

---

## 📂 Organização do Código

| **Seção**           | **Descrição**                                                              |
| ------------------- | -------------------------------------------------------------------------- |
| 📦 **Inclusões**    | Bibliotecas do Pico, Wi-Fi, ADC, TCP/IP.                                   |
| 🌐 **Servidor TCP** | Configura porta 80, aceita conexões, processa GET e envia a página HTML.   |
| 🌡 **Temperatura**  | Função `read_internal_temperature()` lê o sensor interno via ADC.          |
| 🔘 **Botão**        | Lógica de debounce → gera log → atualiza status → acende/desliga LED.      |
| 🖥 **Página HTML**  | Interface HTML com estilos, log de eventos, status, temperatura e refresh. |
| ♻️ **Loop**         | Atualiza temperatura, verifica botão e mantém comunicação TCP funcionando. |

---

## 📜 Licença

MIT License – veja o arquivo LICENSE para detalhes.


