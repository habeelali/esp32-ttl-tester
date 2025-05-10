#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "esp_system.h"
#include "esp_spi_flash.h"

#include <WiFi.h>
#include <WebServer.h>

const char *ssid = "esp32-ttl";
const char *password = "12345678";
WebServer server(80);

IPAddress local_IP(13, 37, 4, 20);
IPAddress gateway(13, 37, 4, 20);
IPAddress subnet(255, 255, 255, 0);

void handleRoot()
{
  String page = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Edit chips.dat</title>
      <style>
        body { font-family: Arial; margin: 2em; background: #f4f4f4; }
        textarea { width: 100%; max-width: 800px; height: 400px; font-family: monospace; }
        input[type="submit"] {
          padding: 0.5em 1em;
          font-size: 1em;
          background: #4CAF50;
          color: white;
          border: none;
          border-radius: 4px;
          cursor: pointer;
        }
        input[type="submit"]:hover {
          background: #45a049;
        }
      </style>
    </head>
    <body>
      <h2>Edit chips.dat</h2>
      <form method='POST' action='/save'>
        <textarea name='contents'>)rawliteral";

  if (SD.exists("/chips.dat"))
  {
    File file = SD.open("/chips.dat", FILE_READ);
    if (file)
    {
      while (file.available())
      {
        char c = file.read();
        if (c == '&')
          page += "&amp;";
        else if (c == '<')
          page += "&lt;";
        else if (c == '>')
          page += "&gt;";
        else if (c == '"')
          page += "&quot;";
        else
          page += c;
      }
      file.close();
    }
  }

  page += R"rawliteral(</textarea><br><br>
        <input type='submit' value='Save'>
      </form>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", page);
}

