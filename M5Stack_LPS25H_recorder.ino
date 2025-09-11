// ------------------------------------------------------------------------------------------
// M5Stack_LPS25H_recorder.ino - LPS25H temperature and pressure sensor for M5Stack
// Enabling recording functionality into a TF memory card
// (c) 2025 @RR_Inyo
// Released under the MIT License
// https://opensource.org/licenses/mit-license.php
// ------------------------------------------------------------------------------------------

#include <Wire.h>
#include <SD.h>
#include <M5Unified.h>
#include <SparkFun_LPS25HB_Arduino_Library.h>

// Define instant for LPS25HB sensor
LPS25HB sensor;

// Define file name container and recording flag
File file;
char fileName[16];
bool isRecording = false;
float t_rec = 0.0;

// Define constant
const auto COLOR_7SEG_OFF = M5.Display.color888(32, 32, 32);
const auto COLOR_7SEG_ON = M5.Display.color888(72, 255, 166);
const auto COLOR_7SEG_FRAME = M5.Display.color888(64, 64, 16);
const auto COLOR_LABEL = M5.Display.color888(128, 128, 144);
const auto COLOR_UNIT = M5.Display.color888(225, 48, 48);
const int FRAME_OFFSET_X = 0;
const int FRAME_OFFSET_Y = 2;
const int FRAME_MARGIN = 5;

// Define task handler
TaskHandle_t taskHandle_measure;

// ----------------------------------------
// The ISR
// ----------------------------------------
void IRAM_ATTR onTimer() {
  BaseType_t taskWoken;

  // Wake up the task
  xTaskNotifyFromISR(taskHandle_measure, 0, eIncrement, &taskWoken);
}

