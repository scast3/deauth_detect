#include "esp_event.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <time.h>

static const int DEAUTH_THRESH = 500;
static volatile int deauth_count = 0;

// MAC headers
// Maybe include type and subtype here instead
typedef struct {
  unsigned frame_ctrl : 16;
  unsigned duration_id : 16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl : 16;
  uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

// MAC header + payload
// Maybe merge these?
typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

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

// Convert type of packet to string
// Will prob delete this later
const char *wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type) {
  switch (type) {
  case WIFI_PKT_MGMT:
    return "MGMT";
  case WIFI_PKT_DATA:
    return "DATA";
  case WIFI_PKT_CTRL:
    return "CTRL";
  case WIFI_PKT_MISC:
  default:
    return "MISC";
  }
}

// Sniff for deauth frames
// Alert if number of deauth frames exceed threshold
// Extend later for time window for greater accuracy
void wifi_sniffer_packet_handler(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT)
    return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  const wifi_ieee80211_packet_t *ipkt =
      (wifi_ieee80211_packet_t *)ppkt->payload;
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

  uint16_t fc = hdr->frame_ctrl;
  // shift the 16-bit field right by 2 bits so that bits 2–3 move to bits 0–1
  //& 0x3 → mask with 0b11 to only keep those 2 bits
  uint8_t type_field = (fc >> 2) & 0x3;

  // shift the 16-bit field right by 2 bits so that bits 2–3 move to
  // bits 0–1 & 0x3 → mask with 0b11 to only keep those 2 bits
  uint8_t subtype = (fc >> 4) & 0xF;

  if (type_field != 0)
    return;
  if (subtype != 0xC)
    return;

  deauth_count++;
  if (deauth_count >= DEAUTH_THRESH) {
    printf("DEAUTHENTICATION THRESHOLD EXCEEDED. DEAUTHENTICATION ATTACK "
           "DETECTED\n");
    deauth_count = 0;
  }

  /*  printf("DEAUTH #%d CH=%02d RSSI=%d\n"
           "  SA=%02x:%02x:%02x:%02x:%02x:%02x\n"
           "  DA=%02x:%02x:%02x:%02x:%02x:%02x\n",
           deauth_count, ppkt->rx_ctrl.channel, ppkt->rx_ctrl.rssi,
           hdr->addr2[0], hdr->addr2[1], hdr->addr2[2], hdr->addr2[3],
           hdr->addr2[4], hdr->addr2[5],
           hdr->addr1[0], hdr->addr1[1], hdr->addr1[2], hdr->addr1[3],
           hdr->addr1[4], hdr->addr1[5]); */
}

// Initialize in promiscous mode
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

  // Main loop
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(500)); // Keep the task alive
  }
}
