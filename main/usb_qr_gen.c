#include "usb_qr_gen.h"

static const char *TAG = "example";
#define MNT_PATH         "/usb"
#define APP_QUIT_PIN     GPIO_NUM_0
#define BUFFER_SIZE      4096

bool usb_mount = false;      // флешка подключена
bool qr_generated = false;     // QR коды созданы

extern void usb_qr_task(void *args);

static QueueHandle_t app_queue;
typedef struct {
    enum {
        APP_QUIT,
        APP_DEVICE_CONNECTED,
        APP_DEVICE_DISCONNECTED,
    } id;
    union {
        uint8_t new_dev_address;
    } data;
} app_message_t;

static void gpio_cb(void *arg)
{
    BaseType_t xTaskWoken = pdFALSE;
    app_message_t message = {.id = APP_QUIT};
    if (app_queue) {
        xQueueSendFromISR(app_queue, &message, &xTaskWoken);
    }
    if (xTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    app_message_t message;
    if (event->event == MSC_DEVICE_CONNECTED) {
        ESP_LOGI(TAG, "MSC device connected (usb_addr=%d)", event->device.address);
        message.id = APP_DEVICE_CONNECTED;
        message.data.new_dev_address = event->device.address;
    } else {
        ESP_LOGI(TAG, "MSC device disconnected");
        message.id = APP_DEVICE_DISCONNECTED;
    }
    xQueueSend(app_queue, &message, portMAX_DELAY);
}

static void print_device_info(msc_host_device_info_t *info)
{
    const size_t megabyte = 1024 * 1024;
    uint64_t capacity = ((uint64_t)info->sector_size * info->sector_count) / megabyte;
    printf("Device info:\n");
    printf("\t Capacity: %llu MB\n", capacity);
    printf("\t Sector size: %"PRIu32"\n", info->sector_size);
    printf("\t Sector count: %"PRIu32"\n", info->sector_count);
    printf("\t PID: 0x%04X \n", info->idProduct);
    printf("\t VID: 0x%04X \n", info->idVendor);
#ifndef CONFIG_NEWLIB_NANO_FORMAT
    wprintf(L"\t iProduct: %S \n", info->iProduct);
    wprintf(L"\t iManufacturer: %S \n", info->iManufacturer);
    wprintf(L"\t iSerialNumber: %S \n", info->iSerialNumber);
#endif
}

static FILE *png_fp = NULL;
static png_structp png_ptr = NULL;
static png_infop info_ptr = NULL;
static int png_img_size = 0;
static int png_scale = 4;

// Функция для записи одной строки PNG с QR кодом
static void png_write_qr_row(esp_qrcode_handle_t qrcode, int y)
{
    png_bytep row = malloc(3 * png_img_size);
    if (!row) {
        ESP_LOGE(TAG, "Failed to allocate memory for PNG row");
        return;
    }
    for (int sy = 0; sy < png_scale; sy++) {
        for (int x = 0; x < png_img_size / png_scale; x++) {
            bool module = esp_qrcode_get_module(qrcode, x, y);
            uint8_t pixel = module ? 0 : 255;
            for (int sx = 0; sx < png_scale; sx++) {
                int px = x * png_scale + sx;
                row[px * 3 + 0] = pixel;
                row[px * 3 + 1] = pixel;
                row[px * 3 + 2] = pixel;
            }
        }
        png_write_row(png_ptr, row);
    }
    free(row);
}

// Функция отображения QR кода для esp_qrcode_generate (вызывается при генерации)
static void display_func(esp_qrcode_handle_t qrcode)
{
    int size = esp_qrcode_get_size(qrcode);
    png_img_size = size * png_scale;

    // Запускаем создание PNG
    png_set_IHDR(png_ptr, info_ptr, png_img_size, png_img_size, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < size; y++) {
        png_write_qr_row(qrcode, y);
    }

    png_write_end(png_ptr, NULL);
}

void generate_uuid_v4(char *buffer)
{
    uint8_t uuid[16];
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = esp_random();
        uuid[i] = (r >> 0) & 0xFF;
        uuid[i+1] = (r >> 8) & 0xFF;
        uuid[i+2] = (r >> 16) & 0xFF;
        uuid[i+3] = (r >> 24) & 0xFF;
    }

    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    for (int i = 0; i < 16; i++) {
        sprintf(buffer + i*2, "%02x", uuid[i]);
    }
    buffer[32] = '\0';
}

