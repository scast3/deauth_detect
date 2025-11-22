#ifndef ESP_TO_UART_H
#define ESP_TO_UART_H

#include <cstddef>

int openSerialPort(const char *portname);
bool configureSerialPort(int fd, int speed);
bool readSerialExact(int fd, void *buf, size_t len);

#endif // ESP_TO_UART_H
