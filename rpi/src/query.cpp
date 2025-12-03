#include <duckdb.hpp>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <tuple>
#include <cmath>

using namespace std;

struct Event {
    uint64_t timestamp;
    string attack_mac;
    string sensor_mac;
    int rssi_mean;
    float rssi_variance;
    int frame_count;
};
// we might need to change from raw rssi to some regression funciton to get distance, let's test this out
// x and y values should be fixed, only thing changing is r
std::tuple<double, double> trilaterate(double x1, double y1, double r1, double x2, double y2, double r2, double x3, double y3, double r3){
    double A = 2*(x2 - x1);
    double B = 2*(y2 - y1);
    double C = r1*r1 - r2*r2 - x1*x1 + x2*x2 - y1*y1 + y2*y2;

    double D = 2*(x3 - x1);
    double E = 2*(y3 - y1);
    double F = r1*r1 - r3*r3 - x1*x1 + x3*x3 - y1*y1 + y3*y3;

    double denominator = A*E - B*D;
    if (fabs(denominator) < 1e-9) return {NAN, NAN};

    double x = (C*E - B*F) / denominator;
    double y = (A*F - C*D) / denominator;
    return {x, y};
}

double rssi_to_distance(int rssi) {
    // Example quadratic model: d = a*rssi^2 + b*rssi + c
    // double a = -0.0025;
    // double b = 0.35;
    // double c = -5.0;
    return rssi;
    // regression, will need to look at my notes from senior design
}

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