void handleSave()
{
  if (!server.hasArg("contents"))
  {
    server.send(400, "text/plain", "Bad Request: Missing contents");
    return;
  }

  String newData = server.arg("contents");

  if (SD.exists("/chips.dat"))
  {
    SD.remove("/chips.dat");
  }

  File file = SD.open("/chips.dat", FILE_WRITE);
  if (!file)
  {
    server.send(500, "text/plain", "Failed to open file for writing");
    return;
  }

  file.print(newData);
  file.close();

  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

#define SDA_PIN 21
#define SCL_PIN 22
#define SD_CS 2
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define I2C_ADDRESS 0x3c
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BTN_BACK 16
#define BTN_ENTER 17
#define BTN_SCROLL 5

#define MAX_ICS 10
#define MAX_TESTS 20
#define MAX_LINES 50
#define I2C_RETRIES 3
#define I2C_DELAY 50

struct IC
{
  String name;
  int pins;
  int inputPins[16];
  int outputPins[16];
  String tests[MAX_TESTS];
  int inputCount;
  int outputCount;
  int testCount;
};

IC ics[MAX_ICS];
int icCount = 0;
const char *menuItems[] = {"Manual Mode", "Auto Mode", "System Info"};
int currentSelection = 0;
bool inSubmenu = false, viewingChips = false;
int chipViewIndex = 0;
String chipLines[MAX_LINES];
int chipLineCount = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

bool getExpanderForPin(int pin, int pinCount, uint8_t &address, uint8_t &port)
{
  if (pinCount == 14)
  {

    if (pin <= 6)
    {
      address = 0x20;
      port = 6 - (pin - 1);
      return true;
    }
    if (pin >= 8 && pin <= 13)
    {
      address = 0x21;
      port = pin - 8;
      return true;
    }
    return false;
  }
  else if (pinCount == 16)
  {

    if (pin >= 1 && pin <= 7)
    {
      address = 0x20;
      if (pin <= 6)
      {
        port = 6 - (pin - 1);
      }
      else
      {
        port = 0;
      }
      return true;
    }
    if (pin >= 9 && pin <= 14)
    {
      address = 0x21;
      port = pin - 9;
      return true;
    }
    if (pin == 15)
    {
      address = 0x21;
      port = 6;
      return true;
    }
    return false;
  }
  return false;
}
bool writeI2C(uint8_t address, uint8_t data)
{
  for (int i = 0; i < I2C_RETRIES; i++)
  {
    Wire.beginTransmission(address);
    Wire.write(data);
    if (Wire.endTransmission() == 0)
    {
      Serial.printf("[I2C] Write 0x%02X: 0x%02X\n", address, data);
      return true;
    }
    delay(I2C_DELAY);
  }
  Serial.printf("[ERROR] I2C Write Failed: 0x%02X\n", address);
  return false;
}

bool readI2C(uint8_t address, uint8_t *data)
{
  for (int i = 0; i < I2C_RETRIES; i++)
  {
    Wire.requestFrom(address, 1);
    if (Wire.available())
    {
      *data = Wire.read();
      Serial.printf("[I2C] Read 0x%02X: 0x%02X\n", address, *data);
      return true;
    }
    delay(I2C_DELAY);
  }
  Serial.printf("[ERROR] I2C Read Failed: 0x%02X\n", address);
  return false;
}

void runTest(IC ic)
{
  Serial.printf("\n[TEST] Starting %s (%d-pin)\n", ic.name.c_str(), ic.pins);
  if (ic.pins != 14 && ic.pins != 16)
  {
    Serial.println("[ERROR] Only 14/16-pin ICs supported!");
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("Testing %s", ic.name.c_str());
  display.display();

  uint8_t dummy;
  if (!readI2C(0x20, &dummy) || !readI2C(0x21, &dummy))
  {
    Serial.println("[ERROR] I2C Not Responding!");
    display.println("I2C Error!");
    display.display();
    delay(2000);
    return;
  }

  for (int t = 0; t < ic.testCount; t++)
  {
    String test = ic.tests[t];
    int colon = test.indexOf(':');
    if (colon == -1)
    {
      Serial.printf("[ERROR] Invalid test: %s\n", test.c_str());
      continue;
    }

    String inputStr = test.substring(0, colon);
    String expectedStr = test.substring(colon + 1);

    if (inputStr.length() != ic.inputCount)
    {
      Serial.printf("[ERROR] Input length %d != declared %d\n",
                    inputStr.length(), ic.inputCount);
      continue;
    }
    if (expectedStr.length() != ic.outputCount)
    {
      Serial.printf("[ERROR] Output length %d != declared %d\n",
                    expectedStr.length(), ic.outputCount);
      continue;
    }

    uint8_t exp20 = 0xFF;
    uint8_t exp21 = 0xFF;

    for (int i = 0; i < ic.inputCount; i++)
    {
      char state = inputStr[i];
      uint8_t addr, port;
      int pin = ic.inputPins[i];

      if (getExpanderForPin(pin, ic.pins, addr, port))
      {
        if (state == '0')
        {
          if (addr == 0x20)
            exp20 &= ~(1 << port);
          else
            exp21 &= ~(1 << port);
        }
      }
    }

    if (!writeI2C(0x20, exp20) || !writeI2C(0x21, exp21))
      return;
    delay(50);

    uint8_t read20, read21;
    if (!readI2C(0x20, &read20) || !readI2C(0x21, &read21))
      return;

    bool testPassed = true;
    String actualStr = "";

    for (int o = 0; o < ic.outputCount; o++)
    {
      uint8_t addr, port;
      int pin = ic.outputPins[o];

      if (getExpanderForPin(pin, ic.pins, addr, port))
      {
        char expectedChar = expectedStr[o];
        bool actual = (addr == 0x20) ? (read20 & (1 << port)) : (read21 & (1 << port));
        actualStr += actual ? '1' : '0';

        if (expectedChar == '2')
          continue;
        else if (expectedChar == '0' || expectedChar == '1')
        {
          bool expected = (expectedChar == '1');
          if (expected != actual)
            testPassed = false;
        }
        else
        {
          Serial.printf("[ERROR] Invalid expected '%c' at %d\n", expectedChar, o);
          testPassed = false;
        }
      }
    }

    if (!testPassed)
    {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextColor(SH110X_WHITE);
      display.setTextSize(2);
      display.println("  Failed ");
      display.setTextSize(1);
      display.println("---------------------");

      display.printf("Vec:%d/%d\n", t + 1, ic.testCount);
      display.printf("Exp:%s\n", expectedStr.c_str());
      display.printf("Got:%s\n", actualStr.c_str());
      display.display();
      delay(3000);
      return;
    }
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.println("  Passed ");
  display.setTextSize(1);
  display.println("---------------------");

  display.println("  All tests passed!");
  display.display();
  delay(2000);
}

void parseICs()
{
  icCount = 0;
  File file = SD.open("/chips.dat");
  if (!file)
  {
    Serial.println("[ERROR] chips.dat not found");
    return;
  }

  Serial.println("\n[SD] Parsing IC Database...");
  IC currentIC = {"", 0, {0}, {0}, {}, 0, 0, 0};

  String line;

  while (file.available() && icCount < MAX_ICS)
  {
    line = file.readStringUntil('\n');
    line.trim();

    if (line.startsWith("[IC]"))
    {
      if (currentIC.name != "")
      {
        if ((currentIC.pins == 14 || currentIC.pins == 16) && currentIC.testCount > 0)
        {
          ics[icCount++] = currentIC;
          Serial.printf("[SD] Loaded %s\n", currentIC.name.c_str());
        }
        else
        {
          Serial.println("[SD] Invalid IC skipped");
        }
      }
      currentIC = {"", 0, {}, {}, {}, 0, 0, 0};
    }
    else if (line.startsWith("Name="))
      currentIC.name = line.substring(5);
    else if (line.startsWith("Pins="))
      currentIC.pins = line.substring(5).toInt();
    else if (line.startsWith("Inputs="))
    {
      String inputs = line.substring(7);
      inputs.replace(" ", "");
      int idx = 0;

      while (inputs.length() > 0 && idx < 16)
      {
        int comma = inputs.indexOf(',');
        if (comma == -1)
          comma = inputs.length();
        currentIC.inputPins[idx++] = inputs.substring(0, comma).toInt();
        inputs = inputs.substring(comma + 1);
      }
      currentIC.inputCount = idx;
    }
    else if (line.startsWith("Outputs="))
    {
      String outputs = line.substring(8);
      outputs.replace(" ", "");
      int idx = 0;

      while (outputs.length() > 0 && idx < 16)
      {
        int comma = outputs.indexOf(',');
        if (comma == -1)
          comma = outputs.length();
        currentIC.outputPins[idx++] = outputs.substring(0, comma).toInt();
        outputs = outputs.substring(comma + 1);
      }
      currentIC.outputCount = idx;
    }
    else if (line.startsWith("Tests="))
    {
      String tests = line.substring(6);
      tests.replace(" ", "");
      int idx = 0;
      while (tests.length() > 0 && idx < MAX_TESTS)
      {
        int semicolon = tests.indexOf(';');
        if (semicolon == -1)
          semicolon = tests.length();
        String test = tests.substring(0, semicolon);

        if (test.indexOf(':') != -1)
        {
          currentIC.tests[idx++] = test;
        }
        else
        {
          Serial.printf("[SD] Invalid test: %s\n", test.c_str());
        }
        tests = tests.substring(semicolon + 1);
      }
      currentIC.testCount = idx;
    }
  }

  if (currentIC.name != "")
  {
    ics[icCount++] = currentIC;
    Serial.printf("[SD] Loaded %s\n", currentIC.name.c_str());
  }
  file.close();
  Serial.printf("[SD] Total ICs: %d\n", icCount);
}

void showChipLines()
{
  display.clearDisplay();
  display.setTextSize(1);

  for (int i = 0; i < 5; i++)
  {
    int idx = (chipViewIndex + i) % icCount;
    display.setCursor(0, i * 12);
    if (i == 0)
    {
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
      display.print("> ");
    }
    else
      display.setTextColor(SH110X_WHITE);
    display.println(ics[idx].name);
  }
  display.display();
}

void showMenu()
{

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);
  display.println(" Main Menu");
  display.setTextSize(1);
  display.println("---------------------");

  for (int i = 0; i < 3; i++)
  {
    if (i == currentSelection)
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    else
      display.setTextColor(SH110X_WHITE);
    display.println(menuItems[i]);
  }
  display.display();
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("\n[SYSTEM] IC Tester Initializing...");

  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_SCROLL, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(I2C_ADDRESS, true))
  {
    Serial.println("[ERROR] Display initialization failed!");
    while (1)
      ;
  }
  display.clearDisplay();
  display.display();
  Serial.println("[SUCCESS] OLED Ready");

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS))
  {
    display.clearDisplay();
    display.println("SD Card Error!");
    display.display();
    Serial.println("[ERROR] SD Card Mount Failed");
    while (1)
      ;
  }
  Serial.println("[SUCCESS] SD Card Ready");

  WiFi.softAPConfig(local_IP, gateway, subnet);

  WiFi.softAP(ssid, password);

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP started, IP=%s\n", ip.toString().c_str());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("HTTP server started");
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(" IC Tester");
  display.display();
  delay(2000);
  showMenu();
}

