/* Example of Goertzel filter
   Author: P.S.M. Goossens

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "board.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include <math.h>

#include "freertos/task.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "fatfs_stream.h"
#include "wav_encoder.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"

#include "wav_decoder.h"

#include "freertos/timers.h"
#include "audio_mem.h"
#include "audio_sonic.h"
#include "periph_button.h"

#include "goertzel.h"

// static const char *TAG = "EXAMPLE-GOERTZEL";

audio_pipeline_handle_t pipeline;
audio_board_handle_t board_handle;

static bool heared_whistle = false;

#define GOERTZEL_SAMPLE_RATE_HZ 8000 // Sample rate in [Hz]
#define GOERTZEL_FRAME_LENGTH_MS 100 // Block length in [ms]
#define GOERTZEL_BUFFER_LENGTH (GOERTZEL_FRAME_LENGTH_MS * GOERTZEL_SAMPLE_RATE_HZ / 1000)

#define AUDIO_SAMPLE_RATE 48000 // Default sample rate in [Hz]
#define CHANNEL 1
#define BITS 16

#define SONIC_PITCH 1.4f

#define GOERTZEL_N_DETECTION 5
static const int GOERTZEL_DETECT_FREQUENCIES[GOERTZEL_N_DETECTION] = {
    880,
    960,
    1023,
    1120,
    1249};

/**
 Callback where goertzel the edited data returns of the sound. 
 @param filter Struct with Goertzel data
 @param result The DB of the result
*/
static void goertzel_callback(struct goertzel_data_t *filter, float result)
{
    const char *TAG = "GOERTZEL_CALLBACK";

    goertzel_data_t *filt = (goertzel_data_t *)filter;
    float logVal = 10.0f * log10f(result);

    // Detection filter. Only above 25 dB(A)
    if (logVal > 25.0f)
    {
        ESP_LOGI(TAG, "[Goertzel_callback] Freq: %d Hz amplitude: %.2f", filt->target_frequency, 10.0f * log10f(result));
        if (heared_whistle != true)
        {
            ESP_LOGI(TAG, "");
            heared_whistle = true;
        }
    }
}

static audio_element_handle_t create_sonic()
{
    sonic_cfg_t sonic_cfg = DEFAULT_SONIC_CONFIG();
    sonic_cfg.sonic_info.samplerate = AUDIO_SAMPLE_RATE;
    sonic_cfg.sonic_info.channel = CHANNEL;
    sonic_cfg.sonic_info.resample_linear_interpolate = 1;
    return sonic_init(&sonic_cfg);
}

static audio_element_handle_t create_fatfs_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = type;
    audio_element_handle_t fatfs_stream = fatfs_stream_init(&fatfs_cfg);
    mem_assert(fatfs_stream);
    audio_element_info_t writer_info = {0};
    audio_element_getinfo(fatfs_stream, &writer_info);
    writer_info.bits = bits;
    writer_info.channels = channels;
    writer_info.sample_rates = sample_rates;
    audio_element_setinfo(fatfs_stream, &writer_info);
    return fatfs_stream;
}

static audio_element_handle_t create_i2s_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = type;
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    if (i2s_cfg.type == AUDIO_STREAM_READER)
    {
        i2s_cfg.i2s_port = 1;
        i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    }
