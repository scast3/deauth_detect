#include "../include/esp32_to_uart.h"
#include <atomic>
#include <csignal>
#include <duckdb.hpp>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

using namespace std;

struct wifi_deauth_event_t {
  uint8_t attack_mac[6];
  uint8_t sensor_mac[6];
  int8_t rssi_mean;
  float rssi_variance;
  int frame_count;
  uint64_t timestamp = 0;
};

bool operator>(const wifi_deauth_event_t &a, const wifi_deauth_event_t &b) {
  return a.timestamp > b.timestamp;
}

atomic<bool> keep_running(true);

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

string bytes_to_mac(const uint8_t mac[6]) {
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
  con.Query("CREATE INDEX idx_timestamp ON events (timestamp)");
  duckdb::Appender appender(con, "events");

  map<uint64_t, vector<wifi_deauth_event_t>> qbuckets;
  const uint64_t quantum = 2000000;
  uint64_t current_qbucket = 0;

  int rows = 0;
  cout << "Reading events over UART..." << endl;
  while (keep_running) {
    wifi_deauth_event_t event;
    if (!readSerialExact(fd, &event, sizeof(event))) {
      continue;
    }

    event.timestamp = now_us();
    if (rows == 0) {
      current_qbucket = event.timestamp;
    }

    if ((event.timestamp - current_qbucket) > quantum) {
      cout << "*****************************" << endl;
      cout << "Current event timestamp: " << event.timestamp << endl;
      cout << "Current quantum bucket timestamp: " << current_qbucket << endl;
      cout << "Bucket size: " << qbuckets[current_qbucket].size() << endl;
      cout << "Creating new quantum bucket with quantum bucket timestamp: "
           << event.timestamp << endl;
      qbuckets[event.timestamp].push_back(event);

      cout << "Sorting quantum backet with quantum bucket timestamp: "
           << current_qbucket << endl;
      sort(qbuckets[current_qbucket].begin(), qbuckets[current_qbucket].end(),
           [](const wifi_deauth_event_t &a, const wifi_deauth_event_t &b) {
             return !(a > b); // sorts ascending using operator>
           });

      cout << "Batch inersting sorted quantum bucket with quantum bucket "
              "timestamp: "
           << current_qbucket << endl;
      for (int i = 0; i < qbuckets[current_qbucket].size(); i++) {
        wifi_deauth_event_t current_event = qbuckets[current_qbucket].at(i);
        appender.AppendRow(current_event.timestamp,
                           bytes_to_mac(current_event.attack_mac).c_str(),
                           bytes_to_mac(current_event.sensor_mac).c_str(),
                           current_event.rssi_mean, current_event.rssi_variance,
                           current_event.frame_count);
      }
      appender.Flush();
      qbuckets.erase(current_qbucket);
      current_qbucket = event.timestamp;
      cout << "***************************" << endl;
    } else {
      cout << "==============================" << endl;
      cout << "Appending event with timestamp: " << event.timestamp << endl;
      cout << "To quantum bucket: " << current_qbucket << endl;
      qbuckets[current_qbucket].push_back(event);
      cout << "==============================" << endl;
    }
    rows++;
  }

  close(fd);
  appender.Close();

  /* auto result = con.Query("SELECT * FROM events;");
  if (result->HasError()) {
    cerr << "SELECT query failed: " << result->GetError() << endl;
  } else {
    cout << result->ToString() << endl;
  } */
  return 0;
}
