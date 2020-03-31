#import <string.h>
#import <stdio.h>
#import "freertos/FreeRTOS.h"
#import "freertos/task.h"

#import "esp_log.h"
#import "audio_element.h"
#import "audio_pipeline.h"
#import "audio_event_iface.h"
#import "audio_mem.h"
#import "audio_common.h"
#import "i2s_stream.h"
#import "mp3_decoder.h"
#import "esp_peripherals.h"
#import "periph_touch.h"
#import "periph_adc_button.h"
#import "periph_button.h"
#import "board.h"

#import "esp_system.h"
#import "driver/gpio.h"
#import "driver/i2c.h"
#import "sdkconfig.h"
#import "rom/uart.h"

#import "smbus.h"
#import "i2c-lcd1602.h"

void display_volume(int volume, i2c_lcd1602_info_t *lcd_info);
void listenmbutts();

// ////////////////
// VOLUME VOORBEELD
// ////////////////

// we need the volume variable shared across all the things
extern int player_volume;
// we need the lcd info to be able to draw on the screen (alternative: lcd.c)
extern i2c_lcd1602_info_t *lcd_info;

bool listeningtomediabuttons = false;

// audio stuff
audio_event_iface_handle_t evt;
esp_periph_set_handle_t set;
/**
Constructs the volume screen completely
*/
void volume_constructor(){
    // setup events
    ESP_LOGI(TAG, "[ 4 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = {
		.task_stack         = DEFAULT_ESP_PERIPH_STACK_SIZE,
		.task_prio          = DEFAULT_ESP_PERIPH_TASK_PRIO,
		.task_core          = DEFAULT_ESP_PERIPH_TASK_CORE,
	};
    set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[4.1] Initialize keys on board");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 5 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = {                   
        .internal_queue_size = DEFAULT_AUDIO_EVENT_IFACE_SIZE,  
        .external_queue_size = DEFAULT_AUDIO_EVENT_IFACE_SIZE,  
        .queue_set_size = DEFAULT_AUDIO_EVENT_IFACE_SIZE,       
        .on_cmd = NULL,                                         
        .context = NULL,                                        
        .wait_time = portMAX_DELAY,                             
        .type = 0,                                              
    };
    evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[5.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    // display gui
    display_volume(player_volume, lcd_info);
    // start listening to buttons
    xTaskCreate(listenmbutts, "listenmbutts", 1024 * 4, NULL, 10, NULL);
}
/**
Destructs the volume state
*/
void volume_destructor(){
    // stop listening to mediabuttons
    listeningtomediabuttons = false;
    // clean up is done at the end of the listenmbutts function
}

// ////////////////
// VOLUME VOORBEELD
// ////////////////

i2c_lcd1602_info_t *lcd_info;
/**
Turns the volume up on the ESP
*/
void volumeUp()
{
    printf("turning volume up\n");
    player_volume += 5;
    if (player_volume > 100)
    {
        player_volume = 100;
    }
    display_volume((player_volume / 5), lcd_info);
}
/**
Turns the volume up on the ESP
*/
void volumeDown()
{
    printf("turning volume down\n");
    player_volume -= 5;
    if (player_volume < 0)
    {
        player_volume = 0;
    }
    display_volume((player_volume / 5), lcd_info);
}

static uint8_t _wait_for_user(void)
{
    uint8_t c = 0;
    vTaskDelay(50 / portTICK_RATE_MS);
    return c;
}
/**
Displays the volume of the ESP
 @param volume the volume the ESP is on
 @param lcd_info the LCD info struct
*/
void display_volume(int volume, i2c_lcd1602_info_t *lcd_info)
{
    //i2c_lcd1602_info_t* lcd_info = i2c_lcd1602_malloc();
    i2c_lcd1602_clear(lcd_info);
    i2c_lcd1602_write_string(lcd_info, "Volume");

    i2c_lcd1602_move_cursor(lcd_info, 0, 1);
    i2c_lcd1602_write_string(lcd_info, "---------------------");
    i2c_lcd1602_move_cursor(lcd_info, 0, 3);
    i2c_lcd1602_write_string(lcd_info, "---------------------");
    for (int i = 0; i < volume; i++)
    {
        i2c_lcd1602_move_cursor(lcd_info, i, 2);
        i2c_lcd1602_write_char(lcd_info, '#');
    }

    i2c_lcd1602_move_cursor(lcd_info, 0, 0);
    i2c_lcd1602_write_char(lcd_info, 'V');
}
/**
Listens to the buttons constantly. This is ran as a task.
*/
void listenmbutts(void *pvParameters)
{
    listeningtomediabuttons = true;
    while (listeningtomediabuttons)
    {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }
        if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN) && (msg.cmd == PERIPH_TOUCH_TAP || msg.cmd == PERIPH_BUTTON_PRESSED || msg.cmd == PERIPH_ADC_BUTTON_PRESSED))
        {
            if ((int)msg.data == get_input_volup_id())
            {
                volumeUp();
            }
            else if ((int)msg.data == get_input_voldown_id())
            {
                volumeDown();
            }
        }
    }
    esp_periph_set_destroy(set);
    vTaskDelete(NULL);
}