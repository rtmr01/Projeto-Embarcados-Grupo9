#  Sistema Embarcado de Controle de Tráfego - ESP32

[📄 Clique aqui para ler o Relatório Técnico Completo em PDF](./docs/Relatorio_Tecnico.pdf)

Este repositório contém a arquitetura completa de software desenvolvida para um dispositivo de monitoramento de tráfego, abrangendo desde o firmware do microcontrolador até a infraestrutura de rede e dashboard web.

##  Estrutura do Projeto

O projeto é dividido em 3 camadas principais:

- `/esp32-esp8266`: **Firmware (ESP32)** desenvolvido em C/C++ usando o framework Arduino (PlatformIO).
- `/applications/server`: **Infraestrutura Docker** rodando Eclipse Mosquitto (Broker MQTT) e Node-RED localmente.
- `/applications/dashboard`: **Interface Web (Vite)** para visualização da telemetria e gráficos em tempo real.
- `/docs`: Documentações do projeto (ex: relatórios).
- `/schematics`: Diagramas e esquemáticos dos circuitos físicos.

---

## 1.  Firmware do Dispositivo (ESP32)

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

### ⚙️ Como configurar a Rede (Wi-Fi e MQTT)

Caso você mude de ambiente (ex: da faculdade para casa), é necessário atualizar as credenciais do Wi-Fi e o IP do servidor MQTT para que a placa consiga se conectar.
Para facilitar, as variáveis de rede foram isoladas no arquivo `esp32-esp8266/src/config.h`. 

Basta abrir esse arquivo, atualizar os dados e fazer um novo upload para a placa ESP32:
```cpp
#define CONFIG_WIFI_SSID      "SEU_WIFI"
#define CONFIG_WIFI_PASSWORD  "SUA_SENHA"
#define CONFIG_MQTT_SERVER    "IP_DO_SEU_COMPUTADOR"
```

---

## 2.  Mensageria e Comunicação (MQTT)

A conectividade do sistema (IoT) é feita via WiFi utilizando o protocolo MQTT. O ESP32 usa a biblioteca `PubSubClient` para conexão.

- **Payload JSON:** Eventos (Eixo, Recalibração ou intervalo) são transmitidos no formato estruturado:
  ```json
  {"estado":"OPERACAO", "pressaoV": 2.15, "pmedia": 2.10, "potV": 1.5, "potN": 0.45, "x": 2.18, "eixos": 14, "evento": "AXLE"}
  ```
- **Tópico MQTT:** Todos os dados fluem pelo tópico `esp32/trafego/dados`.

---

## 3.  Infraestrutura de Servidores (Docker)

O sistema roda de forma 100% local com latência mínima utilizando containers gerenciados pelo Docker Compose.

- **Eclipse Mosquitto (Broker MQTT):** Recebe publicações (TCP porta `1883`) e as distribui. Usa a porta `9001` para WebSockets, permitindo conexão direta dos navegadores web.
- **Node-RED:** Acessível na porta `1880`. Ambiente visual inscrito no MQTT para depuração de pacotes e criação de futuras automações (integração com bancos de dados, envio de e-mails, etc).
  
<img width="468" height="136" alt="Captura de Tela 2026-06-11 às 18 44 09" src="https://github.com/user-attachments/assets/cfbd7033-440e-45d2-ae57-882339619a02" />

---

## 4.  Interface do Usuário (Dashboard Web)

Painel desenvolvido do zero para monitoramento por operadores, focado em clareza e alta performance.

- **Stack Tecnológica:** Construído sobre **Vite** usando JavaScript Vanilla, HTML5 e CSS3.
- **Comunicação Direta:** Utiliza `mqtt.js` para estabelecer um WebSocket direto com o Mosquitto, dispensando um backend intermediário (API).
- **UI/UX e Funcionalidades:**
  - Estética baseada em *Glassmorphism* e Dark Mode.
  - Placa fixa para leitura isolada do valor da voltagem atual do sensor.
  - Gráfico dinâmico construído com **Chart.js**, plotando a Pressão no Eixo Y em função do tempo no Eixo X, de forma contínua e em tempo real.
  <img width="1470" height="956" alt="Captura de Tela 2026-06-11 às 18 43 38" src="https://github.com/user-attachments/assets/73d2d51c-c305-43bc-98ed-82547968c84f" />

 
  ## 5.  Imagens do protótipo físico
  
<img width="1200" height="1600" alt="image" src="https://github.com/user-attachments/assets/d636d5c7-a94a-4a39-aa3d-e4bbae1ea815" />
<img width="1200" height="1600" alt="image" src="https://github.com/user-attachments/assets/834cb2cd-8527-4693-a82b-915e5a73bd7f" />
<img width="1600" height="1200" alt="image" src="https://github.com/user-attachments/assets/24e03324-e211-40c1-9953-756fb1af6e52" />
<img width="1200" height="1600" alt="image" src="https://github.com/user-attachments/assets/3b147bb1-6e11-4f4a-be9e-a43514ca0d5b" />
<img width="1200" height="1600" alt="image" src="https://github.com/user-attachments/assets/ea3bee1b-3fad-4be8-8566-014ed5fa2755" />
<img width="1200" height="1600" alt="image" src="https://github.com/user-attachments/assets/9d19d644-72cf-4d7f-a380-1aef0db15a05" />
<img width="1200" height="1600" alt="image" src="https://github.com/user-attachments/assets/64c98728-006a-47c5-ad4d-0c4533b5ef5e" />



