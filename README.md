# 🚦 Sistema Embarcado de Controle de Tráfego - ESP32

Este repositório contém a arquitetura completa de software desenvolvida para um dispositivo de monitoramento de tráfego, abrangendo desde o firmware do microcontrolador até a infraestrutura de rede e dashboard web.

## 📁 Estrutura do Projeto

O projeto é dividido em 3 camadas principais:

- `/src`, `/include`, `/lib`: **Firmware (ESP32)** desenvolvido em C/C++ usando o framework Arduino (PlatformIO).
- `/server`: **Infraestrutura Docker** rodando Eclipse Mosquitto (Broker MQTT) e Node-RED localmente.
- `/dashboard`: **Interface Web (Vite)** para visualização da telemetria e gráficos em tempo real.

---

## 1. 🧠 Firmware do Dispositivo (ESP32)

O coração do sistema embarcado opera baseado em uma **Máquina de Estados Finita** para não bloquear o processamento e garantir precisão:

- **Sensores:** 
  - **Sensor MAP (Pressão):** Lê o sinal analógico de pressão exercida na via.
  - **Potenciômetro:** Calibra a margem de disparo (Threshold) que contabiliza os eixos.
- **Estados da Máquina:**
  - `INIT`: Inicialização do hardware (LEDs, SD Card, Watchdog).
  - `AGUARDANDO_PRESSAO`: Aguarda o sinal do MAP entrar em faixa operacional (VMIN a VMAX).
  - `ESTAVEL`: Valida a estabilidade do sinal.
  - `COLETA_MEDIA`: Coleta amostras para calcular uma Pressão Média Base (PMedia).
  - `OPERACAO`: Estado de medição contínua. Caso a pressão ultrapasse a histerese (`PMedia + Fator`), registra um `EIXO (AXLE)`.
- **Segurança (Data Logging):**
  - **SD Card:** Backup local em arquivo CSV (barramento SPI) contra falhas de rede.
  - **Watchdog Timer (WDT):** Reinicialização automática de segurança contra travamentos.

---

## 2. 📡 Mensageria e Comunicação (MQTT)

A conectividade do sistema (IoT) é feita via WiFi utilizando o protocolo MQTT. O ESP32 usa a biblioteca `PubSubClient` para conexão.

- **Payload JSON:** Eventos (Eixo, Recalibração ou intervalo) são transmitidos no formato estruturado:
  ```json
  {"estado":"OPERACAO", "pressaoV": 2.15, "pmedia": 2.10, "potV": 1.5, "potN": 0.45, "x": 2.18, "eixos": 14, "evento": "AXLE"}
  ```
- **Tópico MQTT:** Todos os dados fluem pelo tópico `esp32/trafego/dados`.

---

## 3. 🐳 Infraestrutura de Servidores (Docker)

O sistema roda de forma 100% local com latência mínima utilizando containers gerenciados pelo Docker Compose.

- **Eclipse Mosquitto (Broker MQTT):** Recebe publicações (TCP porta `1883`) e as distribui. Usa a porta `9001` para WebSockets, permitindo conexão direta dos navegadores web.
- **Node-RED:** Acessível na porta `1880`. Ambiente visual inscrito no MQTT para depuração de pacotes e criação de futuras automações (integração com bancos de dados, envio de e-mails, etc).

---

## 4. 📊 Interface do Usuário (Dashboard Web)

Painel desenvolvido do zero para monitoramento por operadores, focado em clareza e alta performance.

- **Stack Tecnológica:** Construído sobre **Vite** usando JavaScript Vanilla, HTML5 e CSS3.
- **Comunicação Direta:** Utiliza `mqtt.js` para estabelecer um WebSocket direto com o Mosquitto, dispensando um backend intermediário (API).
- **UI/UX e Funcionalidades:**
  - Estética baseada em *Glassmorphism* e Dark Mode.
  - Placa fixa para leitura isolada do valor da voltagem atual do sensor.
  - Gráfico dinâmico construído com **Chart.js**, plotando a Pressão no Eixo Y em função do tempo no Eixo X, de forma contínua e em tempo real.