bool testIC(IC ic)
{
  if (ic.pins != 14 && ic.pins != 16)
    return false;

  uint8_t dummy;
  if (!readI2C(0x20, &dummy) || !readI2C(0x21, &dummy))
    return false;

  for (int t = 0; t < ic.testCount; t++)
  {
    String test = ic.tests[t];
    int colon = test.indexOf(':');
    if (colon == -1)
      continue;

    String inputStr = test.substring(0, colon);
    String expectedStr = test.substring(colon + 1);

    if (inputStr.length() != ic.inputCount ||
        expectedStr.length() != ic.outputCount)
      continue;

    uint8_t exp20 = 0xFF, exp21 = 0xFF;
    for (int i = 0; i < ic.inputCount; i++)
    {
      uint8_t addr, port;
      int pin = ic.inputPins[i];
      if (getExpanderForPin(pin, ic.pins, addr, port))
      {
        if (inputStr[i] == '0')
        {
          if (addr == 0x20)
            exp20 &= ~(1 << port);
          else
            exp21 &= ~(1 << port);
        }
      }
    }

    if (!writeI2C(0x20, exp20) || !writeI2C(0x21, exp21))
      return false;
    delay(50);

    uint8_t read20, read21;
    if (!readI2C(0x20, &read20) || !readI2C(0x21, &read21))
      return false;

    for (int o = 0; o < ic.outputCount; o++)
    {
      if (expectedStr[o] == '2')
        continue;
      uint8_t addr, port;
      int pin = ic.outputPins[o];
      if (getExpanderForPin(pin, ic.pins, addr, port))
      {
        bool expected = (expectedStr[o] == '1');
        bool actual = (addr == 0x20) ? (read20 & (1 << port)) : (read21 & (1 << port));
        if (expected != actual)
          return false;
      }
    }
  }
  return true;
}