// ----------------------------------------
// The measure function
// ----------------------------------------
void measure(void *pvParameters) {
  // Define a variable to store notified value
  uint32_t ulNotifiedValue;

  while (true) {
    // Wait to be woken up
    xTaskNotifyWait(0, 0, &ulNotifiedValue, portMAX_DELAY);

    // Acquire LCD control
    M5.Display.startWrite();

    // Show measurement mark on display, become red if recording is ongoing
    if (isRecording) {
      M5.Display.fillCircle(8, 8, 5, TFT_RED);
    }
    else {
      M5.Display.fillCircle(8, 8, 5, COLOR_7SEG_ON);
    }
    
    // Obtain sensor data
   float t, p;
   if (sensor.isConnected() == false) {
      Serial.println("LPS25H not found.");
      Serial.println("");
      while (1) {
        // Infinate loop
      }
    }
    else {
      t = sensor.getTemperature_degC();
      p = sensor.getPressure_hPa();
    }
    
    // Send data to serial communication
    Serial.printf("%5.2f degC, %7.2f hPa", t, p);
    if (isRecording) {
      Serial.printf(", %6.1f sec", t_rec);
    }
    Serial.printf("\n");

    // Define buffer to store values
    char buf[16];
    int widthText, heightText;

    // Show values on LCD screen, temperature
    M5.Display.setFont(&fonts::Font7);
    widthText = M5.Display.textWidth("8888.88");
    heightText = M5.Display.fontHeight();
    M5.Display.setTextColor(COLOR_7SEG_OFF, TFT_BLACK);
    M5.Display.setCursor(230 - widthText, 50);
    M5.Display.printf("8888.88");
    M5.Display.drawRect(230 - widthText - FRAME_MARGIN + FRAME_OFFSET_X, 48 - FRAME_MARGIN + FRAME_OFFSET_Y, widthText + FRAME_MARGIN * 2, heightText + FRAME_MARGIN * 2, COLOR_7SEG_FRAME);
    M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_ON);
    sprintf(buf, "%5.2f", t);
    widthText = M5.Display.textWidth(buf);
    M5.Display.setCursor(230 - widthText, 50);
    M5.Display.printf(buf);
  
    // Show values on LCD screen, pressure
    widthText = M5.Display.textWidth("8888.88");
    heightText = M5.Display.fontHeight();
    M5.Display.setTextColor(COLOR_7SEG_OFF, TFT_BLACK);
    M5.Display.setCursor(230 - widthText, 142);
    M5.Display.printf("8888.88");
    M5.Display.drawRect(230 - widthText - FRAME_MARGIN + FRAME_OFFSET_X, 140 - FRAME_MARGIN + FRAME_OFFSET_Y, widthText + FRAME_MARGIN * 2, heightText + FRAME_MARGIN * 2, COLOR_7SEG_FRAME);
    M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_ON);
    sprintf(buf, "%7.2f", p);
    widthText = M5.Display.textWidth(buf);
    M5.Display.setCursor(230 - widthText, 142);
    M5.Display.printf(buf);

    // Show battery level on LCD screen
    int battLevel = M5.Power.getBatteryLevel();
    int numBattBlocks;
    auto colorBattBlocks = TFT_DARKGREY;
    if (battLevel > 75 && battLevel <= 100) {
      numBattBlocks = 4;
      colorBattBlocks = TFT_LIGHTGREY;
    }
    else if (battLevel > 50 && battLevel <= 75) {
      numBattBlocks = 3;
      colorBattBlocks = TFT_LIGHTGREY;
    }
    else if (battLevel > 25 && battLevel <= 50) {
      numBattBlocks = 2;
      colorBattBlocks = TFT_ORANGE;
    }
    else if (battLevel > 0 && battLevel <= 25) {
      numBattBlocks = 1;
      colorBattBlocks = TFT_RED;
    }

    // Show battery charging if connected
    if (M5.Power.isCharging()) {
      // Show power plug icon
      M5.Display.fillRect(235, 9, 5, 2, COLOR_7SEG_ON);       // Cable
      M5.Display.fillCircle(244, 9, 4, COLOR_7SEG_ON);        // Plug, round part at back
      M5.Display.fillRect(244, 5, 7, 10, COLOR_7SEG_ON);      // Plug, square part at front
      M5.Display.fillRect(251, 7, 4, 2, COLOR_7SEG_ON);       // Blade, upper
      M5.Display.fillRect(251, 11, 4, 2, COLOR_7SEG_ON);      // Blade, lower
    }
    else {
      M5.Display.fillRect(235, 4, 21, 21, TFT_BLACK);
    }

    // Show battery percentage
    M5.Display.fillRect(300, 4, 15, 12, COLOR_7SEG_OFF);
    M5.Display.drawRect(300, 4, 15, 12, TFT_DARKGREY);
    for (int i = 0; i < numBattBlocks; i++) {
      M5.Display.fillRect(302 + i * 3, 6, 2, 8, colorBattBlocks);
    }

    // Record to microSD card if flag is raised
    if (isRecording) {    
      // Release LCD control to open file
      M5.Display.endWrite();

      // Write to microSD
      file = SD.open(fileName, FILE_APPEND);
      if (t_rec == 0.0) {
        file.printf("Time [s], Temperature [degC], Pressure [hPa]\n");
      }
      file.printf("%6.1f, %6.3f, %8.3f\n", t_rec, t, p);
      file.close();

      // Acquire LCD control again after closing file
      M5.Display.startWrite();

      // Show message on LCD
      M5.Display.setFont(&fonts::Font2);
      M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_OFF);
      M5.Display.setCursor(20, 201);
      M5.Display.printf("Recording to ");
      M5.Display.printf(fileName);
      M5.Display.printf(", %6.1f sec...", t_rec);

      // Increment recording time
      t_rec += 0.5;
    }
    else {
      // Delete message on LCD
      M5.Display.setFont(&fonts::Font2);
      M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_OFF);
      char buf[] = "Recording to /R0000.CSV, 00000.0 sec...";
      widthText = M5.Display.textWidth(buf);
      heightText = M5.Display.fontHeight();
      M5.Display.fillRect(20, 201, widthText, heightText, COLOR_7SEG_OFF);
    }

    // Clear measurement mark on display
    M5.Display.fillCircle(8, 8, 5, COLOR_7SEG_OFF);

    // Release LCD control
    M5.Display.endWrite();
  }
}

