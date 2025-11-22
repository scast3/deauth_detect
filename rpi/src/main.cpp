#include "../include/esp32_to_uart.h"
#include <atomic>
#include <bits/stdc++.h>
#include <csignal>
#include <duckdb.hpp>
#include <fcntl.h>
#include <iostream>
#include <string>
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

std::atomic<bool> keep_running(true);

void signal_handler(int signal) {
  if (signal == SIGINT) {
    keep_running = false;
  }
}

int64_t now_us() {
  using namespace std::chrono;
  return duration_cast<microseconds>(system_clock::now().time_since_epoch())
      .count();
}

std::string bytes_to_mac(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  return std::string(buf);
}

int main() {
  signal(SIGINT, signal_handler);

  const char *portname = "/dev/serial0";
  int fd = openSerialPort(portname);
  if (fd < 0)
    return 1;

  if (!configureSerialPort(fd, B921600)) {
    close(fd);
    cerr << "Error opening serial port" << endl;
    return 1;
  }

  duckdb::DuckDB db(nullptr);
  duckdb::Connection con(db);

  con.Query("CREATE TABLE IF NOT EXISTS events (timestamp BIGINT, attack_mac "
            "VARCHAR(17), sensor_mac VARCHAR(17), rssi_mean INT, rssi_variance "
            "FLOAT, frame_count INT)");

  duckdb::Appender appender(con, "events");
  int rows = 0;
  while (keep_running) {
    wifi_deauth_event_t event;
    if (!readSerialExact(fd, &event, sizeof(event))) {
      continue;
    }

    auto attack_mac_str = bytes_to_mac(event.attack_mac);
    auto sensor_mac_str = bytes_to_mac(event.sensor_mac);
    appender.AppendRow(now_us(), attack_mac_str.c_str(), sensor_mac_str.c_str(),
                       event.rssi_mean, event.rssi_variance, event.frame_count);
    rows++;
    if (rows % 100 == 0) {
      appender.Flush();
    }
  }

  close(fd);
  appender.Close();

  auto result = con.Query("SELECT * FROM events;");
  if (result->HasError()) {
    cerr << "SELECT query failed: " << result->GetError() << endl;
  } else {
    cout << result->ToString() << endl;
  }
  return 0;
}