// Генерация PNG файла с QR-кодом используя esp_qrcode_generate
static void generate_qr_png(const char *file_path, const char *text)
{
    png_fp = fopen(file_path, "wb");
    if (!png_fp) {
        ESP_LOGE(TAG, "Failed to open PNG file for writing");
        return;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(png_fp);
        ESP_LOGE(TAG, "Failed to create PNG write struct");
        return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr || setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(png_fp);
        ESP_LOGE(TAG, "Error setting up PNG write");
        return;
    }

    png_init_io(png_ptr, png_fp);

    // Настройка конфигурации QR-кода
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    cfg.max_qrcode_version = 4;  // Версия QR кода 4 (29x29 модулей с поддержкой восстановления ошибок)
    cfg.display_func = display_func;
    cfg.qrcode_ecc_level = ESP_QRCODE_ECC_HIGH;

    esp_err_t err = esp_qrcode_generate(&cfg, text);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate QR code: %d", err);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(png_fp);
        return;
    }

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(png_fp);
    ESP_LOGI(TAG, "QR PNG image saved to %s", file_path);
}

static void file_operations(void)
{
    const char *directory = "/usb/qr";
    const int qr_count = 50;

    // Удалим папку /usb/qr если она уже существует
    DIR *dir = opendir(directory);
    if (dir) {
        struct dirent *entry;
        char filepath[512];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);
            remove(filepath);
        }
        closedir(dir);
        remove(directory);
    }

    // Создадим папку заново
    mkdir(directory, 0775);

    // Генерация нескольких QR-кодов с UUID
    char uuid_str[33];
    for (int i = 0; i < qr_count; i++) {
        char filepath[128];

        generate_uuid_v4(uuid_str);  // Генерируем новый UUID

        snprintf(filepath, sizeof(filepath), "/usb/qr/qr_%d.png", i);

        generate_qr_png(filepath, uuid_str);
    }
}

void usb_task(void *args)
{
    usb_host_install(&(usb_host_config_t){.intr_flags = ESP_INTR_FLAG_LEVEL1});
    msc_host_install(&(msc_host_driver_config_t){
        .create_backround_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .callback = msc_event_cb
    });

    while (true) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS && usb_host_device_free_all() == ESP_OK) break;
    }

    usb_host_uninstall();
    vTaskDelete(NULL);
}

void usb_qr_task(void *pvParameters)
{
    app_queue = xQueueCreate(5, sizeof(app_message_t));
    xTaskCreate(usb_task, "usb_task", 4096, NULL, 2, NULL);


    gpio_config(&(gpio_config_t){
        .pin_bit_mask = BIT64(APP_QUIT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    });
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(APP_QUIT_PIN, gpio_cb, NULL);

    msc_host_device_handle_t msc_device = NULL;
    msc_host_vfs_handle_t vfs_handle = NULL;

    while (1) {
        app_message_t msg;
        xQueueReceive(app_queue, &msg, portMAX_DELAY);

        if (msg.id == APP_DEVICE_CONNECTED && !usb_mount) {
            usb_mount = true;
            qr_generated = false; // сброс флага генерации
            msc_host_install_device(msg.data.new_dev_address, &msc_device);

            esp_vfs_fat_mount_config_t cfg = {
                .format_if_mount_failed = false,
                .max_files = 3,
                .allocation_unit_size = 8192
            };

            if (msc_host_vfs_register(msc_device, MNT_PATH, &cfg, &vfs_handle) != ESP_OK) {
                ESP_LOGE(TAG, "VFS mount failed");
            } else {
                ESP_LOGI(TAG, "USB device mounted");

/////////////////////////////////////////////////////////////////////////////////////////////
                if (usb_mount && !qr_generated) {
                    ESP_LOGI(TAG, "Generating QR codes on USB flash");
                    file_operations();
                    qr_generated = true;
                } else if (!usb_mount) {
                    ESP_LOGW(TAG, "USB device not present, skipping QR generation");
                } else if (qr_generated) {
                    ESP_LOGI(TAG, "QR codes already generated on USB flash");
                }
/////////////////////////////////////////////////////////////////////////////////////////////

            }
        }

        if (msg.id == APP_DEVICE_DISCONNECTED || msg.id == APP_QUIT) {
            if (usb_mount) {
                usb_mount = false;
                qr_generated = false;  // сброс флага
                if (vfs_handle) msc_host_vfs_unregister(vfs_handle);
                if (msc_device) msc_host_uninstall_device(msc_device);
                ESP_LOGI(TAG, "USB device unmounted");
            }
            if (msg.id == APP_QUIT) {
                msc_host_uninstall();
                break;
            }
        }
    }

    gpio_isr_handler_remove(APP_QUIT_PIN);
    vQueueDelete(app_queue);
    vTaskDelete(NULL);
}

void usb_generate_qr_to_flash(void) {
    if (usb_mount && !qr_generated) {
        ESP_LOGI(TAG, "Generating QR codes on USB flash");
        file_operations();
        qr_generated = true;
    } else if (!usb_mount) {
        ESP_LOGW(TAG, "USB device not present, skipping QR generation");
    } else if (qr_generated) {
        ESP_LOGI(TAG, "QR codes already generated on USB flash");
    }
}