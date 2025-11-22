#include "../include/esp32_to_uart.h"
#include <duckdb.hpp>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>
using namespace std;

struct wifi_deauth_event_t {
  uint8_t attack_mac[6];
  uint8_t sensor_mac[6];
  int8_t rssi_mean;
  float rssi_variance;
  int frame_count;
};

int main() {
  /*
  duckdb::DuckDB db(nullptr);
  duckdb::Connection con(db);

  // create a table
  con.Query("CREATE TABLE integers (i INTEGER, j INTEGER)");

  // insert three rows into the table
  con.Query("INSERT INTO integers VALUES (3, 4), (5, 6), (7, NULL)");

  auto result = con.Query("SELECT * FROM integers");
  if (result->HasError()) {
          cerr << result->GetError() << endl;
  } else {
          cout << result->ToString() << endl;
  }
  */

  const char *portname = "/dev/serial0";
  int fd = openSerialPort(portname);
  if (fd < 0)
    return 1;

  if (!configureSerialPort(fd, B921600)) {
    close(fd);
    cerr << "Error opening serial port" << endl;
    return 1;
  }

  while (true) {
    wifi_deauth_event_t event;
    if (!readSerialExact(fd, &event, sizeof(event))) {
      continue;
    }

    printf("Attack MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", event.attack_mac[0],
           event.attack_mac[1], event.attack_mac[2], event.attack_mac[3],
           event.attack_mac[4], event.attack_mac[5]);

    printf("Sensor MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", event.sensor_mac[0],
           event.sensor_mac[1], event.sensor_mac[2], event.sensor_mac[3],
           event.sensor_mac[4], event.sensor_mac[5]);

    printf("RSSI mean: %d\n", event.rssi_mean);
    printf("RSSI variance: %f\n", event.rssi_variance);
    printf("Frame count: %d\n", event.frame_count);
  }

  close(fd);
  return 0;
}
