#include "./clock/generic.h"

#import <string.h>

#import "nvs_flash.h"
#import "esp_wifi.h"

#import "audio_element.h"
#import "audio_pipeline.h"
#import "audio_event_iface.h"
#import "audio_common.h"
#import "fatfs_stream.h"
#import "i2s_stream.h"
#import "mp3_decoder.h"

#import "esp_peripherals.h"
#import "periph_sdcard.h"
#import "periph_touch.h"
#import "periph_button.h"
#import "periph_wifi.h"
#import "input_key_service.h"

#import "sdcard_list.h"
#import "sdcard_scan.h"

#import "smbus.h"
#import "i2c-lcd1602.h"
#import "sntp_sync.h"
#import "talking_clock.h"

#import <sys/time.h>

TimerHandle_t timer_1_sec;

audio_pipeline_handle_t pipeline;
audio_element_handle_t i2s_stream_writer, mp3_decoder, fatfs_stream_reader;

esp_periph_set_handle_t set;
audio_event_iface_handle_t evt;
periph_service_handle_t input_ser;

/**
 * 
 * Method to get the current time
 * 
**/

void stmp_timesync_event(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");

    time_t now;
    struct tm timeinfo;
    time(&now);

    char strftime_buf[64];
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Amsterdam is: %s", strftime_buf);

    talking_clock_fill_queue();
}

/**
 * Method to check current time and when an hour has passed tells the time 
 * 
 * Params xTimer.... 
**/

void timer_1_sec_callback(TimerHandle_t xTimer)
{
    // Print current time to the screen
    time_t now;
    struct tm timeinfo;
    time(&now);

    char strftime_buf[20];
    localtime_r(&now, &timeinfo);
    sprintf(&strftime_buf[0], "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    size_t timeSize = strlen(strftime_buf);
    // Say the time every hour
    if (timeinfo.tm_min == 0 && timeinfo.tm_sec == 0)
    {
        talking_clock_fill_queue();
        audio_element_set_uri(fatfs_stream_reader, talking_clock_files[TALKING_CLOCK_ITSNOW_INDEX]); // Set first sample
        audio_pipeline_reset_ringbuffer(pipeline);
        audio_pipeline_reset_elements(pipeline);
        audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
        audio_pipeline_run(pipeline);
    }
}

/**
 * Method to initialize the input keys on the LyraT board
 * 
 * Params handle, periphs variable callback - used in another method to use this method initialization
 * Params *evt, event listener of the LyraT board, cannot be used at the same time for audio listening. 
 * Params *ctx, castable variable for the board handler 
**/

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    /* Handle touch pad events
           to start, pause, resume, finish current song and adjust volume
        */
    audio_board_handle_t board_handle = (audio_board_handle_t)ctx;
    int player_volume;
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);

    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE)
    {
        ESP_LOGI(TAG, "[ * ] input key id is %d", (int)evt->data);
        switch ((int)evt->data)
        {
        case INPUT_KEY_USER_ID_PLAY:
        {
            ESP_LOGI(TAG, "[ * ] [Play] input key event");
            audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
            switch (el_state)
            {
            case AEL_STATE_INIT:
                ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
                talking_clock_fill_queue();
                audio_element_set_uri(fatfs_stream_reader, talking_clock_files[TALKING_CLOCK_ITSNOW_INDEX]);
                audio_pipeline_run(pipeline);
                break;
            case AEL_STATE_RUNNING:
                ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
                audio_pipeline_pause(pipeline);
                // Clear Queue
                break;
            case AEL_STATE_PAUSED:
                ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
                // Create new queue
                // Set first item in the queue
                talking_clock_fill_queue();
                audio_element_set_uri(fatfs_stream_reader, talking_clock_files[TALKING_CLOCK_ITSNOW_INDEX]); // Set first sample
                audio_pipeline_reset_ringbuffer(pipeline);
                audio_pipeline_reset_elements(pipeline);
                audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
                audio_pipeline_run(pipeline);

                break;
            default:
                ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
            }
            break;
        }
        case INPUT_KEY_USER_ID_VOLUP:
        {
            ESP_LOGI(TAG, "[ * ] [Vol+] input key event");
            player_volume += 10;
            if (player_volume > 100)
            {
                player_volume = 100;
            }
            audio_hal_set_volume(board_handle->audio_hal, player_volume);
            ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            break;
        }
        case INPUT_KEY_USER_ID_VOLDOWN:
        {
            ESP_LOGI(TAG, "[ * ] [Vol-] input key event");
            player_volume -= 10;
            if (player_volume < 0)
            {
                player_volume = 0;
            }
            audio_hal_set_volume(board_handle->audio_hal, player_volume);
            ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            break;
        }
        }
    }

    return ESP_OK;
}

