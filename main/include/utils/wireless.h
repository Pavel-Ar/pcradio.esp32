#ifndef WIRELESS_H
#define WIRELESS_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h" // Необходимо для EventGroupHandle_t

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries (этот бит больше не используется в текущей логике, но определение можно оставить)
 */
#define WIFI_CONNECTED_BIT BIT0
// #define WIFI_FAIL_BIT      BIT1 // Этот бит больше не используется в wireless.c

void wifi_init_sta(void);
EventGroupHandle_t get_wifi_event_group(void); // Новая функция

#endif // WIRELESS_H
