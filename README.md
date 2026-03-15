# STM32F411RE-Smart-Lamp-Post-RTOS
Smart lamp post system with FreeRTOS, 4 sensors, OLED display, and RTC on STM32F411RE
# 🚦 STM32F411RE Smart Lamp Post

> A real-time embedded environment monitoring system built with FreeRTOS on the STM32F411RE.



## Features
- 🌡️ Temperature & Humidity — DHT11
- 💨 Air Quality Index — MQ-135
- 🌧️ Rain Detection — YL-83
- 💡 Auto LED control — LDR (GPIO)
- 📟 3-screen rotating OLED dashboard — SSD1306 128×64
- 🕐 Real-time clock with persistent backup register — RTC

## Tech Stack
| Component | Detail |
|-----------|--------|
| MCU | STM32F411RE (Cortex-M4 @ 96 MHz) |
| RTOS | FreeRTOS CMSIS-V2 |
| Tasks | 4 concurrent (DHT, ADC, OLED, Default) |
| Sync | osMutex for thread-safe display |
| Peripherals | ADC, I2C, UART, TIM1, RTC |
| Display | SSD1306 over I2C @ 400kHz |
| Debug | UART @ 115200 bps |

## Block Diagram
![Block Diagram](block_diagram.png)

## Wiring
| Sensor | STM32 Pin |
|--------|-----------|
| DHT11 DATA | PA1 |
| MQ-135 AO | PA0 (ADC CH0) |
| YL-83 AO | PB0 (ADC CH8) |
| LDR DO | PA4 |
| InBuild - LED | PA5 |
| SSD1306 SDA | PB9 (I2C1) |
| SSD1306 SCL | PB8 (I2C1) |

## Project Structure
```
01_RTOS_FreeRTOS/   — Full FreeRTOS project (STM32CubeIDE)
02_BareMetal/       — Bare metal version (no RTOS)
03_IOC_Config/      — CubeMX .ioc configuration
04_Docs/            — Block diagram, wiring, report
```

## How to Flash
1. Open `01_RTOS_FreeRTOS/` in STM32CubeIDE
2. Build → Debug or Run
3. RTC sets automatically on first flash via backup register

## Key Design Decisions
- **RTC persistence**: Uses BKP_DR0 with magic key `0xCAFE` — time sets once and survives all power cycles
- **Thread safety**: OLED mutex prevents display corruption across tasks
- **Sensor validation**: DHT11 readings sanity-checked before accepting

## Institution
Nirma University | ECE Department | 2026