// ----------------------------------------
// The setup function
// ----------------------------------------
void setup() {
  // Initialize M5Stack
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Power.begin();
  M5.In_I2C.release();  // This is very important to allow Wire to take control of I2C over M5Unified!!
  M5.Display.setBrightness(128);

  // Set up serial communication
  Serial.begin(115200);
  Serial.println("------------------------------------------------------------");
  Serial.println("M5Stack + LPS25H air pressure and temperature meter");
  Serial.println("------------------------------------------------------------");

  // Initialize microSD card
  while (!SD.begin(GPIO_NUM_4, SPI, 15000000)) {
    // Show message to serial communication
    Serial.println("microSD card not found.");
    Serial.println("");

    // Show message on LCD screen
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_OFF);
    M5.Display.setCursor(10, 10);
    M5.Display.printf("microSD card not found.");

    // Wait for a while for microSD to be mounted
    delay(500);
  }

  // Set up I2C bus
  Wire.begin();
  
  // Begin LPS25H sensor
  sensor.begin(Wire, LPS25HB_I2C_ADDR_DEF);
  
  // Confirm LPS25H connection
  while (true) {
    if (!sensor.isConnected()) {
      // Show message to serial communication
      Serial.println("LPS25H not found.");
      Serial.println("");

      // Show message to LCD screen
      M5.Display.setFont(&fonts::Font4);
      M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_OFF);
      M5.Display.setCursor(10, 50);
      M5.Display.printf("LPS25H not found.");

      // Wait for a while for LPS25H to be connected
      delay(500);
      continue;
    }
    else {
      // Begin sensor again, just in case...
      sensor.begin(Wire, LPS25HB_I2C_ADDR_DEF);
      break;
    }
  }

  // Set the offset value to the RPDS register to subtract approximately 7 hPa from the raw value
  uint8_t offsetPressure[2] = {0xa0, 0x00};
  sensor.write(LPS25HB_REG_RPDS_L, offsetPressure, 2);

  // Clear display first
  M5.Display.clearDisplay();

  // Acquire LCD control
  M5.Display.startWrite();

  // Show labels on LCD screen, temperature
  M5.Display.setTextColor(COLOR_LABEL, TFT_BLACK);
  M5.Display.setFont(&fonts::Orbitron_Light_24);
  M5.Display.setCursor(20, 14);    
  M5.Display.printf("Temperature");
  M5.Display.setTextColor(COLOR_UNIT, TFT_BLACK);
  M5.Display.setCursor(250, 74);
  M5.Display.printf("C");
  M5.Display.drawCircle(247, 75, 3, COLOR_UNIT);

  // Show labels on LCD screen, pressure
  M5.Display.setTextColor(COLOR_LABEL, TFT_BLACK);
  M5.Display.setFont(&fonts::Orbitron_Light_24);
  M5.Display.setCursor(20, 106);
  M5.Display.printf("Pressure");
  M5.Display.setTextColor(COLOR_UNIT, TFT_BLACK);
  M5.Display.setCursor(250, 166);
  M5.Display.printf("hPa");

  // Show function name above Button A and B on LCD screen
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_OFF);
  M5.Display.setCursor(46, 220);
  M5.Display.printf("Record");
  M5.Display.setCursor(145, 220);
  M5.Display.printf("Stop");

  // Show battery "BATT." on LCD screen
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(COLOR_7SEG_ON, TFT_BLACK);
  M5.Display.setCursor(261, 2);
  M5.Display.printf("BATT.");

  // Release LCD control
  M5.Display.endWrite();

  // Create a task combined with a timer
  xTaskCreateUniversal(
    measure,              // Function as a task
    "measure",            // Name of task
    8192,                 // Stack memory sie
    NULL,                 // Parameters
    5,                   // Priority
    &taskHandle_measure,  // Task handler
    PRO_CPU_NUM           // Core to execute this task
  );

  // Set timer interrption, Timer at 1 MHz (1 microsec), Interruption for 500000 cycles (0.5 sec)
  hw_timer_t *timer = NULL;
  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 500000, true, 0);
}

// ----------------------------------------
// The loop function
// ----------------------------------------
void loop() {
    // Observe button status to raise of lower flag for recording
    M5.update();
    if (M5.BtnA.wasClicked() && !isRecording) {
      isRecording = true;
      for (int i = 0; i < 1000; i++) {
        sprintf(fileName, "/R%04d.CSV", i);
        if(SD.exists(fileName)) {
          continue;
        }
        else {
          break;
        }
      }
    }
    else if (M5.BtnB.wasClicked()) {
      isRecording = false;
      t_rec = 0.0;
    }

    delay(10);
}
