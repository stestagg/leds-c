set(srcs "main.c"
         "gatt_svr.c")

idf_component_register(SRCS "led.c" "led_driver.c" "vm.c" "${srcs}"
                       INCLUDE_DIRS ".")
