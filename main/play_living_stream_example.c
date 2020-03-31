static const char *TAG = "SIP";

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "sdkconfig.h"

#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "board.h"
#include <stdio.h>

#include "periph_touch.h"
#include "periph_adc_button.h"
#include "periph_button.h"

#include "esp_system.h"
#include "driver/gpio.h"
#include "mcp23017.h"
#include "driver/i2c.h"
#include "rom/uart.h"

#include "smbus.h"
#include "i2c-lcd1602.h"

// Hardware
#include "./screen/lcd.c"
#include "i2c_RTE_Controller.h"
#include "i2c_RTE_Module.h"
// Menu items
#include "./radio/radio.c"
#include "./volume/volume.c"
#include "./clock/clock.c"
#include "./did/did.c"

#define F_CPU 20000000UL
#define NUMBER_OF_BUTTONS 5
#define string char *

void runMenuOptionFromButton(int buttonStates);
void doMenuAction(int button);
void listenmbutts(void *pvParameters);
void initWifi();

mcp23017_t mcp23017;
SemaphoreHandle_t xMutex;

int player_volume = 10;

//--------------------------------------------------------------------------------------------------------
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TX_BUF_LEN 0 // disabled
#define I2C_MASTER_RX_BUF_LEN 0 // disabled
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_SDA_IO CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_SCL_IO CONFIG_I2C_MASTER_SCL
#define LCD_NUM_ROWS 4
#define LCD_NUM_COLUMNS 40
#define LCD_NUM_VIS_COLUMNS 20

#define true 1
#define false 0
#define boolean int

typedef struct
{
    boolean isOverLay;
    boolean isActive;
    void (*constructor)();
    void (*destructor)();
    void (*onKnobLeft)();
    void (*onKnobRight)();
    char name[(4 * 20)];
} menuState;

void doMenuAction(int num);
void constructorDefault(menuState state);
void destructorDefault(menuState state);

void constructorVolume(menuState state);
void destructorVolume(menuState state);

void constructorTijd(menuState state);
void destructorTijd(menuState state);

void constructorSpellen(menuState state);
void destructorSpellen(menuState state);

void constructorDID(menuState state);
void destructorDID(menuState state);

void constructorRadio(menuState state);
void destructorRadio(menuState state);

void onKnobLeftDefault();
void onKnobRightDefault();

void printRadioScreen();
void startInternetRadio();

typedef struct
{
    menuState menuItems[5];
} menuStore;

menuState state;
// menuState overlayedState;

menuState idle = {
    false,
    false,
    &constructorDefault,
    &destructorDefault,
    &onKnobLeftDefault,
    &onKnobRightDefault,
    "idle"};

menuState idleoverlay = {
    true,
    false,
    &constructorDefault,
    &destructorDefault,
    &onKnobLeftDefault,
    &onKnobRightDefault,
    "idle"};

menuStore menu = {
    {
        {true,
         false,
         &constructorVolume,
         &destructorVolume,
         &onKnobLeftDefault,
         &onKnobRightDefault,
         "Volume"},
        {false,
         false,
         &constructorTijd,
         &destructorTijd,
         &onKnobLeftDefault,
         &onKnobRightDefault,
         "Tijd"},
        {false,
         false,
         &constructorSpellen,
         &destructorSpellen,
         &onKnobLeftDefault,
         &onKnobRightDefault,
         "Spellen"},
        {false,
         false,
         &constructorDID,
         &destructorDID,
         &onKnobLeftDefault,
         &onKnobRightDefault,
         "DID"},
        {false,
         false,
         &constructorRadio,
         &destructorRadio,
         &onKnobLeftDefault,
         &onKnobRightDefault,
         "Radio"},
    }};

// init LCD and SMBus
smbus_info_t *smbus_lcd;
smbus_info_t *smbus_rgb;
smbus_info_t *smbus_audio;
i2c_lcd1602_info_t *lcd_info;

void lcd1602_task(void *pvParameter)
{
    // Set up I2C
    i2c_port_t i2c_num = I2C_MASTER_NUM;
    uint8_t address = CONFIG_LCD1602_I2C_ADDRESS;

    // Set up the SMBus for lcd
    smbus_lcd = smbus_malloc();
    smbus_init(smbus_lcd, i2c_num, address);
    smbus_set_timeout(smbus_lcd, 1000 / portTICK_RATE_MS);

    // Set up the LCD1602 device with backlight off
    lcd_info = i2c_lcd1602_malloc();
    i2c_lcd1602_init(lcd_info, smbus_lcd, true, LCD_NUM_ROWS, LCD_NUM_COLUMNS, LCD_NUM_VIS_COLUMNS);

    i2c_lcd1602_set_backlight(lcd_info, true);
    writeLineFromStart("Werkt dit?");
    //vTaskDelete(NULL);
}

