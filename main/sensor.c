#include "esp_event.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const int DEAUTH_THRESH = 1000; // Tune this
static const int TIME_WIND_MS = 10000; // Tune this
static const int64_t TIME_WIND_US = (int64_t)TIME_WIND_MS * 1000LL;
#define MAX_DEAUTH_BUFFER 1024

static volatile int total_deauths = 0;
static int64_t deauth_times[MAX_DEAUTH_BUFFER];
static int deauth_head = 0;
static int deauth_tail = 0;

typedef struct {
  uint8_t attack_mac[6];
  int8_t attack_rssi;
} deauth_alert_t;

// Flash helper function
// Apply settings
void init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

// Sniff for deauth frames
// Alert if number of deauth frames exceed threshold within time window
// Core
void wifi_sniffer_packet_handler(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT)
    return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;

  if (ppkt->rx_ctrl.sig_len < 24)
    return;

  uint16_t frame_ctrl = (ppkt->payload[1] << 8) | ppkt->payload[0];

  uint8_t type_field = (frame_ctrl >> 2) & 0x3;
  uint8_t subtype = (frame_ctrl >> 4) & 0xF;

  if (type_field != 0)
    return;
  if (subtype != 0xC) // 0xC == deauth
    return;

  uint8_t recv_mac[6];
  memcpy(recv_mac, &ppkt->payload[4], 6);
  uint8_t send_mac[6];
  memcpy(send_mac, &ppkt->payload[10], 6);

  total_deauths++;
  int64_t now = esp_timer_get_time();

  while (deauth_head != deauth_tail &&
         deauth_times[deauth_head] < now - TIME_WIND_US) {
    deauth_head = (deauth_head + 1) % MAX_DEAUTH_BUFFER;
  }

  if ((deauth_tail + 1) % MAX_DEAUTH_BUFFER == deauth_head) {
    deauth_head = (deauth_head + 1) % MAX_DEAUTH_BUFFER;
  }

  deauth_times[deauth_tail] = now;
  deauth_tail = (deauth_tail + 1) % MAX_DEAUTH_BUFFER;

  int current_count = (deauth_tail >= deauth_head)
                          ? deauth_tail - deauth_head
                          : deauth_tail - deauth_head + MAX_DEAUTH_BUFFER;

  // Alert
  // Construct alert and reset buffer
  if (current_count >= DEAUTH_THRESH) {
    deauth_alert_t *alert = malloc(sizeof(*alert));
    memcpy(alert->attack_mac, send_mac, sizeof(alert->attack_mac));
    alert->attack_rssi = ppkt->rx_ctrl.rssi;

    printf("=============================================================\n"
           "De-authentication frame rate exceeded!\n"
           "De-authentication attack detected!\n"
           "Frame rate (deauth frames/s) = %d\n"
           "RSSI = %d\n"
           "Attacker MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
           DEAUTH_THRESH / (TIME_WIND_MS / 1000), alert->attack_rssi,
           alert->attack_mac[0], alert->attack_mac[1], alert->attack_mac[2],
           alert->attack_mac[3], alert->attack_mac[4], alert->attack_mac[5]);

    free(alert);
    deauth_head = deauth_tail = 0;
  }
}

// Initialize in promiscuous mode
void init_wifi_sniffer() {
  init_nvs();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  ESP_ERROR_CHECK(
      esp_wifi_set_channel(CONFIG_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler));
  ESP_ERROR_CHECK(esp_wifi_start());
  printf("Sniffer is running on channel %d\n", CONFIG_WIFI_CHANNEL);
}

void app_main(void) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  init_wifi_sniffer();

  printf("Sniffer is running in monitor mode. Capturing packets...\n");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000)); // Keep the task alive
  }
}