#endif
    audio_element_handle_t i2s_stream = i2s_stream_init(&i2s_cfg);
    mem_assert(i2s_stream);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream, &i2s_info);
    i2s_info.bits = bits;
    i2s_info.channels = channels;
    i2s_info.sample_rates = sample_rates;
    audio_element_setinfo(i2s_stream, &i2s_info);
    return i2s_stream;
}
/**
Creates a wav encoder
 @return audio_element_handle_t Is the audio element handle of the encoded object.
*/
static audio_element_handle_t create_wav_encoder()
{
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    return wav_encoder_init(&wav_cfg);
}
/**
Creates a wav decoder
 @return audio_element_handle_t Is the audio element handle of the decoded object.
*/
static audio_element_handle_t create_wav_decoder()
{
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    return wav_decoder_init(&wav_cfg);
}
/**
The method that waits for a whistle or clap.
*/
void wait_for_whistle()
{
    const char *TAG = "WHISTEL-WAIT";

    audio_element_handle_t i2s_stream_reader, filter, raw_read;

    ESP_LOGI(TAG, "[ 1 ] -Skipped-       Start codec chip");
    // audio_board_handle_t board_handle = audio_board_init();
    // audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 2 ] -Skipped-       Create audio pipeline for recording");
    // audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    // pipeline = audio_pipeline_init(&pipeline_cfg);
    // mem_assert(pipeline);

    ESP_LOGI(TAG, "[2.1] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_config.sample_rate = AUDIO_SAMPLE_RATE;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    // Filter for reducing sample rate
    ESP_LOGI(TAG, "[2.2] Create filter to resample audio data");
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = AUDIO_SAMPLE_RATE;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = GOERTZEL_SAMPLE_RATE_HZ;
    rsp_cfg.dest_ch = 1;
    filter = rsp_filter_init(&rsp_cfg);

    ESP_LOGI(TAG, "[2.3] Create raw to receive data");
    raw_stream_cfg_t raw_cfg = {
        .out_rb_size = 8 * 1024,
        .type = AUDIO_STREAM_READER,
    };
    raw_read = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[ 3 ] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, filter, "filter");
    audio_pipeline_register(pipeline, raw_read, "raw");

    ESP_LOGI(TAG, "[ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[Goertzel]");
    audio_pipeline_link(pipeline, (const char *[]){"i2s", "filter", "raw"}, 3);

    // Config goertzel filters
    goertzel_data_t **goertzel_filt = goertzel_malloc(GOERTZEL_N_DETECTION); // Alloc mem
    // Apply configuration for all filters
    for (int i = 0; i < GOERTZEL_N_DETECTION; i++)
    {
        goertzel_data_t *currFilter = goertzel_filt[i];
        currFilter->samples = GOERTZEL_BUFFER_LENGTH;
        currFilter->sample_rate = GOERTZEL_SAMPLE_RATE_HZ;
        currFilter->target_frequency = GOERTZEL_DETECT_FREQUENCIES[i];
        currFilter->goertzel_cb = &goertzel_callback;
    }

    // Initialize goertzel filters
    goertzel_init_configs(goertzel_filt, GOERTZEL_N_DETECTION);

    ESP_LOGI(TAG, "[ 6 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    bool noError = true;
    int16_t *raw_buff = (int16_t *)malloc(GOERTZEL_BUFFER_LENGTH * sizeof(short));
    if (raw_buff == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation failed!");
        noError = false;
    }

    if (noError)
    {
        while (!heared_whistle)
        {
            raw_stream_read(raw_read, (char *)raw_buff, GOERTZEL_BUFFER_LENGTH * sizeof(short));

            // process Goertzel Samples
            goertzel_proces(goertzel_filt, GOERTZEL_N_DETECTION, raw_buff, GOERTZEL_BUFFER_LENGTH);
        }
    }

    if (raw_buff != NULL)
    {
        free(raw_buff);
        raw_buff = NULL;
    }

    ESP_LOGI(TAG, "[ 7 ] Destroy goertzel");
    goertzel_free(goertzel_filt);

    ESP_LOGI(TAG, "[ 8 ] Stop audio_pipeline and release all resources");
    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, filter);
    audio_pipeline_unregister(pipeline, raw_read);

    /* Release all resources */
    // audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(filter);
    audio_element_deinit(raw_read);
}

bool has_run_before = false;

esp_periph_set_handle_t set = NULL;

