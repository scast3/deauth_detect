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

static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (len != sizeof(deauth_espnow_t)) return;
    deauth_espnow_t pkt;
    memcpy(&pkt, data, len);
    printf("RECV: MAC %02x:%02x:%02x:%02x:%02x:%02x RSSI %d TS %u CNT %u\n",
           pkt.attack_mac[0], pkt.attack_mac[1], pkt.attack_mac[2],
           pkt.attack_mac[3], pkt.attack_mac[4], pkt.attack_mac[5],
           pkt.rssi, pkt.ts_ms, pkt.count);
}

void init_espnow_receiver(void)
{
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, ESP_NOW_BROADCAST_ADDR, 6);
    peer.channel = ESP_NOW_CHANNEL;
    peer.ifidx = ESP_IF_WIFI_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}
