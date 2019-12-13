/*
  Wind - NMEA Wind Instrument
  Copyright (c) 2018 Tom K
  Originally written for Arduino Pro Mini 328
  v3c

  MIT License

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <rom/ets_sys.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_now.h"

#include "peetbros.h"

#define println(s) printf("%s\n", s);

const uint64_t DEBOUNCE = 10000ul;      // Minimum switch time in microseconds
const uint64_t DIRECTION_OFFSET = 0ul;  // Manual direction offset in degrees, if required
const uint64_t TIMEOUT = 1500000ul;       // Maximum time allowed between speed pulses in microseconds
const uint64_t UPDATE_RATE = 500ul;     // How often to send out NMEA data in milliseconds
const float filterGain = 0.25;               // Filter gain on direction output filter. Range: 0.0 to 1.0
                                             // 1.0 means no filtering. A smaller number increases the filtering

// Knots is actually stored as (Knots * 100). Deviations below should match these units.
const int BAND_0 =  10 * 100;
const int BAND_1 =  80 * 100;

const int SPEED_DEV_LIMIT_0 =  5 * 100;     // Deviation from last measurement to be valid. Band_0: 0 to 10 knots
const int SPEED_DEV_LIMIT_1 = 10 * 100;     // Deviation from last measurement to be valid. Band_1: 10 to 80 knots
const int SPEED_DEV_LIMIT_2 = 30 * 100;     // Deviation from last measurement to be valid. Band_2: 80+ knots

// Should be larger limits as lower speed, as the direction can change more per speed update
const int DIR_DEV_LIMIT_0 = 25;     // Deviation from last measurement to be valid. Band_0: 0 to 10 knots
const int DIR_DEV_LIMIT_1 = 18;     // Deviation from last measurement to be valid. Band_1: 10 to 80 knots
const int DIR_DEV_LIMIT_2 = 10;     // Deviation from last measurement to be valid. Band_2: 80+ knots

volatile uint64_t speedPulse = 0ul;    // Time capture of speed pulse
volatile uint64_t dirPulse = 0ul;      // Time capture of direction pulse
volatile uint64_t speedTime = 0ul;     // Time between speed pulses (microseconds)
volatile uint64_t directionTime = 0ul; // Time between direction pulses (microseconds)
volatile bool newData = false;           // New speed pulse received
volatile uint64_t lastUpdate = 0ul;    // Time of last serial output

volatile int knotsOut = 0;    // Wind speed output in knots * 100
volatile int dirOut = 0;      // Direction output in degrees
volatile bool ignoreNextReading = false;

bool debug = false;

#define INT_MASK_SPD 1
#define INT_MASK_DIR 2
volatile unsigned int speedTrigger = 0;
 
void readWindSpeed(void* data)
{
    // Despite the interrupt being set to FALLING edge, double check the pin is now LOW
    if (((esp_timer_get_time() - speedPulse) > DEBOUNCE) && (gpio_get_level(windSpeedPin) == LOW))
    {
        // Work out time difference between last pulse and now
        speedTime = esp_timer_get_time() - speedPulse;

        // Direction pulse should have occured after the last speed pulse
        if (dirPulse >= speedPulse) directionTime = dirPulse - speedPulse;

        newData = true;
        speedPulse = esp_timer_get_time();    // Capture time of the new speed pulse
    }
    speedTrigger |= INT_MASK_SPD;
}

void readWindDir(void* data)
{
    if (((esp_timer_get_time() - dirPulse) > DEBOUNCE) && (gpio_get_level(windDirPin) == LOW))
    {
      dirPulse = esp_timer_get_time();        // Capture time of direction pulse
    }
    speedTrigger |= INT_MASK_DIR;
}

uint8_t getChecksum(char* str, size_t len)
{
    uint8_t cs = 0;
    for (unsigned int n = 1; n < len - 1; n++)
    {
        cs ^= str[n];
    }
    return cs;
}

/*=== MWV - Wind Speed and Angle ===
 *
 * ------------------------------------------------------------------------------
 *         1   2 3   4 5
 *         |   | |   | |
 *  $--MWV,x.x,a,x.x,a*hh<CR><LF>
 * ------------------------------------------------------------------------------
 *
 * Field Number:
 *
 * 1. Wind Angle, 0 to 360 degrees
 * 2. Reference, R = Relative, T = True
 * 3. Wind Speed
 * 4. Wind Speed Units, K/M/N
 * 5. Status, A = Data Valid
 * 6. Checksum
 *
 */
void printWindNmea()
{
    float spd = knotsOut / 100.0;
    uint8_t cs;
    size_t len;

    //Assemble a sentence of the various parts so that we can calculate the proper checksum
    memset(windSentence, 0, maxDataFrameSize);
    sprintf(windSentence, "$WIMWV,%d.0,R,%.1f,N,A*", dirOut, spd);
    //calculate the checksum
    len = strlen(windSentence);
    cs = getChecksum(windSentence, len);

    sprintf(windSentence + len,"%0X\r\n", cs); // Assemble the final message and send it out the serial port
    
    // output to console
    esp_now_send(slave.peer_addr, (uint8_t *)windSentence, strlen(windSentence)+2);
    // printf(windSentence);
}

