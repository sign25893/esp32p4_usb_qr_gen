#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_qr_gen.h"

extern void usb_qr_task(void *args);

void app_main(void) {
    xTaskCreate(usb_qr_task, "usb_qr_task", 8192, NULL, 5, NULL);
}