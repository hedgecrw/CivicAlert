file(GLOB_RECURSE SOURCES "*.c")

set(REQUIRED_COMPONENTS
      esp_event
      esp_driver_i2s
      esp_driver_gpio
      esp_driver_uart
      esp_timer
      esp_wifi
      nvs_flash
      wifi_provisioning
)

idf_component_register(SRCS ${SOURCES}
                       PRIV_REQUIRES ${REQUIRED_COMPONENTS}
                       INCLUDE_DIRS "." "peripherals")