void mcp23017_task_read(void *pvParameters)
{
    int buttoncount = 0;
    while (1)
    {
        buttoncount++;
        if (buttoncount > 40)
        {
            buttoncount = 0;
            printf("Button thread is still alive\n");
        }
        vTaskDelay(25 / portTICK_RATE_MS);
        // Check states on input
        uint8_t states = 0;
        xSemaphoreTake(xMutex, portMAX_DELAY);

        mcp23017_read_register(&mcp23017, MCP23017_GPIO, GPIOA, &states);

        xSemaphoreGive(xMutex);

        // printf("states: %d\n", states);
        runMenuOptionFromButton(states);

        //ESP_LOGI(TAG, "GPIO register A states: %d", states);
    }
    vTaskDelete(NULL);
}

void runMenuOptionFromButton(int buttonStates)
{
    for (int i = 0; i <= 5; i++)
    {
        int state = buttonStates & (1 << i);
        if (state > 0)
        {
            doMenuAction(i);
        }
    }
}

void init_rgb()
{
    i2c_port_t i2c_num = I2C_MASTER_NUM;
    smbus_rgb = smbus_malloc();
    ESP_ERROR_CHECK(smbus_init(smbus_rgb, i2c_num, 0x40));
    ESP_ERROR_CHECK(smbus_set_timeout(smbus_rgb, 1000 / portTICK_RATE_MS));
    smbus_write_byte(smbus_rgb, 0x00, 0x00);
    smbus_write_byte(smbus_rgb, 0x01, 0b00001100);
}

esp_periph_set_handle_t set;

void init_sdcard()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
}

void app_main()
{
    // setup sd card
    init_sdcard();

    // start the internet connection
    initWifi();

    // Initialize I2C bus
    mcp23017.i2c_addr = 0x20;
    mcp23017.sda_pin = 18;
    mcp23017.scl_pin = 23;
    mcp23017.sda_pullup_en = GPIO_PULLUP_DISABLE;
    mcp23017.scl_pullup_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(mcp23017_init(&mcp23017));

    // protect the gpio registers
    xMutex = xSemaphoreCreateMutex();

    // Rotary encoder
    // hardware doesn't work
    // xTaskCreate(RTEChangeDetect_Task, "RTEChangeDetect_Task", 1024 * 2, NULL, 10, NULL);
    //RTEChangeDetect_Task(NULL);

    // Set GPIO Direction
    mcp23017_write_register(&mcp23017, MCP23017_IODIR, GPIOA, 0x00); // full port on INPUT
    mcp23017_write_register(&mcp23017, MCP23017_GPPU, GPIOA, 0x00);  // full port on INPUT

    // listen to gpio buttons
    xTaskCreate(mcp23017_task_read, "mcp23017_task_read", 1024 * 8, NULL, 24, NULL);
    // init lcd
    lcd_info = i2c_lcd1602_malloc();
    lcd1602_task(NULL);
    // put something in the state variable, otherwise the destructor
    // can't be called when switching to a new menu
    state = idle;
    state.constructor(state);

    doMenuAction(4);
}

void initWifi()
{
    ESP_LOGI(TAG, "[ 3 ] Start and wait for Wi-Fi network");

    tcpip_adapter_init();

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);

    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };

    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);

    periph_wifi_wait_for_connected(wifi_handle, 5000 / portTICK_RATE_MS);

    esp_periph_set_destroy(set);
}

void doMenuAction(int num)
{
    printf("destroying %s\n", state.name);
    state.destructor();
    printf("switching to %s\n", menu.menuItems[num].name);
    state = menu.menuItems[num];
    printf("constructing %s\n", state.name);
    state.constructor();
}

void constructorDefault(menuState state)
{
    printf("constructor %s ran\n", state.name);
}

void constructorVolume(menuState state)
{
    // display_volume((player_volume / 5), lcd_info);
    volume_constructor();
}

void destructorVolume(menuState state)
{
    volume_destructor();
}

void onKnobLeftDefault()
{
    // do nothing
}

void constructorSpellen(menuState state)
{
    printf("hij komt voor het Spellen menu\n");
    if (state.isActive == true)
    {
        printf("hij komt in het Spellen menu\n");
        clearScreen();
        moveCursor(0, 0);
        writeLineOnPosition(0, 0, "Spellen");
        writeLineOnPosition(0, 1, "====================");
        writeLineOnPosition(0, 2, "> 8Ball");
        writeLineOnPosition(0, 3, "  [END]");
    }
}

void destructorSpellen(menuState state)
{
    //delete
}

void destructorDID(menuState state)
{
    kill_did();
}

void constructorRadio(menuState state)
{
    printf("hij komt voor het Radio menu\n");
    radio_constructor();
}

void destructorRadio(menuState state)
{
    radio_destructor();
}

void destructorDefault(menuState state)
{
    printf("destructor %s ran\n", state.name);
}

void onKnobRightDefault()
{
    printf("turning right\n");
}

void constructorDID(menuState state)
{
    init_did();
}

void constructorTijd(menuState state)
{
    init_clock();
}

void destructorTijd(menuState state)
{
    destroy_clock();
}
