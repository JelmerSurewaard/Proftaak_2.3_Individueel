/* LwIP SNTP example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "talking_clock.h"
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"

static const char *TAG = "TALKING_CLOCK";

esp_err_t talking_clock_init()
{
	// Initialize queue
	ESP_LOGI(TAG, "Creating FreeRTOS queue for talking clock");
	talking_clock_queue = xQueueCreate(10, sizeof(int));

	if (talking_clock_queue == NULL)
	{
		ESP_LOGE(TAG, "Error creating queue");
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t talking_clock_fill_queue()
{
	ESP_LOGI(TAG, "Elvis is entering the fill queue method");
	time_t now;
	struct tm timeinfo;
	time(&now);

	char strftime_buf[64];
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time in Amsterdam is: %s", strftime_buf);

	// Reset queue
	printf("1\n");
	esp_err_t ret = xQueueReset(talking_clock_queue);
	printf("2\n");
	if (ret != ESP_OK)
	{
		printf("3\n");
		ESP_LOGE(TAG, "Cannot reset queue");
	}

	int a[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};

	int hour = timeinfo.tm_hour;
	int minute = timeinfo.tm_min;

	hour = (hour == 0 ? 12 : hour);
	hour = (hour > 12 ? hour % 12 : hour);

	printf("4\n");
	xQueueSend(talking_clock_queue, &a[2 + hour], portMAX_DELAY);
	printf("5\n");
	xQueueSend(talking_clock_queue, &a[1], portMAX_DELAY);
	printf("6\n");
	xQueueSend(talking_clock_queue, &a[2], portMAX_DELAY);
	printf("7\n");
	if (minute != 0)
	{
		if (minute <= 14)
		{
			xQueueSend(talking_clock_queue, &a[2 + minute], portMAX_DELAY);
		}
		else
		{

			if (minute % 10 == 0)
			{
			}
			else
			{
				int minuteSingle = (minute % 10);
				if (minuteSingle != 0)
				{
					xQueueSend(talking_clock_queue, &a[2 + minuteSingle], portMAX_DELAY);
					xQueueSend(talking_clock_queue, &a[2], portMAX_DELAY);
				}
			}
			int typeMin = minute / 10;
			xQueueSend(talking_clock_queue, &a[16 + typeMin], portMAX_DELAY);
			xQueueSend(talking_clock_queue, &a[23], portMAX_DELAY);
		}
	}

	ESP_LOGI(TAG, "Queue filled with %d items", uxQueueMessagesWaiting(talking_clock_queue));

	return ESP_OK;
}