void autoDetectIC()
{
  parseICs();
  display.clearDisplay();
  display.println("Auto-detecting IC...");
  display.display();

  for (int i = 0; i < icCount; i++)
  {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println(" Detecting ");
    display.setTextSize(1);
    display.println("---------------------");
    display.printf("Testing %s", ics[i].name.c_str());
    display.display();

    if (testIC(ics[i]))
    {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextColor(SH110X_WHITE);
      display.setTextSize(2);
      display.println(" Detected ");
      display.setTextSize(1);
      display.println("---------------------");
      display.printf("%s", ics[i].name.c_str());
      display.display();
      delay(3000);
      return;
    }
    delay(500);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.println(" Not Found ");
  display.setTextSize(1);
  display.println("---------------------");
  display.println("IC not recognized, maybe faulty or not in library.");
  display.display();
  delay(2000);
}

void loop()
{

  server.handleClient();

  if (millis() - lastDebounceTime > debounceDelay)
  {
    if (digitalRead(BTN_SCROLL) == LOW)
    {
      lastDebounceTime = millis();
      if (inSubmenu && viewingChips)
      {
        chipViewIndex = (chipViewIndex + 1) % icCount;
        showChipLines();
        Serial.printf("[UI] Selected: %s\n", ics[chipViewIndex].name.c_str());
      }
      else if (!inSubmenu)
      {
        currentSelection = (currentSelection + 1) % 3;
        showMenu();
        Serial.printf("[UI] Menu: %s\n", menuItems[currentSelection]);
      }
    }

    if (digitalRead(BTN_ENTER) == LOW)
    {
      lastDebounceTime = millis();
      if (inSubmenu && viewingChips)
      {
        Serial.printf("\n[TEST] Starting %s\n", ics[chipViewIndex].name.c_str());
        runTest(ics[chipViewIndex]);
      }
      else
      {
        inSubmenu = true;
        viewingChips = (currentSelection == 0 || currentSelection == 2);
        if (currentSelection == 0)
        {
          parseICs();
          showChipLines();
        }
        else if (currentSelection == 1)
        {
          autoDetectIC();
        }
        else if (currentSelection == 2)
        {
          display.clearDisplay();
          display.setCursor(0, 0);
          display.setTextColor(SH110X_WHITE);
          display.setTextSize(2);
          display.println("  System");
          display.setTextSize(1);
          display.println("---------------------");

          uint32_t freeRam = esp_get_free_heap_size();

          uint32_t totalFlash = spi_flash_get_chip_size();
          uint32_t flashUsed = ESP.getSketchSize();

          display.printf("CPU Freq: %lu MHz\n", getCpuFrequencyMhz());
          display.printf("Free RAM: %lu bytes\n", (unsigned long)freeRam);
          display.printf("Flash Use: %lu / %lu bytes\n",
                         (unsigned long)flashUsed,
                         (unsigned long)totalFlash);

          display.display();
        }
      }
    }

    if (digitalRead(BTN_BACK) == LOW && inSubmenu)
    {
      lastDebounceTime = millis();
      inSubmenu = false;
      viewingChips = false;
      showMenu();
      Serial.println("[UI] Returned to main menu");
    }
  }
}