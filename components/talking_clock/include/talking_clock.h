#ifndef TALKING_CLOCK_H
#define TALKING_CLOCK_H

// --- #includes

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- #defines

#define TALKING_CLOCK_MAX_STRING 40
#define TALKING_CLOCK_ITEMS 30

#define TALKING_CLOCK_ITSNOW_INDEX 0
#define TALKING_CLOCK_HOUR_INDEX 1
#define TALKING_CLOCK_AND_INDEX 2
#define TALKING_CLOCK_1_INDEX 3
#define TALKING_CLOCK_2_INDEX 4
#define TALKING_CLOCK_3_INDEX 5
#define TALKING_CLOCK_4_INDEX 6
#define TALKING_CLOCK_5_INDEX 7
#define TALKING_CLOCK_6_INDEX 8
#define TALKING_CLOCK_7_INDEX 9
#define TALKING_CLOCK_8_INDEX 10
#define TALKING_CLOCK_9_INDEX 11
#define TALKING_CLOCK_10_INDEX 12
#define TALKING_CLOCK_11_INDEX 13
#define TALKING_CLOCK_12_INDEX 14
#define TALKING_CLOCK_13_INDEX 15
#define TALKING_CLOCK_14_INDEX 16
#define TALKING_CLOCK_20_INDEX 17
#define TALKING_CLOCK_30_INDEX 18
#define TALKING_CLOCK_40_INDEX 19
#define TALKING_CLOCK_50_INDEX 20
#define TALKING_CLOCK_VAND_INDEX 21

// --- variables

static char talking_clock_files[TALKING_CLOCK_ITEMS][TALKING_CLOCK_MAX_STRING] = { // static array list with all the mp3 file locations/names
	"/sdcard/het_is.mp3",
	"/sdcard/uur.mp3",
	"/sdcard/en.mp3",
	"/sdcard/1.mp3",
	"/sdcard/2.mp3",
	"/sdcard/3.mp3",
	"/sdcard/4.mp3",
	"/sdcard/5.mp3",
	"/sdcard/6.mp3",
	"/sdcard/7.mp3",
	"/sdcard/8.mp3",
	"/sdcard/9.mp3",
	"/sdcard/10.mp3",
	"/sdcard/11.mp3",
	"/sdcard/12.mp3",
	"/sdcard/13.mp3",
    "/sdcard/14.mp3",
    "/sdcard/10.mp3",
    "/sdcard/20.mp3",
    "/sdcard/30.mp3",
    "/sdcard/40.mp3",
    "/sdcard/50.mp3",
    "/sdcard/van.mp3",
    "/sdcard/min.mp3"
};

QueueHandle_t talking_clock_queue; 

// --- method prototypes

esp_err_t talking_clock_init();
esp_err_t talking_clock_fill_queue();

#ifdef __cplusplus
}
#endif

#endif  // TALKING_CLOCK_H
