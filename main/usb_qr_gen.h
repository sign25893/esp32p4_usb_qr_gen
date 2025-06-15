#pragma once

#include "esp_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#include <inttypes.h>
#include <setjmp.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "ffconf.h"
#include "errno.h"
#include "driver/gpio.h"
#include "png.h"
#include "qrcode.h"   // Используем API из qrcode.h
#include <unistd.h>
#include "esp_random.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Запускает задачу работы с USB QR-кодами
 */
void usb_qr_task(void *pvParameters);

/**
 * @brief Генерация QR-кодов в PNG и сохранение их на USB флешке
 *
 * @note Предполагается, что флешка уже смонтирована
 */
void usb_generate_qr_to_flash(void);


#ifdef __cplusplus
}
#endif
