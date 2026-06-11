#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// CONFIGURAÇÕES DE REDE (Wi-Fi e MQTT)
// ==========================================

// Substitua pelo Nome (SSID) e Senha do Wi-Fi local
#define CONFIG_WIFI_SSID      "2502"
#define CONFIG_WIFI_PASSWORD  "100200300"

// Substitua pelo IP do computador rodando o Docker (Mosquitto)
#define CONFIG_MQTT_SERVER    "192.168.0.4"

// Configurações padrão do MQTT
#define CONFIG_MQTT_PORT      1883
#define CONFIG_MQTT_TOPIC     "esp32/trafego/dados"

#endif
