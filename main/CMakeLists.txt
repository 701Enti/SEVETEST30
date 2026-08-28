set(COMPONENT_ADD_INCLUDEDIRS ./include)
set(COMPONENT_PRIV_REQUIRES
audio_hal 
driver 
zlib 
esp_peripherals 
json 
esp_http_client 
esp-dsp 
esp-sr 
audio_stream 
matrix_f64
monocypher
)
file(GLOB ALL_C_SRCS "MAX17048.c" "OPT3001.c" "math_tools.c" "GT32L32S0140.c" "calibration_tools.c" "sevetest30_gpio.c" "sevetest30_BWEDA.c" "sevetest30_SWEDA.c" "math_tools_fp64.c" "sevetest30_touch.c" "sevetest30_sound.c" "sevetest30_IWEDA.c" "main.c" "sevetest30_UI.c" "default_UI_icon_data.c" "sevetest30_LedArray.c" "TCA6416A.c" "LSM6DS3TRC.c" "board_ctrl.c" "AHT20.c" "AGS10.c" "HSCDTD008A.c" "*.c")
set(COMPONENT_SRCS ${ALL_C_SRCS})
set(LDFRAGMENTS "./sevetest30.lf")
register_component()