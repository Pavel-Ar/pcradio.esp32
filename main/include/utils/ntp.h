#ifndef NTP_H
#define NTP_H

#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ntp_init(void);
void ntp_get_time_string(char *buf, size_t buf_len);

#endif
