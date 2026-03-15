#include "dht11.h"

extern TIM_HandleTypeDef htim1;

// 1 microsecond delay using TIM1 (prescaler=95, 96MHz → 1tick=1µs)
static void delay_us(uint32_t us)
{
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < us);
}

// Switch PA1 to OUTPUT mode
static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DHT11_PIN_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(DHT11_PIN_GPIO_Port, &GPIO_InitStruct);
}

// Switch PA1 to INPUT mode
static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = DHT11_PIN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PIN_GPIO_Port, &GPIO_InitStruct);
}

// Returns 1 if success, 0 if failed
uint8_t DHT11_Read(uint8_t *temp, uint8_t *hum)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint32_t timeout;

    // ---- START SIGNAL ----
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin, GPIO_PIN_RESET);
    HAL_Delay(18);   // Pull low min 18ms
    HAL_GPIO_WritePin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin, GPIO_PIN_SET);
    delay_us(30);    // Pull high 20-40us

    // ---- WAIT FOR SENSOR RESPONSE ----
    DHT11_SetInput();

    // Wait for DHT11 to pull low (response start)
    timeout = 0;
    while (HAL_GPIO_ReadPin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin) == GPIO_PIN_SET)
    {
        delay_us(1);
        if (++timeout > 100) return 0;  // No response
    }

    // Wait for DHT11 low pulse (~80us)
    timeout = 0;
    while (HAL_GPIO_ReadPin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin) == GPIO_PIN_RESET)
    {
        delay_us(1);
        if (++timeout > 100) return 0;
    }

    // Wait for DHT11 high pulse (~80us)
    timeout = 0;
    while (HAL_GPIO_ReadPin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin) == GPIO_PIN_SET)
    {
        delay_us(1);
        if (++timeout > 100) return 0;
    }

    // ---- READ 40 BITS (5 bytes) ----
    for (int i = 0; i < 5; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            // Wait for bit start (low pulse ~50us)
            timeout = 0;
            while (HAL_GPIO_ReadPin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin) == GPIO_PIN_RESET)
            {
                delay_us(1);
                if (++timeout > 75) return 0;
            }

            // Wait 40us then sample — if still HIGH = bit 1, if LOW = bit 0
            delay_us(40);

            if (HAL_GPIO_ReadPin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin) == GPIO_PIN_SET)
            {
                data[i] |= (1 << j);  // Bit is 1
                // Wait for high pulse to end
                timeout = 0;
                while (HAL_GPIO_ReadPin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin) == GPIO_PIN_SET)
                {
                    delay_us(1);
                    if (++timeout > 75) return 0;
                }
            }
        }
    }

    // ---- CHECKSUM VERIFY ----
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
        return 0;

    *hum  = data[0];
    *temp = data[2];
    return 1;
}
