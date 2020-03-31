#include "i2c_RTE_Module.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char* TAG = "i2c_Device";

// disable buffers
static const size_t I2C_MASTER_TX_BUF_DISABLE = 0;
static const size_t I2C_MASTER_RX_BUF_DISABLE = 0;
static const int INTR_FLAGS = 0;

/**
 * Initializes the MCP23017
 * @param mcp the MCP23017 interface structure
 * @return an error code or MCP23017_ERR_OK if no error encountered
*/

/**
Writes a value to change the red light brightness in the rotary encoder
 @param mcp The MCP23017 interface structure
 @param value A value between 0 and 255 for the brightness
 @return an error code or MCP230177_ERR_OK if no error encountered
*/

mcp23017_err_t RTERedcolorChange(mcp23017_t* mcp, uint8_t value) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, RTEncoder_DEFAULT_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, RTEncoder_COLOR_RED, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(mcp->port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if (ret == ESP_FAIL) {
		ESP_LOGE(TAG, "ERROR: unable to write to register");
		return MCP23017_ERR_FAIL;
	}
	return MCP23017_ERR_OK;
}

/**
Writes a value to change the blue light brightness in the rotary encoder
 @param mcp The MCP23017 interface structure
 @param value A value between 0 and 255 for the brightness
 @return an error code or MCP230177_ERR_OK if no error encountered
*/

mcp23017_err_t RTEBluecolorChange(mcp23017_t* mcp, uint8_t value) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, RTEncoder_DEFAULT_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, RTEncoder_COLOR_BLUE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(mcp->port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if (ret == ESP_FAIL) {
		ESP_LOGE(TAG, "ERROR: unable to write to register");
		return MCP23017_ERR_FAIL;
	}
	return MCP23017_ERR_OK;
}

/**
Writes a value to change the green light brightness in the rotary encoder
 @param mcp The MCP23017 interface structure
 @param value A value between 0 and 255 for the brightness
 @return an error code or MCP230177_ERR_OK if no error encountered
*/

mcp23017_err_t RTEGreencolorChange(mcp23017_t* mcp, uint8_t value) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, RTEncoder_DEFAULT_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, RTEncoder_COLOR_GREEN, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(mcp->port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if (ret == ESP_FAIL) {
		ESP_LOGE(TAG, "ERROR: unable to write to register");
		return MCP23017_ERR_FAIL;
	}
	return MCP23017_ERR_OK;
}

/**
Gets the side the rotary encoder is twisted towards
 @param mcp The MCP23017 interface structure
 @param RTencoder_reg_t is the registers which can be read or written to
 @param data A call by refrence object to display data/ As storage
 @return an error code or MCP230177_ERR_OK if no error encountered
*/


mcp23017_err_t RTEReadRegister(mcp23017_t* mcp, RTencoder_reg_t rt_reg,uint8_t* data) {
	// from the generic register and group, derive register address
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (0x3F << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, RTEncoder_KNOB_TICK_COUNT_LSB, 1); // designates the register to be read from the mcp23017 module
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(mcp->port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if (ret == ESP_FAIL) {
		ESP_LOGE(TAG, "ERROR: unable to write address %02x to read reg %02x", 0x3F, 0x01);
		return MCP23017_ERR_FAIL;
	}

	ESP_LOGI(TAG, "Read first stage: %d", ret);

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (0x3F << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
	i2c_master_read_byte(cmd, data, 0);
	ret = i2c_master_cmd_begin(mcp->port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
    
	if (ret == ESP_FAIL) {
		ESP_LOGE(TAG, "ERROR: unable to read reg %02x from address %02x", 0x05, 0x3F);
		return MCP23017_ERR_FAIL;
	}

	ESP_LOGI(TAG, "Read second stage: %d", ret);

	return MCP23017_ERR_OK;
}
