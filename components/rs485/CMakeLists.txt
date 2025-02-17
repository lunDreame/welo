idf_component_register(
    SRCS "rs485.c" "rs485_parser.c"
    INCLUDE_DIRS "."
    REQUIRES driver
)