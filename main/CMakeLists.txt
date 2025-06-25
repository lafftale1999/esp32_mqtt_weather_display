idf_component_register(SRCS "main.c" "src/esp32_wifi.c" "src/esp32_mqtt.c"
                    INCLUDE_DIRS "." "include"
                    PRIV_INCLUDE_DIRS "../credentials"
                    )

                    