#include "esp_event.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Optional: print less frequently
#define PRINT_EVERY_N_PACKETS 1

// Flash helper function
void init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

// --- Helper functions ---
static const char *wifi_pkt_type_name(wifi_promiscuous_pkt_type_t type) {
  switch (type) {
  case WIFI_PKT_MGMT:
    return "Management";
  case WIFI_PKT_CTRL:
    return "Control";
  case WIFI_PKT_DATA:
    return "Data";
  case WIFI_PKT_MISC:
    return "Misc";
  default:
    return "Unknown";
  }
}

static const char *mgmt_subtype_name(uint8_t subtype) {
  switch (subtype) {
  case 0x0:
    return "Association Request";
  case 0x1:
    return "Association Response";
  case 0x2:
    return "Reassociation Request";
  case 0x3:
    return "Reassociation Response";
  case 0x4:
    return "Probe Request";
  case 0x5:
    return "Probe Response";
  case 0x8:
    return "Beacon";
  case 0xA:
    return "Disassociation";
  case 0xB:
    return "Authentication";
  case 0xC:
    return "Deauthentication";
  default:
    return "Other Management";
  }
}

// --- Main packet handler ---
void wifi_sniffer_packet_handler(void *buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *payload = ppkt->payload;
  int len = ppkt->rx_ctrl.sig_len;

  if (len < 24)
    return; // too short to parse

  // Extract 802.11 frame control fields
  uint16_t frame_ctrl = (payload[1] << 8) | payload[0];
  uint8_t frame_type = (frame_ctrl >> 2) & 0x3;
  uint8_t frame_subtype = (frame_ctrl >> 4) & 0xF;

  // Extract MAC addresses if available
  uint8_t addr1[6], addr2[6], addr3[6];
  memcpy(addr1, &payload[4], 6);  // destination
  memcpy(addr2, &payload[10], 6); // source
  memcpy(addr3, &payload[16], 6); // BSSID (if present)

  static int pkt_counter = 0;
  if (++pkt_counter % PRINT_EVERY_N_PACKETS != 0)
    return;

  printf("=============================================================\n");
  printf("Frame #%d\n", pkt_counter);
  printf("Type: %s (%d) | Subtype: %d ", wifi_pkt_type_name(type),
         frame_type, frame_subtype);
  if (type == WIFI_PKT_MGMT)
    printf("(%s)\n", mgmt_subtype_name(frame_subtype));
  else
    printf("\n");

  printf("RSSI: %d dBm | Length: %d bytes\n", ppkt->rx_ctrl.rssi, len);

  printf("Source      : %02x:%02x:%02x:%02x:%02x:%02x\n",
         addr2[0], addr2[1], addr2[2], addr2[3], addr2[4], addr2[5]);
  printf("Destination : %02x:%02x:%02x:%02x:%02x:%02x\n",
         addr1[0], addr1[1], addr1[2], addr1[3], addr1[4], addr1[5]);
  printf("BSSID       : %02x:%02x:%02x:%02x:%02x:%02x\n",
         addr3[0], addr3[1], addr3[2], addr3[3], addr3[4], addr3[5]);
}

// --- Initialize in promiscuous mode ---
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

  printf("Sniffer is running in monitor mode. Capturing all packets...\n");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000)); // Keep the task alive
  }
}
