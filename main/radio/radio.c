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
#import"./screen/lcd.c"

void initRadio();
void printRadioScreen();
void startAudioChip();
void deleteRadio();
void stopRadio();
void ChangeRadioFrequenty();
void listenToMediaButtons();

static int radioCounter = 0;
const int totalRadioAmount = 3;

audio_pipeline_handle_t RadioPipeline;
esp_periph_set_handle_t set;
audio_event_iface_handle_t evt;
audio_element_handle_t http_stream_reader, i2s_stream_writer, mp3_decoder;
extern int player_volume;
audio_board_handle_t board_handle;

#define true 1
#define false 0
#define boolean int
boolean radioRunning;

bool listeningToRadioMediaButtons = false;

/**
A struct that holds all the information of a radio station
*/
typedef struct {
    char* radioName;
    char* frequenty;
} radioObject;

typedef struct {
    radioObject radioObject[3];
}RadioList;

RadioList radioList = {
    {
    {
        "Radio 2",
        "https://icecast.omroep.nl/radio2-bb-mp3"
    },
    {
        "100% NL",
        "https://stream.100p.nl/100pctnl.mp3"
    },
    {
        "Feest!",
        "https://stream.100p.nl/web01_mp3"
    },
    }
};

void radio_constructor() {
    radioRunning = true;
    ESP_LOGI(TAG, "Hij komt in de radio_constructor");
    initRadio();
}
/**
Prints the radio information on the LCD screen
*/

void radio_destructor() {
    radioRunning = false;
    listeningToRadioMediaButtons = false;

    deleteRadio();
}

int _http_stream_event_handle(http_stream_event_msg_t* msg)
{
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_restart(msg->el);
    }
    return ESP_OK;
}

void runRadioTask(void* pvParameter) {
    while (radioRunning == true) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void*)mp3_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = { 0 };
            audio_element_getinfo(mp3_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        /* restart stream when the first pipeline element (http_stream_reader in this case) receives stop event (caused by reading errors) */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void*)http_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (int)msg.data == AEL_STATUS_ERROR_OPEN) {
            ESP_LOGW(TAG, "[ * ] Restart stream");
            audio_pipeline_stop(RadioPipeline);
            audio_pipeline_wait_for_stop(RadioPipeline);
            audio_element_reset_state(mp3_decoder);
            audio_element_reset_state(i2s_stream_writer);
            audio_pipeline_reset_ringbuffer(RadioPipeline);
            audio_pipeline_reset_items_state(RadioPipeline);
            audio_pipeline_run(RadioPipeline);
            continue;
        }

        if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
            && (msg.cmd == PERIPH_TOUCH_TAP || msg.cmd == PERIPH_BUTTON_PRESSED || msg.cmd == PERIPH_ADC_BUTTON_PRESSED)) {

            if ((int)msg.data == get_input_play_id()) {
                ESP_LOGI(TAG, "[ * ] [Play] touch tap event");
                audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
                switch (el_state) {
                case AEL_STATE_INIT:
                    ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
                    audio_pipeline_run(RadioPipeline);
                    break;
                case AEL_STATE_RUNNING:
                    ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
                    audio_pipeline_pause(RadioPipeline);
                    break;
                case AEL_STATE_PAUSED:
                    ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
                    audio_pipeline_resume(RadioPipeline);
                    break;
                case AEL_STATE_FINISHED:
                    ESP_LOGI(TAG, "[ * ] Rewinding audio pipeline");
                    audio_pipeline_stop(RadioPipeline);
                    audio_pipeline_wait_for_stop(RadioPipeline);
                    //rewind to start
                    audio_pipeline_resume(RadioPipeline);
                    break;
                default:
                    ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
                }
            }
            /* else if ((int)msg.data == get_input_set_id()) {
                 ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
                 ESP_LOGI(TAG, "[ * ] Stopping audio pipeline");
                 break;
             }*/
             /* else if ((int)msg.data == get_input_volup_id()) {
                  ESP_LOGI(TAG, "[ * ] [Vol+] touch tap event");
                  player_volume += 10;
                  if (player_volume > 100) {
                      player_volume = 100;
                  }
                  audio_hal_set_volume(board_handle->audio_hal, player_volume);
                  ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
              }
              else if ((int)msg.data == get_input_voldown_id()) {
                  ESP_LOGI(TAG, "[ * ] [Vol-] touch tap event");
                  player_volume -= 10;
                  if (player_volume < 0) {
                      player_volume = 0;
                  }
                  audio_hal_set_volume(board_handle->audio_hal, player_volume);
                  ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
              }*/
        }
    }
}


