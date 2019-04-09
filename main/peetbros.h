#define VERSION "Wind v3 18-Sep-2015"

#define windSpeedPin 3
#define windDirPin 2

#define LOW  0
#define HIGH 1

#define ESP_INTR_FLAG_DEFAULT 0

// Pin 13 has an LED connected on most Arduino boards.
#define LED = 13;

extern const float filterGain;

void loop();
void readWindSpeed(void* data);
void readWindDir(void* data);

#define interrupts() \
    gpio_intr_enable(windSpeedPin); \
    gpio_intr_enable(windDirPin);

#define noInterrupts() \
    gpio_intr_disable(windSpeedPin); \
    gpio_intr_disable(windDirPin);
