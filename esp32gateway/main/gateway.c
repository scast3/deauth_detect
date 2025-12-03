#include "esp_err.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <driver/uart.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Define event struct to reflect sensor struct
typedef struct __attribute__((packed)) {
  uint8_t attack_mac[6];
  uint8_t sensor_mac[6];
  int8_t rssi_mean;
  float rssi_variance;
  int frame_count;
  uint64_t timestamp;
} wifi_deauth_event_t;
// UART STUFF
static const int uart_buffer_size = (1024 * 64);
static QueueHandle_t uart_queue = NULL;
static const uart_port_t uart_num = UART_NUM_2;
#define TXD_PIN 17
#define RXD_PIN 16

void init_uart() {
  uart_config_t uart_config = {
      .baud_rate = 921600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
  };

  ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
  ESP_ERROR_CHECK(uart_driver_install(uart_num, uart_buffer_size,
                                      uart_buffer_size, 10, &uart_queue, 0));
  ESP_ERROR_CHECK(uart_set_pin(uart_num, TXD_PIN, RXD_PIN, -1, -1));

  // Write data to UART.
  // char *test_str = "This is a test string.\n";
  // while (1) {
  // sleep(2);
  // uart_write_bytes(uart_num, (const char *)test_str, strlen(test_str));
  //}
}

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
  if (len != sizeof(wifi_deauth_event_t)) {
    printf("Received invalid data length: %d\n", len);
    return;
  }
  wifi_deauth_event_t event;
  memcpy(&event, data, sizeof(wifi_deauth_event_t));

  // event.timestamp = (int64_t)esp_timer_get_time();

  printf("Deauth event received on gateway.\n");
  printf("Attacker MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", event.attack_mac[0],
         event.attack_mac[1], event.attack_mac[2], event.attack_mac[3],
         event.attack_mac[4], event.attack_mac[5]);
  printf("From Sensor MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
         event.sensor_mac[0], event.sensor_mac[1], event.sensor_mac[2],
         event.sensor_mac[3], event.sensor_mac[4], event.sensor_mac[5]);
  printf("RSSI: %d dBm\n", event.rssi_mean);

  uart_write_bytes(uart_num, &event, sizeof(event));
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
  init_uart();
  init_gateway();
  printf("Receiver is listening for ESP-NOW deauth events...\n");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
