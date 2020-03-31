#pragma once

#ifndef MCP_GPIO_H
#define MCP_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

    // Error library
#include "esp_err.h"

// I2C driver
#include "driver/i2c.h"

// FreeRTOS (for delay)
#include "freertos/task.h"

// registers
#define RTEncoder_COLOR_RED		            0x0D
#define RTEncoder_COLOR_GREEN 	            0x0E
#define RTEncoder_COLOR_BLUE	            0x0F
#define RTEncoder_KNOB_TICK_COUNT_LSB       0x05
#define RTEncoder_KNOB_TICK_COUNT_MSB       0x06
#define RTEncoder_KNOB_TICK_DIFFERENCE_LSB  0x07
#define RTEncoder_KNOB_TICK_DIFFERENCE_MSB  0x08

#define RTEncoder_DEFAULT_ADDR	            0x3F

/*
   mcp23017_err_t

   Specifies an error code returned by functions
   in the MCP23017 API
*/
    typedef enum {
        MCP23017_ERR_OK = 0x00,
        MCP23017_ERR_CONFIG = 0x01,
        MCP23017_ERR_INSTALL = 0x02,
        MCP23017_ERR_FAIL = 0x03
    } mcp23017_err_t;

    /*
       R/W bits
    */

#ifndef WRITE_BIT
#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#endif

#ifndef READ_BIT
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#endif

    // bit to enable checking for ACK
#ifndef ACK_CHECK_EN
#define ACK_CHECK_EN   0x1
#endif

/*
   RTencoder_reg_t

   Contains the reference registers of the SparkFun Qwicc Twist 

        RTE_COLOR_GREEN = Register of the Green color
        RTE_COLOR_RED = Register of the Red color
        RTE_COLOR_BLUE = Register of the Blue color
        RTE_KNOB_TICK_COUNT_LSB = Register of the tick count of the knob - goes up to 255 (when you don't bitshift the LSB and MSB)
        RTE_KNOB_TICK_COUNT_MSB = Register of the tick count of the knob ^ 
        RTE_KNOB_TICK_DIFFERENCE_LSB = 0x07 = Register of the time difference of the knob, also goes up to 255 (measures the amount of time the twist-knob is left untouched)
        RTE_KNOB_TICK_DIFFERENCE_MSB = 0x08 = Register of the time difference of the knob ^

        NOTE: There are more registers which are Readable and Writable
        Check: https://cdn.sparkfun.com/assets/learn_tutorials/8/4/6/Qwiic_Twist_Register_Map_-_Fixed_Endianness.pdf
*/
    typedef enum {
        RTE_COLOR_GREEN = 0x0D,
        RTE_COLOR_RED = 0x0E,
        RTE_COLOR_BLUE = 0x0F,
        RTE_KNOB_TICK_COUNT_LSB = 0x05,
        RTE_KNOB_TICK_COUNT_MSB = 0x06,
        RTE_KNOB_TICK_DIFFERENCE_LSB = 0x07,
        RTE_KNOB_TICK_DIFFERENCE_MSB = 0x08
    } RTencoder_reg_t;

    /*
       mcp23017_t
       Specifies an interface configuration
    */
    typedef struct {
        uint8_t i2c_addr;
        i2c_port_t port;
        uint8_t sda_pin;
        uint8_t scl_pin;
        gpio_pullup_t sda_pullup_en;
        gpio_pullup_t scl_pullup_en;
    } mcp23017_t;

    /*
       Function prototypes
    */
    mcp23017_err_t RTERedcolorChange(mcp23017_t* mcp, uint8_t value);
    mcp23017_err_t RTEBluecolorChange(mcp23017_t* mcp, uint8_t value);
    mcp23017_err_t RTEGreencolorChange(mcp23017_t* mcp, uint8_t value);
    mcp23017_err_t RTEReadRegister(mcp23017_t* mcp, RTencoder_reg_t rt_reg, uint8_t* data);

#ifdef __cplusplus
}
#endif

#endif