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
    Serial.printf("%5.2f degC, %7.2 hPa\n", t, p);

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
    sprintf(buf, "%4.2f", t);
    widthText = M5.Display.textWidth(buf);
    M5.Display.setCursor(230 - widthText, 50);
    M5.Display.printf(buf);
  
    // Show values on LCD screen, pressure
    widthText = M5.Display.textWidth("8888.88");
    heightText = M5.Display.fontHeight();
    M5.Display.setTextColor(COLOR_7SEG_OFF, TFT_BLACK);
    M5.Display.setCursor(230 - widthText, 145);
    M5.Display.printf("8888.88");
    M5.Display.drawRect(230 - widthText - FRAME_MARGIN + FRAME_OFFSET_X, 143 - FRAME_MARGIN + FRAME_OFFSET_Y, widthText + FRAME_MARGIN * 2, heightText + FRAME_MARGIN * 2, COLOR_7SEG_FRAME);
    M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_ON);
    sprintf(buf, "%7.2f", p);
    widthText = M5.Display.textWidth(buf);
    M5.Display.setCursor(230 - widthText, 145);
    M5.Display.printf("%7.2f", p);
    M5.Display.endWrite();

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
 
    // Turn off measurement mark on display
    M5.Display.fillCircle(8, 8, 5, COLOR_7SEG_OFF);

    // Record to microSD card if flag is raised
    if (isRecording) {
      // Write to microSD
      file = SD.open(fileName, FILE_APPEND);
      file.printf("%6.3f, %7.3f\n", t, p);
      file.close();

      // Show message on LCD
      M5.Display.setFont(&fonts::Font2);
      M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_OFF);
      M5.Display.setCursor(20, 201);
      M5.Display.printf("Recording to ");
      M5.Display.printf(fileName);
      M5.Display.printf("...");
    }
    else {
      // Delete message on LCD
      M5.Display.setFont(&fonts::Font2);
      M5.Display.setTextColor(COLOR_7SEG_ON, COLOR_7SEG_OFF);
      char buf[] = "Recording to /R0000.CSV...";
      widthText = M5.Display.textWidth(buf);
      heightText = M5.Display.fontHeight();
      M5.Display.fillRect(20, 201, widthText, heightText, COLOR_7SEG_OFF);
    }

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
    Serial.println("Mounting microSD card...");
    delay(500);
  }

  // Set up I2C bus
  Wire.begin();
  
  // Set up LPS25H
  // pinMode(SDA, INPUT_PULLUP);
  // pinMode(SCL, INPUT_PULLUP);
  sensor.begin(Wire, LPS25HB_I2C_ADDR_DEF);
 
  delay(100);
  if (sensor.isConnected() == false)
  {
    Serial.println("LPS25H not found.");
    Serial.println("");
    while (true) {
      // Infinate loop
    }
  }

  // Set the offset value to the RPDS register to subtract approximately 7 hPa from the raw value
  uint8_t offsetPressure[2] = {0xa0, 0x00};
  sensor.write(LPS25HB_REG_RPDS_L, offsetPressure, 2);

  // Show labels on LCD screen, temperature
  M5.Display.setTextColor(COLOR_LABEL, TFT_BLACK);
  M5.Display.setFont(&fonts::Orbitron_Light_24);
  M5.Display.setCursor(20, 14);    
  M5.Display.printf("Temperature");
  M5.Display.setTextColor(COLOR_UNIT, TFT_BLACK);
  M5.Display.setCursor(250, 78);
  M5.Display.printf("C");
  M5.Display.drawCircle(247, 79, 3, COLOR_UNIT);

  // Show labels on LCD screen, pressure
  M5.Display.setTextColor(COLOR_LABEL, TFT_BLACK);
  M5.Display.setFont(&fonts::Orbitron_Light_24);
  M5.Display.setCursor(20, 109);
  M5.Display.printf("Pressure");
  M5.Display.setTextColor(COLOR_UNIT, TFT_BLACK);
  M5.Display.setCursor(250, 173);
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
    if (M5.BtnA.wasClicked()) {
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
      file.close();
    }

    delay(10);
}
