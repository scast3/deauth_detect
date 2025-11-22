#include <duckdb.hpp>
#include <iostream>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;

struct wifi_deauth_event_t{
  uint8_t attack_mac[6];
  uint8_t sensor_mac[6];
  int8_t rssi_mean;
  float rssi_variance;
  int frame_count;
};

int openSerialPort(const char* portname)
{
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        cerr << "Error opening " << portname << ": "
             << strerror(errno) << endl;
        return -1;
    }
    return fd;
}

bool configureSerialPort(int fd, int speed)
{
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        cerr << "Error from tcgetattr: " << strerror(errno)
             << endl;
        return false;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag
        = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit characters
    tty.c_iflag &= ~IGNBRK; // disable break processing
    tty.c_lflag = 0; // no signaling chars, no echo, no
                     // canonical processing
    tty.c_oflag = 0; // no remapping, no delays
    tty.c_cc[VMIN] = 0; // read doesn't block
    tty.c_cc[VTIME] = 5; // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF
                     | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag
        |= (CLOCAL | CREAD); // ignore modem controls,
                             // enable reading
    tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        cerr << "Error from tcsetattr: " << strerror(errno)
             << endl;
        return false;
    }
    return true;
}

bool readSerialExact(int fd, void *buf, size_t len) {
    size_t total = 0;
    char *p = (char *)buf;

    while (total < len) {
        int n = read(fd, p + total, len - total);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

void closeSerialPort(int fd) { close(fd); }

int main()
{
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

	const char* portname = "/dev/serial0";
	int fd = openSerialPort(portname);
	if (fd < 0)
		return 1;
	
	if(!configureSerialPort(fd, B921600)) {
		closeSerialPort(fd);
		cerr << "Error opening serial port" << endl;
		return 1;
	}
	
	while (true) {
		wifi_deauth_event_t event;
		if (!readSerialExact(fd, &event, sizeof(event))) {
        		continue;
    		}
		
		printf("Attack MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
		event.attack_mac[0], event.attack_mac[1], event.attack_mac[2],
		event.attack_mac[3], event.attack_mac[4], event.attack_mac[5]);

		printf("Sensor MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
		event.sensor_mac[0], event.sensor_mac[1], event.sensor_mac[2],
		event.sensor_mac[3], event.sensor_mac[4], event.sensor_mac[5]);

		printf("RSSI mean: %d\n", event.rssi_mean);
		printf("RSSI variance: %f\n", event.rssi_variance);
		printf("Frame count: %d\n", event.frame_count);
	}

	closeSerialPort(fd);
	return 0;
}
