idf_component_register(SRCS owb.c owb_rmt.c ds18b20.c
                       REQUIRES espstring log
                       INCLUDE_DIRS "."
                       )

