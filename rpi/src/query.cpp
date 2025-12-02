#include <duckdb.hpp>
#include <iostream>
#include <map>
#include <vector>
#include <string>

using namespace std;

struct Event {
    uint64_t timestamp;
    string attack_mac;
    string sensor_mac;
    int rssi_mean;
    float rssi_variance;
    int frame_count;
};

int main(int argc, char *argv[]) {

    if (argc < 3) {
        cerr << "Usage: " << argv[0] 
             << " <center_timestamp_us> <window_us>\n";
        return 1;
    } // timestamp +/- window in microsecs

    uint64_t center_ts = stoull(argv[1]);
    uint64_t window = stoull(argv[2]);

    uint64_t ts_min = center_ts - window;
    uint64_t ts_max = center_ts + window;

    cout << "Window between " << ts_min 
         << " and " << ts_max << "\n";

    // duck db stuff
    duckdb::DuckDB db("events.db");
    duckdb::Connection con(db);

    string query =
        "SELECT timestamp, attack_mac, sensor_mac, rssi_mean, "
        "rssi_variance, frame_count "
        "FROM events "
        "WHERE timestamp >= " + to_string(ts_min) +
        " AND timestamp <= " + to_string(ts_max) +
        " ORDER BY attack_mac, timestamp;";

    auto result = con.Query(query);
    if (!result->success) {
        cerr << "Query failed: " << result->error << endl;
        return 1;
    }

    map<string, vector<Event>> grouped_events; //group by mac

    for (size_t i = 0; i < result->row_count(); i++) {
        Event ev;
        ev.timestamp      = result->GetValue<uint64_t>(0, i);
        ev.attack_mac     = result->GetValue<string>(1, i);
        ev.sensor_mac     = result->GetValue<string>(2, i);
        ev.rssi_mean      = result->GetValue<int32_t>(3, i);
        ev.rssi_variance  = result->GetValue<float>(4, i);
        ev.frame_count    = result->GetValue<int32_t>(5, i);

        grouped_events[ev.attack_mac].push_back(ev); // store by attack mac
    }

    for (auto &pair : grouped_events) {
        cout << "\nAttack MAC: " << pair.first 
             << " (" << pair.second.size() << " events) ===\n"; // printing mac and num events

        for (auto &ev : pair.second) {
            cout << "  ts=" << ev.timestamp
                 << "  sensor=" << ev.sensor_mac
                 << "  rssi=" << ev.rssi_mean
                 << "  frames=" << ev.frame_count
                 << "\n";
        }
    }

    return 0;
}
