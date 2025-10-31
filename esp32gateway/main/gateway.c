#include "esp_err.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Define alert struct to reflect sensor struct
typedef struct {
  uint8_t attack_mac[6];
  uint8_t sensor_mac[6];
  int8_t attack_rssi;
} deauth_alert_t;

// Init nvs
void init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

// Receiver callback
// Called whenever data is received from sensors
void recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len) {
  if (len != sizeof(deauth_alert_t)) {
    printf("Received invalid data length: %d\n", len);
    return;
  }
  deauth_alert_t alert;
  memcpy(&alert, data, sizeof(deauth_alert_t));

  printf("Deauth Attack Detected!\n");
  printf("Attacker MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", alert.attack_mac[0],
         alert.attack_mac[1], alert.attack_mac[2], alert.attack_mac[3],
         alert.attack_mac[4], alert.attack_mac[5]);
  printf("From Sensor MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
         alert.sensor_mac[0], alert.sensor_mac[1], alert.sensor_mac[2],
         alert.sensor_mac[3], alert.sensor_mac[4], alert.sensor_mac[5]);
  printf("RSSI: %d dBm\n", alert.attack_rssi);
}

// Inialize:
//   WiFi Config
//   WiFi mode (station)
//   WiFi channel
//   ESP-NOW
//   ESP-NOW receiving callback function
void init_gateway() {
  init_nvs();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start()); // Start WiFi before setting channel
  ESP_ERROR_CHECK(
      esp_wifi_set_channel(CONFIG_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
  printf("Receiver running on channel %d\n", CONFIG_WIFI_CHANNEL);
  if (esp_now_init() != ESP_OK) {
    printf("Error initializing ESP-NOW\n");
    return;
  }
  printf("ESP-NOW successfully initialized\n");
  ESP_ERROR_CHECK(esp_now_register_recv_cb((esp_now_recv_cb_t)recv_cb));
}

// Main function
void app_main(void) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  init_gateway();
  printf("Receiver is listening for ESP-NOW alerts...\n");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
