idf_component_register(SRCS "main.c" "usb_qr_gen.c"
                    INCLUDE_DIRS ""
                    PRIV_REQUIRES usb fatfs esp_driver_gpio esp_timer qrcode libpng esp_system 
                    )