bool checkDirDev(int64_t knots, int dev)
{
    if (knots < BAND_0)
    {
        if ((abs(dev) < DIR_DEV_LIMIT_0) || (abs(dev) > 360 - DIR_DEV_LIMIT_0)) return true;
    }
    else if (knots < BAND_1)
    {
        if ((abs(dev) < DIR_DEV_LIMIT_1) || (abs(dev) > 360 - DIR_DEV_LIMIT_1)) return true;
    }
    else
    {
        if ((abs(dev) < DIR_DEV_LIMIT_2) || (abs(dev) > 360 - DIR_DEV_LIMIT_2)) return true;
    }
    return false;
}

bool checkSpeedDev(int64_t knots, int dev)
{
    if (knots < BAND_0)
    {
        if (abs(dev) < SPEED_DEV_LIMIT_0) return true;
    }
    else if (knots < BAND_1)
    {
        if (abs(dev) < SPEED_DEV_LIMIT_1) return true;
    }
    else
    {
        if (abs(dev) < SPEED_DEV_LIMIT_2) return true;
    }
    return false;
}

void calcWindSpeedAndDir()
{
    // uint64_t dirPulse_;
    uint64_t speedPulse_;
    uint64_t speedTime_;
    uint64_t directionTime_;
    int64_t windDirection = 0l, rps = 0l, knots = 0l;

    static int prevKnots = 0;
    static int prevDir = 0;
    int dev = 0;

    // Get snapshot of data into local variables. Note: an interrupt could trigger here
    noInterrupts();
    // dirPulse_ = dirPulse;
    speedPulse_ = speedPulse;
    speedTime_ = speedTime;
    directionTime_ = directionTime;
    interrupts();

    // Make speed zero, if the pulse delay is too int64_t
    if (esp_timer_get_time() - speedPulse_ > TIMEOUT) speedTime_ = 0ul;

    // The following converts revolutions per 100 seconds (rps) to knots x 100
    // This calculation follows the Peet Bros. piecemeal calibration data
    if (speedTime_ > 0)
    {
        rps = 100000000/speedTime_;                  //revolutions per 100s

        if (rps < 323)
        {
          knots = (rps * rps * -11)/11507 + (293 * rps)/115 - 12;
        }
        else if (rps < 5436)
        {
          knots = (rps * rps / 2)/11507 + (220 * rps)/115 + 96;
        }
        else
        {
          knots = (rps * rps * 11)/11507 - (957 * rps)/115 + 28664;
        }
        //knots = mph * 0.86897

        if (knots < 0l) knots = 0l;  // Remove the possibility of negative speed
        // Find deviation from previous value
        dev = (int)knots - prevKnots;

        // Only update output if in deviation limit
        if (checkSpeedDev(knots, dev))
        {
          knotsOut = knots;

          // If speed data is ok, then continue with direction data
          if (directionTime_ > speedTime_)
          {
              windDirection = 999;    // For debugging only (not output to knots)
          }
          else
          {
            // Calculate direction from captured pulse times
            windDirection = (((directionTime_ * 360) / speedTime_) + DIRECTION_OFFSET) % 360;

            // Find deviation from previous value
            dev = (int)windDirection - prevDir;

            // Check deviation is in range
            if (checkDirDev(knots, dev))
            {
              int delta = ((int)windDirection - dirOut);
              if (delta < -180)
              {
                delta = delta + 360;    // Take the shortest path when filtering
              }
              else if (delta > +180)
              {
                delta = delta - 360;
              }
              // Perform filtering to smooth the direction output
              dirOut = (dirOut + (int)(round(filterGain * delta))) % 360;
              if (dirOut < 0) dirOut = dirOut + 360;
            }
            prevDir = windDirection;
          }
        }
        else
        {
          ignoreNextReading = true;
        }

        prevKnots = knots;    // Update, even if outside deviation limit, cause it might be valid!?
    }
    else
    {
        knotsOut = 0;
        prevKnots = 0;
    }

    if (debug)
    {
        printf("%lld", (esp_timer_get_time() / 1000));
        printf(",");
        printf("%d", dirOut);
        printf(",");
        printf("%lld", windDirection);
        printf(",");
        printf("%d\n", knotsOut/100);
        //Serial.print(",");
        //Serial.print(knots/100);
        //Serial.print(",");
        //Serial.println(rps);
    }
    else
    {
      if ((esp_timer_get_time() / 1000) - lastUpdate > UPDATE_RATE)
      {
        printWindNmea();
        lastUpdate = (esp_timer_get_time() / 1000);
      }
    }
}

void debugInterrupt(void) {
  if (!debug) return;
  if (speedTrigger & INT_MASK_SPD) {
    printf("[INT] WIND\n");
  }
  if (speedTrigger & INT_MASK_DIR) {
    printf("[INT] DIR\n");
  }
  speedTrigger = 0;
}

void loop(void *pvParameters)
{
    for(;;) {
        int i;
        const unsigned int LOOP_DELAY = 50;
        const unsigned int LOOP_TIME = TIMEOUT / LOOP_DELAY;

        //   digitalWrite(LED, !digitalRead(LED));    // Toggle LED

        i = 0;
        // If there is new data, process it, otherwise wait for LOOP_TIME to pass
        while ((newData != true) && (i < LOOP_TIME))
        {
            i++;
            ets_delay_us(LOOP_DELAY);
        }

        calcWindSpeedAndDir();    // Process new data
        newData = false;
        debugInterrupt();
        vTaskDelay(10);
    }
    vTaskDelete(NULL);
}
