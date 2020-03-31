#ifndef GENERIC_H
#define GENERIC_H


// File with generic used includes and settings.
// Used for the LCD to write the clock data on the LCD-screen.


#define LCD_NUM_ROWS			 4
#define LCD_NUM_COLUMNS			 40
#define LCD_NUM_VIS_COLUMNS		 20
#define LCD_ADDRESS				 0x27


#import "freertos/FreeRTOS.h"
#import "freertos/task.h"
#import "freertos/timers.h"
#import "freertos/event_groups.h"
#import "freertos/queue.h"

#import "board.h"

#import "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif


#endif