/**
 * Method to initialize the clock, sets the logging level, initializes the touch periphs, inits the audio board handler,
 * set the wifi/network connection, sets up the audio streams (i2s, mp3, fatfs) to prepare the talking clock, sets up te event listeners,
 * gathers globale volume levels, initializes the 1_second_timer callback to display the time on the logs and lastly starts up the whole listening process
 * 
**/

void init_clock()
{

    tcpip_adapter_init();

    // Setup logging level
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = {
		.task_stack         = DEFAULT_ESP_PERIPH_STACK_SIZE,
		.task_prio          = DEFAULT_ESP_PERIPH_TASK_PRIO,
		.task_core          = DEFAULT_ESP_PERIPH_TASK_CORE,
	};
    set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Initialize and start peripherals");
    audio_board_key_init(set);
    audio_board_sdcard_init(set);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Start and wait for Wi-Fi network");
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    if (strlen(CONFIG_WIFI_IDENTITY) > 0)
        wifi_cfg.identity = strdup(CONFIG_WIFI_IDENTITY);

    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    ESP_LOGI(TAG, "[ 3 ] Create and start input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_ser = input_key_service_create(set);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void *)board_handle);

    ESP_LOGI(TAG, "[4.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[4.1] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[4.2] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[4.4] Create fatfs stream to read data from sdcard");
    char *url = NULL;
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);
    audio_element_set_uri(fatfs_stream_reader, url);

    ESP_LOGI(TAG, "[4.5] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[4.6] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline, (const char *[]){"file", "mp3", "i2s"}, 3);

    ESP_LOGI(TAG, "[5.0] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = {                   
        .internal_queue_size = DEFAULT_AUDIO_EVENT_IFACE_SIZE,  
        .external_queue_size = DEFAULT_AUDIO_EVENT_IFACE_SIZE,  
        .queue_set_size = DEFAULT_AUDIO_EVENT_IFACE_SIZE,       
        .on_cmd = NULL,                                         
        .context = NULL,                                        
        .wait_time = portMAX_DELAY,                             
        .type = 0,                                              
    };
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[5.1] Listen for all pipeline events");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGW(TAG, "[ 6 ] Press the keys to control talking clock");
    ESP_LOGW(TAG, "      [Play] to start reading time");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");

    // Initialize Talking clock method
    talking_clock_init();

    // Synchronize NTP time
    sntp_sync(stmp_timesync_event);

    // Setup first audio sample 'it's now'
    audio_element_set_uri(fatfs_stream_reader, talking_clock_files[TALKING_CLOCK_ITSNOW_INDEX]);

    // Initialize 1 second timer to display the time
    int id = 1;
    timer_1_sec = xTimerCreate("MyTimer", pdMS_TO_TICKS(1000), pdTRUE, (void *)id, &timer_1_sec_callback);
    if (xTimerStart(timer_1_sec, 10) != pdPASS)
    {
        ESP_LOGE(TAG, "Cannot start 1 second timer");
    }

    printf("1\n");
    while (1)
    {
        /* 
            Handle event interface messages from pipeline
            to set music info and to advance to the next song
        */
        printf("12\n");
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);

        printf("13\n");
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT)
        {
            printf("14\n");
            // Set music info for a new song to be played
            if (msg.source == (void *)mp3_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
            {
                printf("15\n");
                audio_element_info_t music_info = {};
                audio_element_getinfo(mp3_decoder, &music_info);
                ESP_LOGI(TAG, "[ * ] Received music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels);
                audio_element_setinfo(i2s_stream_writer, &music_info);
                continue;
            }
            // Advance to the next song when previous finishes
            if (msg.source == (void *)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS)
            {
                printf("16\n");
                audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
                if (el_state == AEL_STATE_FINISHED)
                {
                    int element = 0;
                    if (uxQueueMessagesWaiting(talking_clock_queue) > 0 &&
                        xQueueReceive(talking_clock_queue, &element, portMAX_DELAY))
                    {
                        ESP_LOGI(TAG, "Finish sample, towards next sample");
                        url = talking_clock_files[element];
                        ESP_LOGI(TAG, "URL: %s", url);
                        audio_element_set_uri(fatfs_stream_reader, url);
                        audio_pipeline_reset_ringbuffer(pipeline);
                        audio_pipeline_reset_elements(pipeline);
                        audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
                        audio_pipeline_run(pipeline);
                    }
                    else
                    {
                        // No more samples. Pause for now
                        audio_pipeline_pause(pipeline);
                    }
                }
                continue;
            }
        }
    }
}

/**
 * 
 * Method to deconstruct the talking clock, removes the event listener, unregisters the pipelines and finally deïnitializes the audio elements.
 *  
**/

void destroy_clock()
{
    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    vTaskDelete(timer_1_sec);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, mp3_decoder);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);
    esp_periph_set_destroy(set);
    periph_service_destroy(input_ser);

    // i2c_lcd1602_free(&lcd_info);
    // smbus_free(&smbus_info);
}