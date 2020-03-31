#include "i2c_RTE_Module.h"

/*
	struct to compare the value of the twist knob using the 
*/

typedef struct {
	uint8_t valNew;
	uint8_t valOld;
	uint8_t statusValue;
} RTE_Value_Comparator;

/*
	Function Prototypes
*/

void RTEChangeDetect_Task(void* pvParameter);

