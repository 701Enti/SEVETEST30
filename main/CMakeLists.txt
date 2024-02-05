set(COMPONENT_SRCS
./main.c
./sevetest30_BWEDA.c
./sevetest30_IWEDA.c
./TCA6416A.c
./sevetest30_LedArray.c
./sevetest30_gpio.c
./sevetest30_UI.c
./sevetest30_SWEDA.c
./board_ctrl.c
./fonts_chip.c
./sevetest30_touch.c
./sevetest30_sound.c
)
set(LDFRAGMENTS "./sevetest30.lf")
register_component()