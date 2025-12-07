#include "../include/esp32_to_uart.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <duckdb.hpp>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <termios.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <vector>

using std::string;
using namespace std;

static double sensor_x1 = 0, sensor_y1 = 0;
static double sensor_x2 = 2, sensor_y2 = 0;
static double sensor_x3 = 0, sensor_y3 = 3;
map<string, pair<double, double>> sensor_positions = {
    {"00:4B:12:3C:04:B0",
     {sensor_x1, sensor_y1}}, // put actual mac addresses ine here
    {"78:1C:3C:E3:AB:CC", {sensor_x2, sensor_y2}},
    {"78:1C:3C:2D:15:D4", {sensor_x3, sensor_y3}}};

// we might need to change from raw rssi to some regression funciton to get
// distance, let's test this out x and y values should be fixed, only thing
// changing is r
std::tuple<double, double> trilaterate(double x1, double y1, double r1,
                                       double x2, double y2, double r2,
                                       double x3, double y3, double r3) {
  double A = 2 * (x2 - x1);
  double B = 2 * (y2 - y1);
  double C = r1 * r1 - r2 * r2 - x1 * x1 + x2 * x2 - y1 * y1 + y2 * y2;

  double D = 2 * (x3 - x1);
  double E = 2 * (y3 - y1);
  double F = r1 * r1 - r3 * r3 - x1 * x1 + x3 * x3 - y1 * y1 + y3 * y3;

  double denominator = A * E - B * D;
  if (fabs(denominator) < 1e-9)
    return {NAN, NAN};

  double x = (C * E - B * F) / denominator;
  double y = (A * F - C * D) / denominator;
  return {x, y};
}
// i am realizing we may beed a custom rssi to distance function calibrated for
// each reciever
double rssi_to_distance(int rssi) {
  double RSSI0 = -40; // RSSI at 1 meter
  double n = 2.0;     // path-loss exponent, this should stay the same

  double exponent = (RSSI0 - rssi) / (10 * n);
  return pow(10.0, exponent);
}

struct __attribute__((packed)) wifi_deauth_event_t {
  uint8_t attack_mac[6];
  uint8_t sensor_mac[6];
  int8_t rssi_mean;
  float rssi_variance;
  int frame_count;
  uint64_t timestamp;
};

bool operator>(const wifi_deauth_event_t &a, const wifi_deauth_event_t &b) {
  return a.timestamp > b.timestamp;
}
// Multithreading variables
static queue<wifi_deauth_event_t> event_queue; // Shared
static mutex event_queue_mutex;
static condition_variable event_queue_cv;

// Time quantum for in-flight re-ordering
// Tune
static const int64_t quantum = 2000000; // 2s

// Signal handling stuff for graceful shutdown
// Allows Ctrl+C graceful shutdown
atomic<bool> keep_running(true);
void signal_handler(int signal) {
  if (signal == SIGINT) {
    cerr << "[SIGNAL] Caught SIGINT, shutting down..." << endl;
    keep_running = false;
  }
}

// Helper function
int64_t now_us() {
  using namespace std::chrono;
  return duration_cast<microseconds>(system_clock::now().time_since_epoch())
      .count();
}

// Helper function
string bytes_to_mac(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  return std::string(buf);
}

// Read events from UART and place in shared queue
void read_events(int fd) {
  cerr << "[THREAD] read_events started" << endl;

  while (keep_running) {
    wifi_deauth_event_t event;
    if (!readSerialExact(fd, &event, sizeof(event))) {
      continue;
    }

    // event.timestamp = now_us();

    /*cerr << "[read_events] Event received: attack="
         << bytes_to_mac(event.attack_mac)
         << " sensor=" << bytes_to_mac(event.sensor_mac)
         << " rssi_mean=" << (int)event.rssi_mean
         << " frame_count=" << event.frame_count << " ts=" << event.timestamp
         << endl;
*/
    // CRITICAL SECTION
    {
      lock_guard<mutex> lock(event_queue_mutex);
      event_queue.push(event);
    }
    /*cerr << "[read_events] Event pushed to queue (size=" << event_queue.size()
         << ")" << endl;
*/
    event_queue_cv.notify_one();
  }

  cerr << "[THREAD] read_events exiting" << endl;
}

