set(app_src i2c_lcd1602.c)
set(pri_req smbus)
idf_component_register(SRCS  ${app_src}
                    INCLUDE_DIRS "."
                    REQUIRES ${pri_req})

 