#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

// THIS LINE FIXES THE FAMILY DETECTION ERROR
#define STM32F4

// Choose interface — we use I2C
#define SSD1306_USE_I2C

// OLED size
#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

// I2C port
#define SSD1306_I2C_PORT   hi2c1

// I2C address
#define SSD1306_I2C_ADDR   (0x3C << 1)

// Fonts
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18

#endif /* __SSD1306_CONF_H__ */
