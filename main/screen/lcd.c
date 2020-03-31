extern i2c_lcd1602_info_t* lcd_info;
/**
Writes lines at the current position of the cursor.
 @param string[] the text that is written
*/
void writeLineCurrentPosition(const char string[]) {
    i2c_lcd1602_write_string(lcd_info, string);
}
/**
Writes lines at the start of the screen.
 @param string[] the text that is written
*/
void writeLineFromStart(const char string[]) {
    i2c_lcd1602_home(lcd_info);
    i2c_lcd1602_write_string(lcd_info, string);
}
/**
Clears the LCD screen completely.
*/
void clearScreen() {
    i2c_lcd1602_clear(lcd_info);
}
/**
Moves the cursor to a postion that is determined by the params
 @param collumn the collumn the cursor is moved towards.
 @param line the line the cursor is moved towards
*/
void moveCursor(int collumn, int line) {
    if (collumn > 19) {
        collumn = 19;
    }
    if (line > 3) {
        line = 3;
    }
    i2c_lcd1602_move_cursor(lcd_info, collumn, line);
}
/**
Moves the cursor to a postion that is determined by the params and writes text there.
 @param collumn the collumn the cursor is moved towards.
 @param line the line the cursor is moved towards.
 @param string[] the text that is written at the position of the cursor
*/
void writeLineOnPosition(int collumn, int line, const char string[]) {
    moveCursor(collumn, line);
    writeLineCurrentPosition(string);
}
/**
Writes a character on the LCD
 @param chare The char written
*/
void writeChar(const char chare) {
    i2c_lcd1602_write_char(lcd_info, chare);
}
/**
Moves the cursor to a postion that is determined by the params and writes a char there.
 @param collumn the collumn the cursor is moved towards.
 @param line the line the cursor is moved towards.
 @param char the char that is written at the position of the cursor
*/
void writeCharOnPosition(int collumn, int line, const char chare) {
    moveCursor(collumn, line);
    i2c_lcd1602_write_char(lcd_info, chare);
}
/**
Turns on the Blinker function on the cursor
*/
void turnBlinkerOn() {
	i2c_lcd1602_set_blink(lcd_info, true);
}
/**
Turns off the Blinker function on the cursor
*/
void turnBlinkerOff() {
	i2c_lcd1602_set_blink(lcd_info, false);
}
