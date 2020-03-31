static const char *TAG = "Individueel_Main";

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

#define F_CPU 20000000UL
#define NUMBER_OF_BUTTONS 5
#define string char *

void initWifi();
void init_sdcard();
void loading_screen();

mcp23017_t mcp23017;
SemaphoreHandle_t xMutex;

esp_periph_set_handle_t set;

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

boolean loading = false;

// init LCD and SMBus
smbus_info_t *smbus_lcd;
smbus_info_t *smbus_rgb;
smbus_info_t *smbus_audio;
i2c_lcd1602_info_t *lcd_info;

// Initialize for the LCD display. Essential for the program to run.

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

    loading = true;    
    
    //vTaskDelete(NULL);
}

// Task that reads the input of the connected buttons on breadboard. 

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
        //runMenuOptionFromButton(states);

        //ESP_LOGI(TAG, "GPIO register A states: %d", states);
    }
    vTaskDelete(NULL);
}

// The main of the program

void app_main()
{
    // init SD card
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

    // Set GPIO Direction
    mcp23017_write_register(&mcp23017, MCP23017_IODIR, GPIOA, 0x00); // full port on INPUT
    mcp23017_write_register(&mcp23017, MCP23017_GPPU, GPIOA, 0x00);  // full port on INPUT

    // listen to gpio buttons
    xTaskCreate(mcp23017_task_read, "mcp23017_task_read", 1024 * 8, NULL, 24, NULL);
    // init lcd
    lcd_info = i2c_lcd1602_malloc();
    lcd1602_task(NULL);

    // setup loading task
    xTaskCreate(loading_screen, "loading_screen", 1024 * 8, NULL, 24, NULL);

    vTaskDelay(5000 / portTICK_RATE_MS);

    loading = false;

}

// Initializes Wifi with given parameters in the config. Essential for the program to run.

void initWifi()
{
    ESP_LOGI(TAG, "[ 2 ] Start and wait for Wi-Fi network");

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

// Initializes SD card. Essential for the program to run.

void init_sdcard()
{
    ESP_LOGI(TAG, "[ 1 ] Initializes SD card");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
}

// Displays the loading screen as long as the value loading == true.
// Deletes itself when the value of loading is set to false.

void loading_screen() {
    loading = true;
    while (loading == true)
    {
        clearScreen();
        for (size_t i = 0; i < 3; i++)
        {
            writeLineFromStart("Loading");
            writeLineOnPosition(8 + i, 0, ".");
            vTaskDelay(500 / portTICK_RATE_MS);
        }
    }
    vTaskDelete(NULL);
}