// read events from shared queue and insert events into DB using appender
// in-flight re-ordering
void insert_events(duckdb::Appender *appender) {
  cerr << "[THREAD] insert_events started" << endl;
  map<uint64_t, vector<wifi_deauth_event_t>> qbuckets;
  uint64_t current_qbucket = 0;
  int rows = 0;

  while (keep_running || !event_queue.empty()) {
    wifi_deauth_event_t event;

    // CRITICAL SECTION
    {
      unique_lock<mutex> lock(event_queue_mutex);
      event_queue_cv.wait(lock,
                          [] { return !keep_running || !event_queue.empty(); });

      if (event_queue.empty()) {
        cerr << "[insert_events] Woke up but queue empty" << endl;
        continue;
      }

      event = event_queue.front();
      event_queue.pop();
      // cerr << "[insert_events] Popped event from queue" << endl;
    }

    // Set the first quantum bucket index as the first timestamp
    if (rows == 0) {
      current_qbucket = event.timestamp;
      /*cerr << "[insert_events] Starting first qbucket at " << current_qbucket
           << endl; */
    }

    // If incoming timestamp exceeds (current index + quantum)
    if ((event.timestamp - current_qbucket) > quantum) {
      /*cerr << "[insert_events] Closing qbucket " << current_qbucket << " with
         "
           << qbuckets[current_qbucket].size() << " events" << endl;
*/
      // Sort completed bucket (the OLD bucket)
      sort(qbuckets[current_qbucket].begin(), qbuckets[current_qbucket].end(),
           [](const wifi_deauth_event_t &a, const wifi_deauth_event_t &b) {
             return !(a > b);
           });

      // Append sorted bucket to DB
      for (const auto &current_event : qbuckets[current_qbucket]) {
        /*       cerr << "[insert_events] Appending event ts=" <<
           current_event.timestamp
                    << " attack=" << bytes_to_mac(current_event.attack_mac)
                    << " sensor=" << bytes_to_mac(current_event.sensor_mac)
                    << " rssi=" << (int)current_event.rssi_mean
                    << " frames=" << current_event.frame_count << endl;
       */
        appender->AppendRow(current_event.timestamp,
                            bytes_to_mac(current_event.attack_mac).c_str(),
                            bytes_to_mac(current_event.sensor_mac).c_str(),
                            current_event.rssi_mean,
                            current_event.rssi_variance,
                            current_event.frame_count);
      }

      //     cerr << "[insert_events] Flushing appender..." << endl;
      appender->Flush();

      // Clean up old bucket and move to new one
      qbuckets.erase(current_qbucket);
      current_qbucket = event.timestamp; // ← Now update to new bucket
      //    cerr << "[insert_events] New qbucket=" << current_qbucket << endl;
    }

    // Add incoming event to CURRENT bucket (whether new or existing)
    qbuckets[current_qbucket].push_back(event);
    /* cerr << "[insert_events] Added event to current bucket ("
          << qbuckets[current_qbucket].size() << " total)" << endl; */

    rows++;
  }

  cerr << "[THREAD] insert_events exiting" << endl;
}

