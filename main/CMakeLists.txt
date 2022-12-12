set(srcs "main.c"
    "ili9340.c"
    "decode_png.c"
    "pngle.c"
    "pngSlide.c"
    "fontx.c"
    )


idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ".")



# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'. 
spiffs_create_partition_image(storage ../spiffs_files FLASH_IN_PROJECT)





# # tjpgd library does not exist in ESP32-S2 ROM.
# if(IDF_TARGET STREQUAL "esp32")
#     list(APPEND srcs "decode_jpeg.c")
# endif()
# 
# if(IDF_TARGET STREQUAL "esp32c3")
#     list(APPEND srcs "decode_jpeg.c")
# endif()


