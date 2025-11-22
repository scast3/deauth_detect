#include "../include/esp32_to_uart.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>
using namespace std;

int openSerialPort(const char *portname) {
  int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
    cerr << "Error opening " << portname << ": " << strerror(errno) << endl;
    return -1;
  }
  return fd;
}

bool configureSerialPort(int fd, int speed) {
  struct termios tty;
  if (tcgetattr(fd, &tty) != 0) {
    cerr << "Error from tcgetattr: " << strerror(errno) << endl;
    return false;
  }

  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit characters
  tty.c_iflag &= ~IGNBRK;                     // disable break processing
  tty.c_lflag = 0;                            // no signaling chars, no echo, no
                                              // canonical processing
  tty.c_oflag = 0;                            // no remapping, no delays
  tty.c_cc[VMIN] = 0;                         // read doesn't block
  tty.c_cc[VTIME] = 5;                        // 0.5 seconds read timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

  tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
                                     // enable reading
  tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    cerr << "Error from tcsetattr: " << strerror(errno) << endl;
    return false;
  }
  return true;
}

bool readSerialExact(int fd, void *buf, size_t len) {
  size_t total = 0;
  char *p = (char *)buf;

  while (total < len) {
    int n = read(fd, p + total, len - total);
    if (n <= 0)
      return false;
    total += n;
  }
  return true;
}