void initRadio() {

    startAudioChip();

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    audio_hal_get_volume(board_handle->audio_hal, &player_volume);

    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    RadioPipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[2.1] Create http stream to read data");
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    http_stream_reader = http_stream_init(&http_cfg);

    ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[2.3] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[2.4] Register all elements to audio pipeline");
    audio_pipeline_register(RadioPipeline, http_stream_reader, "http");
    audio_pipeline_register(RadioPipeline, mp3_decoder, "mp3");
    audio_pipeline_register(RadioPipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(RadioPipeline, (const char* []) { "http", "mp3", "i2s" }, 3);

    ESP_LOGI(TAG, "[2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)");
    //audio_element_set_uri(http_stream_reader, mp3_STREAM_URI);
    audio_element_set_uri(http_stream_reader, radioList.radioObject[radioCounter].frequenty);

    ESP_LOGI(TAG, "[4] Initialize keys on board");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 5 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[5.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(RadioPipeline, evt);

    ESP_LOGI(TAG, "[5.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGW(TAG, "[ 6 ] Tap touch buttons to control music player:");
    ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] to stop.");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");
    audio_pipeline_run(RadioPipeline);

    xTaskCreate(listenToMediaButtons, "listenmbutts", 1024 * 4, NULL, 10, NULL);

    xTaskCreate(runRadioTask, "radio-task", 4096, NULL, 5, NULL);

    printRadioScreen();
}

void ChangeRadioFrequenty() {

    ESP_LOGW(TAG, "[ * ] Restart stream");
    audio_pipeline_stop(RadioPipeline);
    audio_pipeline_wait_for_stop(RadioPipeline);
    audio_element_reset_state(mp3_decoder);
    audio_element_reset_state(http_stream_reader);
    audio_element_set_uri(http_stream_reader, radioList.radioObject[radioCounter].frequenty);
    audio_element_reset_state(i2s_stream_writer);
    audio_pipeline_reset_ringbuffer(RadioPipeline);
    audio_pipeline_reset_items_state(RadioPipeline);
    audio_pipeline_run(RadioPipeline);

    printRadioScreen();
}

void deleteRadio() {
    ESP_LOGI(TAG, "Stop audio_pipeline");
    audio_pipeline_terminate(RadioPipeline);

    audio_pipeline_unregister(RadioPipeline, http_stream_reader);
    audio_pipeline_unregister(RadioPipeline, i2s_stream_writer);
    audio_pipeline_unregister(RadioPipeline, mp3_decoder);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(RadioPipeline);

    ///* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(RadioPipeline);
    audio_element_deinit(http_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);

    //esp_periph_set_destroy(set);

    //board_handle = NULL;
}

void stopRadio() {
    radioRunning = false;
    audio_pipeline_pause(RadioPipeline);
}

void printRadioScreen() {
    clearScreen();
    moveCursor(0, 0);
    writeLineOnPosition(0, 0, "Radio");
    writeLineOnPosition(0, 2, "> ");
    writeLineOnPosition(2, 2, (radioList.radioObject[radioCounter].radioName));

    if (radioCounter - 1 < 0) {
        int tempRadioCounter = totalRadioAmount - 1;
        writeLineOnPosition(0, 1, (radioList.radioObject[tempRadioCounter].radioName));
    }
    else {
        writeLineOnPosition(0, 1, (radioList.radioObject[radioCounter - 1].radioName));
    }

    if (radioCounter + 1 > totalRadioAmount - 1) {
        int tempRadioCounter = 0;
        writeLineOnPosition(0, 3, (radioList.radioObject[tempRadioCounter].radioName));
    }
    else {
        writeLineOnPosition(0, 3, (radioList.radioObject[radioCounter + 1].radioName));
    }
}

void startAudioChip() {
    ESP_LOGI(TAG, "Start audio codec chip");
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
}

void radioNext()
{
    printf("setting next Radio frequenty\n");
    if (radioCounter == 2)
    {
        radioCounter = 0;
    }
    else
    {
        radioCounter++;
    }

    ESP_LOGI(TAG, "Radiocounter: %d", radioCounter);

    ChangeRadioFrequenty();

}

void radioLast()
{
    printf("setting last Radio frequenty\n");

    if (radioCounter == 0)
    {
        radioCounter = 2;
    }
    else
    {
        radioCounter--;
    }

    ESP_LOGI(TAG, "Radiocounter: %d", radioCounter);

    ChangeRadioFrequenty();
}

void listenToMediaButtons(void* pvParameters)
{
    listeningToRadioMediaButtons = true;
    while (listeningToRadioMediaButtons)
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
                radioNext();
            }
            else if ((int)msg.data == get_input_voldown_id())
            {
                radioLast();
            }
        }
    }
    // esp_periph_set_destroy(set);
    vTaskDelete(NULL);
}
