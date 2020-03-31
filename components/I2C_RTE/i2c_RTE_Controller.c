#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "i2c_RTE_Module.h"
#include "i2c_RTE_Controller.h"

RTE_Value_Comparator knobComparator;
extern mcp23017_t mcp23017;

void RTEChangeDetect_Task(void* pvParameter) {
	knobComparator.valOld = 0;
	while (1) {
		vTaskDelay(100 / portTICK_RATE_MS);

		RTEReadRegister(&mcp23017, RTE_KNOB_TICK_COUNT_LSB, &knobComparator.valNew);
		if (knobComparator.valNew > knobComparator.valOld) {
			printf("Knob is turned right");
			knobComparator.statusValue = 1;
			//MOVED TO THE RIGHT
		}
		else if (knobComparator.valNew < knobComparator.valOld) {
			printf("Knob is turned left");
			knobComparator.statusValue = -1;
			//MOVED TO THE LEFT
		}
		else if (knobComparator.valOld == knobComparator.valNew)
		{
			printf("Knob is idle");
			knobComparator.statusValue = 0;
		}
		knobComparator.valOld = knobComparator.valNew;
	}
}