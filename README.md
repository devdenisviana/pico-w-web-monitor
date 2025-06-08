# Servidor Web em Raspberry Pi Pico W para Monitoramento de Eventos 📶

Este projeto transforma um Raspberry Pi Pico W em um servidor web para monitoramento em tempo real. Ele exibe a temperatura interna do microcontrolador e registra cada pressionamento de um botão conectado, tudo em uma página web que se atualiza automaticamente.

A página web mostra:
* A temperatura atual da CPU do Pico W.
* Um log com o timestamp de cada vez que o botão foi pressionado.

Além da interface web, um LED externo acende como feedback visual sempre que o botão é pressionado.

![Exemplo da Interface Web](https://imgur.com/a/zFj6u4e) 
---

## ✨ Funcionalidades

* **Servidor Web HTTP:** Hospeda uma página web na porta 80.
* **Conectividade Wi-Fi:** Conecta-se a uma rede Wi-Fi local.
* **Log de Eventos:** Registra cada pressionamento de botão com o tempo decorrido desde a inicialização.
* **Monitoramento de Temperatura:** Lê e exibe a temperatura do sensor interno do Pico W.
* **Feedback Visual:** Aciona um LED externo simultaneamente com o pressionamento do botão.
* **Lógica de Debounce:** Evita múltiplas leituras de um único clique no botão.
* **Comunicação Robusta:** Utiliza uma implementação TCP que envia dados em pacotes, prevenindo falhas quando o log de eventos se torna muito grande.

---

## 🛠️ Hardware Necessário

* 1x Raspberry Pi Pico W
* 1x LED (qualquer cor)
* 1x Resistor de ~330Ω (para o LED)
* 1x Botão de pressão (push-button)
* 1x Protoboard (placa de ensaio)
* Fios de jumper

---

## 🔌 Montagem do Circuito

As conexões são simples e utilizam o resistor de pull-up interno do Pico para o botão.

* **LED:**
    * O pino **positivo** (perna mais longa) do LED conecta-se ao **GPIO 11** (Pino 15).
    * O pino **negativo** (perna mais curta) do LED conecta-se a uma perna do resistor de 330Ω. A outra perna do resistor conecta-se ao **GND** (qualquer pino de terra, como o Pino 3).

* **Botão:**
    * Uma perna do botão conecta-se ao **GPIO 5** (Pino 7).
    * A outra perna do botão conecta-se ao **GND** (qualquer pino de terra, como o Pino 8).

  Pico W
  +---------+
  |         |
GND --| 3       |
|         |
GND --| 8       |-- Botão -- GPIO 5 (Pino 7)
|         |
|         |
|         |-- LED -- Resistor -- GND
|         |    |
|         |-- GPIO 11 (Pino 15)
|         |
+---------+


---

## 🚀 Configuração e Compilação

Este projeto foi desenvolvido utilizando o **Pico SDK**.

### 1. Configurar Credenciais Wi-Fi

Antes de compilar, abra o arquivo `.c` e altere as seguintes linhas com os dados da sua rede Wi-Fi:

```c
#define WIFI_SSID "SuaRedeWiFi"
#define WIFI_PASSWORD "SuaSenha"
2. Arquivo CMakeLists.txt
Para compilar o projeto, você precisará de um arquivo CMakeLists.txt no mesmo diretório do seu código. Use o exemplo abaixo:

CMake

# Versão mínima do CMake
cmake_minimum_required(VERSION 3.13)

# Inicializa o projeto com o Pico SDK
include(pico_sdk_import.cmake)
project(pico_w_web_monitor C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Importa as definições do SDK
pico_sdk_init()

# Adiciona o executável
add_executable(<span class="math-inline">\{PROJECT\_NAME\}
seu\_arquivo\.c \# <\-\- TROQUE PELO NOME DO SEU ARQUIVO \.C
\)
\# Habilita o suporte a Wi\-Fi no Pico W
pico\_enable\_sdk\_section\("pico\_cyw43\_arch\_lwip\_threadsafe\_background"\)
\# Adiciona as bibliotecas necessárias
target\_link\_libraries\(</span>{PROJECT_NAME}
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_adc
)

# Habilita a saída USB e UART para `printf`
pico_add_extra_outputs(${PROJECT_NAME})
3. Compilar o Projeto
Com o ambiente do Pico SDK configurado, navegue até o diretório do projeto e execute os seguintes comandos:

Bash

# Cria um diretório de compilação
mkdir build
cd build

# Prepara os arquivos de compilação
cmake ..

# Compila o projeto
make
Isso irá gerar um arquivo .uf2 dentro da pasta build.

💻 Uso
Flashear o Pico W: Pressione e segure o botão BOOTSEL no seu Pico W enquanto o conecta ao computador. Ele aparecerá como um dispositivo de armazenamento.
Arraste e solte o arquivo ${PROJECT_NAME}.uf2 (gerado na pasta build) para dentro do Pico. A placa irá reiniciar automaticamente.
Descobrir o IP: Abra um monitor serial (usando PuTTY, o terminal do Thonny, ou o Serial Monitor do VS Code) para ver as mensagens de log. Assim que o Pico W se conectar à rede, ele imprimirá seu endereço IP.
Conectado com sucesso!
IP do dispositivo: 192.168.1.XX
Acessar a Página: Em um navegador web (no celular ou computador) na mesma rede Wi-Fi, acesse o endereço IP exibido.
Testar: Pressione o botão e veja o LED acender e o log de eventos ser atualizado na página web!
📜 Licença
Este projeto está sob a licença MIT. Veja o arquivo LICENSE para mais detalhes.