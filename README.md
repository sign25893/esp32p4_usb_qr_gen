
# 📦 ESP32-P4 USB QR Code Generator

Этот проект демонстрирует, как использовать **ESP32-P4** для взаимодействия с внешней USB-флешкой и генерации **QR-кодов** в виде PNG-изображений прямо на неё.

## 🚀 Возможности

- 📎 Подключение и монтирование USB Mass Storage (MSC) устройств (флешек).
- 🧾 Автоматическое создание директории `/usb/qr` и генерация **1 - 50 PNG файлов** с уникальными QR-кодами (UUID v4).
- 🖨️ Отрисовка QR-кодов в виде PNG с помощью библиотеки `libpng`.
- 👂 Обработка событий подключения/отключения устройств и кнопки выхода.

---

## 🧰 Требования

- **ESP32-P4**
- [ESP-IDF v5.4+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html)
- USB OTG-поддержка
- Подключаемое USB устройство хранения данных (MSC)

---

## 🛠️ Сборка и загрузка

1. **Клонируйте репозиторий:**

    ```bash
    git clone https://github.com/yourusername/esp32p4-usb-qr-generator.git
    cd esp32p4-usb-qr-generator
    ```

2. **Выберите целевую платформу:**

    ```bash
    idf.py set-target esp32p4
    ```

3. **Соберите проект:**

    ```bash
    idf.py build
    ```

4. **Загрузите прошивку:**

    ```bash
    idf.py -p /dev/ttyUSB0 flash monitor
    ```

---

## ⚙️ Использование

1. Подключите USB-флешку к вашему ESP32-P4.
2. Подождите, пока устройство будет смонтировано.
3. Автоматически сгенерируются QR-коды и сохранятся в `/usb/qr/qr_N.png`.
4. Нажатие кнопки GPIO0 приведёт к завершению работы задачи и размонтированию флешки.

---

## 🧪 Пример кода

Функция, отвечающая за генерацию QR PNG:

```c
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
```

---

## 📂 Структура файлов на флешке

После генерации структура будет следующей:

```
/usb/
└── qr/
    ├── qr_0.png
    ├── qr_1.png
    ├── ...
    └── qr_49.png
```

---

## 📸 Пример изображения

Каждое изображение представляет QR-код, сгенерированный из UUIDv4 и отрисованный как PNG-изображение размером 116x116 пикселей (при `png_scale = 4`).

---

## 📋 Зависимости

- `esp_qrcode` — встроенная библиотека генерации QR-кодов
- `libpng` — генерация PNG-файлов
- `msc_host` — USB Mass Storage Host драйвер ESP-IDF

---

## 🔐 Лицензия

Этот проект распространяется под лицензией **MIT**. См. файл [LICENSE](LICENSE) для подробностей.

---

## 💬 Обратная связь

Открывайте issue или pull request, если хотите внести вклад или нашли баг.
