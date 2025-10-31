#include "esp_err.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Initialize global constant variables
static const int DEAUTH_THRESH = 1000; // tune
static const int TIME_WIND_MS = 10000; // tune
static const int64_t TIME_WIND_US =
    (int64_t)TIME_WIND_MS * 1000LL; // esp_timer_get_time works in microseconds
                                    // so this value is used for comparisons
#define MAX_DEAUTH_BUFFER 1024
#define ALERT_QUEUE_LEN                                                        \
  8 // Tune this (ISR blocking/overflowing queue with packets)

// Initialize global "volatile" vairables
static volatile int total_deauths = 0;
static int64_t deauth_times[MAX_DEAUTH_BUFFER];
static int deauth_head = 0; // Oldest timestamp
static int deauth_tail = 0; // Newest timestamp
uint8_t gateway_address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Initialize variables for ESP-NOW
static QueueHandle_t alert_queue = NULL;
static esp_now_peer_info_t peer_info;

// Define alert struct to be sent over ESP-NOW
typedef struct {
  uint8_t attack_mac[6];
  uint8_t sensor_mac[6];
  int8_t attack_rssi;
} deauth_alert_t;

// Initialize nvs storage partition
// Store configuration settings such as WiFi channel
void init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

// ESP-NOW sending callback function
// Automatically called by ESP-NOW after it attempts to send a packet
void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
  printf("\r\nLast Packet Send Status:\t%s\n",
         status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Background task dedicated to sending alerts
// FreeRTOS task/Core 1
void espnow_sender_task(void *arg) {
  deauth_alert_t alert;
  while (1) {
    // Attempt to take one item from alert_queue
    // & alert is where the alert will be copied
    // portMAX_DELAY tells FreeRTOS to block this task until an item is
    // available
    if (xQueueReceive(alert_queue, &alert, portMAX_DELAY) == pdTRUE) {
      esp_err_t r =
          esp_now_send(gateway_address, (uint8_t *)&alert, sizeof(alert));
      if (r == ESP_OK) {
        printf("esp_now_send queued OK\n");
      } else {
        printf("esp_now_send failed: %d\n", r);
      }
    }
  }
}

// Callback each time a packet is received
// Core of sensor system
// Promiscuous mode core
// Sniffer callback
// ISR/CORE 0
void wifi_promiscuous_packet_handler(void *buf,
                                     wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT)
    return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  if (!ppkt)
    return;

  uint16_t frame_ctrl = (ppkt->payload[1] << 8) | ppkt->payload[0];
  uint8_t type_field = (frame_ctrl >> 2) & 0x3;
  uint8_t subtype = (frame_ctrl >> 4) & 0xF;

  if (type_field != 0)
    return; // management
  if (subtype != 0xC)
    return; // deauth

  uint8_t send_mac[6];
  memcpy(send_mac, &ppkt->payload[10], 6);

  total_deauths++;
  int64_t now = esp_timer_get_time();

  // Prune timestamps in deauth_times older than TIME_WIND
  // %MAX_DEAUTH_BUFFER for wraparound
  while (deauth_head != deauth_tail &&
         deauth_times[deauth_head] < now - TIME_WIND_US) {
    deauth_head = (deauth_head + 1) % MAX_DEAUTH_BUFFER;
  }

  // Check if buffer is full by incrementing tail by 1
  // If so, increment deauth_head by 1 to make space
  if ((deauth_tail + 1) % MAX_DEAUTH_BUFFER == deauth_head) {
    deauth_head = (deauth_head + 1) % MAX_DEAUTH_BUFFER;
  }

  deauth_times[deauth_tail] = now;
  deauth_tail = (deauth_tail + 1) % MAX_DEAUTH_BUFFER;

  // Calculates total number of items in buffer
  // (deauths in time window)
  int current_count = (deauth_tail >= deauth_head)
                          ? deauth_tail - deauth_head
                          : deauth_tail - deauth_head + MAX_DEAUTH_BUFFER;

  // Attack detection logic
  if (current_count >= DEAUTH_THRESH) {
    deauth_alert_t alert;
    memcpy(alert.attack_mac, send_mac, sizeof(alert.attack_mac));
    alert.attack_rssi = ppkt->rx_ctrl.rssi;
    esp_wifi_get_mac(WIFI_IF_STA, alert.sensor_mac);

    // Send alert to FreeRTOS queue
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(alert_queue, &alert, &xHigherPriorityTaskWoken) !=
        pdTRUE) {
      printf("Alert queue full! Increase the size of ALERT_QUEUE_LEN!\n");
    }
    if (xHigherPriorityTaskWoken)
      portYIELD_FROM_ISR();
    deauth_head = deauth_tail = 0;
  }
}

// Inialize:
//   WiFi Config
//   WiFi mode (station)
//   WiFi promiscuous mode
//   WiFi channel
//   Promiscuous callback function
//   Start WiFi
//   ESP-NOW
//   ESP-NOW sending callback function
void init_wifi_sniffer() {
  init_nvs();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  // ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(WIFI_PKT_MGMT));
  ESP_ERROR_CHECK(
      esp_wifi_set_channel(CONFIG_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
  ESP_ERROR_CHECK(
      esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_packet_handler));
  ESP_ERROR_CHECK(esp_wifi_start());
  printf("Sniffer is running on channel %d\n", CONFIG_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    printf("Error initializing ESP-NOW\n");
    return;
  }
  printf("ESP-NOW successfully initialized\n");
  esp_now_register_send_cb((esp_now_send_cb_t)send_cb);

  memset(&peer_info, 0, sizeof(peer_info));
  memcpy(peer_info.peer_addr, gateway_address, 6);
  peer_info.channel = CONFIG_WIFI_CHANNEL; // or 0
  peer_info.encrypt = false;

  if (esp_now_add_peer(&peer_info) != ESP_OK) {
    printf("Failed to add peer\n");
    // optionally continue or return
  } else {
    printf("ESP-NOW receiver successfully added as a peer\n");
  }
}

void app_main(void) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  alert_queue = xQueueCreate(ALERT_QUEUE_LEN, sizeof(deauth_alert_t));
  if (alert_queue == NULL) {
    printf("Failed to create alert queue\n");
    return;
  }
  xTaskCreate(espnow_sender_task, "espnow_sender", 4096, NULL, 5, NULL);

  init_wifi_sniffer();

  printf("Sniffer is running in monitor mode. Capturing packets...\n");
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