/**
Records sounds to be replayed later
 @param seconds_to_record How many seconds the ESP should record.
*/
void record(int seconds_to_record)
{
    const char *TAG = "RECORDING";

    audio_element_handle_t fatfs_stream_writer, i2s_stream_reader, wav_encoder;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 1 ] Mount sdcard");
    // Initialize peripherals management
    if (!has_run_before)
    {
        esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
        set = esp_periph_set_init(&periph_cfg);

        // Initialize SD Card peripheral
        audio_board_sdcard_init(set);
        has_run_before = true;
    }

    ESP_LOGI(TAG, "[ 2 ] -Skipped-      Start codec chip");
    // audio_board_handle_t board_handle = audio_board_init();
    // audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] -Skipped-      Create audio pipeline for recording");
    // audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    // pipeline = audio_pipeline_init(&pipeline_cfg);
    // mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create fatfs stream to write data to sdcard");
    fatfs_stream_writer = create_fatfs_stream(AUDIO_SAMPLE_RATE, BITS, CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_reader = create_i2s_stream(AUDIO_SAMPLE_RATE, BITS, CHANNEL, AUDIO_STREAM_READER);

    ESP_LOGI(TAG, "[3.3] Create wav encoder to encode wav format");
    wav_encoder = create_wav_encoder();

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    /**
     * Wav encoder actually passes data without doing anything, which makes the pipeline structure easy to understand.
     * Because WAV is raw data and audio information is stored in the header,
     * I2S Stream will write the WAV header after ending the record with enough the information
     */
    audio_pipeline_register(pipeline, wav_encoder, "wav");
    audio_pipeline_register(pipeline, fatfs_stream_writer, "file");

    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]");
    audio_pipeline_link(pipeline, (const char *[]){"i2s", "wav", "file"}, 3);

    ESP_LOGI(TAG, "[3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)");
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.wav");

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    i2s_stream_set_clk(i2s_stream_reader, AUDIO_SAMPLE_RATE, BITS, CHANNEL);
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events, record for %d Seconds", seconds_to_record);
    int second_recorded = 0;
    while (1)
    {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, 1000 / portTICK_RATE_MS) != ESP_OK)
        {
            second_recorded++;
            ESP_LOGI(TAG, "[ * ] Recording ... %d", second_recorded);
            if (second_recorded >= seconds_to_record)
            {
                break;
            }
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_reader in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_reader && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED)))
        {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }
    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, wav_encoder);
    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, fatfs_stream_writer);
    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);
    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);
    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);
    /* Release all resources */
    // audio_pipeline_deinit(pipeline);
    audio_element_deinit(fatfs_stream_writer);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(wav_encoder);
}
/**
Plays a recording
*/
void play_recording()
{
    const char *TAG = "PLAY_WAV";

    audio_element_handle_t fatfs_stream_reader, i2s_stream_writer, wav_decoder, sonic;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 1 ] -Skipped-      Mount sdcard");
    // Initialize peripherals management
    // esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    // esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // Initialize SD Card peripheral
    // audio_board_sdcard_init(set);

    ESP_LOGI(TAG, "[ 2 ] -Skipped-      Start codec chip");
    // audio_board_handle_t board_handle = audio_board_init();
    // audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] -Skipped-      Create audio pipeline for playback");
    // audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    // pipeline = audio_pipeline_init(&pipeline_cfg);
    // mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create fatfs stream to read data from sdcard");
    fatfs_stream_reader = create_fatfs_stream(AUDIO_SAMPLE_RATE, BITS, CHANNEL, AUDIO_STREAM_READER);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to write data to codec chip");
    i2s_stream_writer = create_i2s_stream(AUDIO_SAMPLE_RATE, BITS, CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[3.3] Create wav decoder to decode wav file");
    wav_decoder = create_wav_decoder();

    ESP_LOGI(TAG, "[2.2] Create audio elements for playback pipeline");
    sonic = create_sonic();

    ESP_LOGI(TAG, "[2.3] Register audio elements to playback pipeline");
    audio_pipeline_register(pipeline, fatfs_stream_reader, "file_reader");
    audio_pipeline_register(pipeline, wav_decoder, "wav_decoder");
    audio_pipeline_register(pipeline, sonic, "sonic");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s_writer");
    audio_pipeline_link(pipeline, (const char *[]){"file_reader", "wav_decoder", "sonic", "i2s_writer"}, 4);

    ESP_LOGI(TAG, "[3.6] Set up  uri (file as fatfs_stream, wav as wav decoder, and default output is i2s)");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/rec.wav");
    i2s_stream_set_clk(i2s_stream_writer, AUDIO_SAMPLE_RATE, BITS, CHANNEL);
    sonic_set_pitch_and_speed_info(sonic, SONIC_PITCH, 1.0f);

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events");
    while (1)
    {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)wav_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
        {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(wav_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from wav decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }
        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED)))
        {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, fatfs_stream_reader);
    audio_pipeline_unregister(pipeline, sonic);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, wav_decoder);

    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);

    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    // audio_pipeline_deinit(pipeline);
    audio_element_deinit(fatfs_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(wav_decoder);
    ESP_LOGI(TAG, "[ 8 ] Deinitialize Complete");
    // esp_periph_set_destroy(set);
}

void did_task(void *pvParameter)
{
    while (1)
    {
        wait_for_whistle();
        heared_whistle = false;
        record(5); //records 5 sec of audio
        play_recording();
        vTaskDelay(10);
    }
    vTaskDelete(NULL);
}

void init_did()
{
    const char *TAG = "APP_MAIN";

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    xTaskCreate(did_task, "did_task", 1024 * 8, NULL, 24, NULL);
}

void kill_did()
{
    audio_pipeline_terminate(pipeline);
    vTaskDelete(NULL);
}