int main() {
  signal(SIGINT, signal_handler);

  // UART/Serial stuff
  const char *portname = "/dev/serial0";
  int fd = openSerialPort(portname);
  if (fd < 0) {
    cerr << "[main] Failed to open serial port" << endl;
    return 1;
  }
  cerr << "[main] Serial port opened" << endl;

  if (!configureSerialPort(fd, B921600)) {
    close(fd);
    cerr << "[main] Error configuring serial port" << endl;
    return 1;
  }
  cerr << "[main] Serial port configured" << endl;

  // Configure DuckDB
  duckdb::DuckDB db(nullptr);
  duckdb::Connection con(db);
  con.Query("CREATE TABLE IF NOT EXISTS events (timestamp BIGINT, attack_mac "
            "VARCHAR(17), sensor_mac VARCHAR(17), rssi_mean INT, rssi_variance "
            "FLOAT, frame_count INT)");
  con.Query("CREATE INDEX idx_timestamp ON events (timestamp)");
  cerr << "[main] DuckDB initialized and tables ready" << endl;

  duckdb::Appender appender(con, "events");

  // Start producer and consumer threads
  thread producer(read_events, fd);
  thread consumer(insert_events, &appender);

  cerr << "[main] Threads started" << endl;

  // Keep main thread running
  while (keep_running) {
    // SQL QUERIES FOR ANALYSIS HERE!
    // con.Query("SELECT .....");

    // most recent timestemp
    // could just choose top index, if that makes it any faster, n
    auto latest_ts_result = con.Query("SELECT MAX(timestamp) FROM events;");
    if (latest_ts_result->HasError() ||
        latest_ts_result->GetValue(0, 0).IsNull()) {
      this_thread::sleep_for(chrono::milliseconds(200));
      continue;
    }

    uint64_t ts_max = latest_ts_result->GetValue<uint64_t>(0, 0);
    uint64_t window_us = 2000; // for a 2ms window
    uint64_t ts_min = ts_max - window_us;

    // add main query here
    string query = "SELECT DISTINCT ON (sensor_mac) "
                   "    sensor_mac, "
                   "    rssi_mean, "
                   "    rssi_variance, "
                   "    frame_count, "
                   "    timestamp "
                   "FROM events "
                   "ORDER BY sensor_mac, timestamp DESC;";

    auto result = con.Query(query); // check if fails
    if (result->HasError()) {
      cerr << "[main] Query failed: " << result->GetError() << endl;
      this_thread::sleep_for(chrono::milliseconds(200));
      continue;
    }
    // cout << "  [debug] result struct: " << result.ToString() << "\n";

    // row count of this result should ideally be 3, if not, then we prob need
    // to expand window to hit all 3 sensors

    struct SensorReading { // calculating the averages on the window
      string sensor_mac;
      float avg_rssi;
      float avg_variance;
      int frame_count; // maybe rename
    };
    vector<SensorReading> readings;
    readings.reserve(result->RowCount());

    // iterate through the 3 sensors and populate the readings vec
    for (size_t i = 0; i < result->RowCount(); ++i) {
      SensorReading sr;
      // column order from the SQL:
      // 0 = sensor_mac, 1 = avg_rssi, 2 = avg_variance, 3 = frame_count
      sr.sensor_mac = result->GetValue(0, i).ToString();
      sr.avg_rssi = static_cast<float>(result->GetValue<double>(1, i));
      sr.avg_variance = static_cast<float>(result->GetValue<double>(2, i));
      sr.frame_count = result->GetValue<int32_t>(3, i);
      readings.push_back(std::move(sr));
    }

    cout << "\nWindow " << ts_min << " to " << ts_max << " → "
         << readings.size() << " sensors\n";

    for (auto &r : readings) {
      cout << "  Sensor: " << r.sensor_mac << "  coords=("
           << sensor_positions[r.sensor_mac].first;
      << ", " << sensor_positions[r.sensor_mac].second;
      << ")"
      << "  avg_rssi=" << r.avg_rssi
      << "  calculated dist=" << rssi_to_distance(r.avg_rssi)
      << "  var=" << r.avg_variance << "  frames=" << r.frame_count << "\n";
    } // debug prints

    // distance and triangulation math!!!
    vector<double> distances;
    vector<pair<double, double>> coords; // to store triang resulkts

    for (auto &r : readings) { // iterate through the 3 sensors
      if (sensor_positions.find(r.sensor_mac) ==
          sensor_positions.end()) { // no corresponding mac
        cerr << "[WARN] Unknown sensor MAC: " << r.sensor_mac << "\n";
        continue;
      }

      auto [sx, sy] = sensor_positions[r.sensor_mac];

      // convert rssi to distance
      double dist = rssi_to_distance(r.avg_rssi);

      distances.push_back(dist);
      coords.push_back({sx, sy});
    }
    if (distances.size() ==
        3) { // need to check theres enough sensors to trilaterate
      double r1 = distances[0];
      double r2 = distances[1];
      double r3 = distances[2];

      double sx1 = coords[0].first, sy1 = coords[0].second;
      double sx2 = coords[1].first, sy2 = coords[1].second;
      double sx3 = coords[2].first, sy3 = coords[2].second;

      auto [x, y] = trilaterate(sx1, sy1, r1, sx2, sy2, r2, sx3, sy3, r3);

      if (std::isnan(x) || std::isnan(y)) {
        cout << "[TRILATERATION] Failed — geometry degenerate\n";
      } else {
        cout << "[TRILATERATION] Position estimate: (" << x << ", " << y
             << ")\n";
      }
    } else {
      cout << "[TRILATERATION] Not enough sensors: " << distances.size()
           << " available\n";
    }

    this_thread::sleep_for(chrono::milliseconds(200));
  }

  // Shutdown
  cerr << "[main] Shutdown requested, joining threads..." << endl;

  close(fd);
  event_queue_cv.notify_all();
  producer.join();
  consumer.join();
  appender.Close();

  cerr << "[main] Clean exit" << endl;
  return 0;
}
