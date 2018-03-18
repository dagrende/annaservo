#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>
#include "step.h"
#include "secrets.h"

AsyncWebServer webServer(80);
Servo servos[SERVO_COUNT];
int otaMode = 0;
const int MAX_STEPS = 2040 / sizeof(Step);
int runMode = false;
int nextStep = 0;


Servo myservo;

int pos = 0;    // variable to store the servo position

// handles one program step to a set of positions, with timed moves
Step::Step() {
      stepTime = 0;
      for (int i = 0; i < SERVO_COUNT; i++) {
        pos[i] = 90;
      }
    }
void Step::moveTo() {
  // Serial.print("moveTo pos");
  if (stepTime == 0) {
    // no time - go directly to destination
    for (int i = 0; i < SERVO_COUNT; i++) {
      // Serial.print(" "); Serial.print(pos[i]);
      servos[i].write(pos[i]);
    }
  } else {
    int tickCount = 100L * stepTime / SERVO_TICK_DELAY_MS;
    struct posDist {
      float currPos;
      float tickDist;
    } posDists[SERVO_COUNT];

    for (int i = 0; i < SERVO_COUNT; i++) {
      // Serial.print(" "); Serial.print(pos[i]);
      struct posDist &posDist = posDists[i];
      posDist.currPos = (float)servos[i].read();
      posDist.tickDist = (pos[i] - posDist.currPos) / tickCount;
    }
    // Serial.print(" in "); Serial.print(stepTime / 10.0); Serial.print("s");
    while (tickCount-- > 0) {
      for (int i = 0; i < SERVO_COUNT; i++) {
        struct posDist &posDist = posDists[i];
        posDist.currPos += posDist.tickDist;
        servos[i].write((int)posDist.currPos);
      }
      delay(SERVO_TICK_DELAY_MS);
    }
  }
  // Serial.println(" ");
}

const long programMagicNumber = 671349586L; // used to check if flash data is a program saved by this app
struct {
  long magicNumber;
  int formatVersion;
  Step steps[MAX_STEPS];
  int stepCount;
} program __attribute__((aligned(4)));

int saveProgram() {
  int success = 0;
  // program.magicNumber = programMagicNumber;
  // program.formatVersion = 1;
  // noInterrupts();
  // if(spi_flash_erase_sector(_sector) == SPI_FLASH_RESULT_OK) {
  //   if(spi_flash_write(_sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t*>(&program), sizeof(program)) == SPI_FLASH_RESULT_OK) {
  //     success = 1;
  //   }
  // }
  // interrupts();
  return success;
}

int restoreProgram() {
  int success = 0;
  // noInterrupts();
  // if(spi_flash_read(_sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t*>(&program), sizeof(program)) == SPI_FLASH_RESULT_OK) {
  //   if (program.magicNumber == programMagicNumber) {
  //     success = 1;
  //   } else {
      // flash did not contain a valid program - clear it
      // Serial.println("restoreProgram: not valid program in flash");
      program.stepCount = 0;
      program.formatVersion = 1;
  //   }
  // }
  // interrupts();
  return success;
}

void attachServos() {
  // Serial.println("attachServos");
  servos[0].attach(15);
  servos[1].attach(13);
  servos[2].attach(12);
  servos[3].attach(14);
  servos[4].attach(16);
  servos[5].attach(5);
}

void detachServos() {
  // Serial.println("detachServos");
  servos[0].detach();
  servos[1].detach();
  servos[2].detach();
  servos[3].detach();
  servos[4].detach();
  servos[5].detach();
}

void httpRespond(WiFiClient client, int status) {
  client.print("HTTP/1.1 ");
  client.print(status);
  client.println(" OK");
  client.println("Access-Control-Allow-Origin: *");
  client.println(""); // mark end of headers
}

void httpRespond(WiFiClient client, int status, const char *contentType) {
  client.print("HTTP/1.1 ");
  client.print(status);
  client.println(" OK");
  client.print("Content-Type: "); client.println(contentType);
  client.println("Access-Control-Allow-Origin: *");
  client.println(""); // mark end of headers
}

// convert string representation into referenced Step
// s is t,p,p,p,p,p,p,p
// omittes numbers are not set
int stringToStep(String s, Step &step) {
  int i = 0, j;
  // parse time (float) terminated by comma
  j = s.indexOf(',', i);
  if (i < j) {
    float t = s.substring(i, j).toFloat();
    step.stepTime = (int)(t * 10 + 0.5);
  }
  i = j + 1;
  // get each servo position
  for (int posi = 0; posi < SERVO_COUNT; posi++) {
    if (j < s.length()) {
      // parse position (int) terminated by comma or end of string
      j = s.indexOf(',', i);
      if (j == -1) {
        j = s.length();
      }
      if (i < j) {
        step.pos[posi] = s.substring(i, j).toInt();
      }
      i = j + 1;
    }
  }
  return true;
}

void printStepsJson(AsyncResponseStream *response) {
  response->println("[");
  for (int i = 0; i < program.stepCount; i++) {
    response->print("  {\"timeToStep\": ");
    int t = program.steps[i].stepTime;
    response->print(t / 10); response->print("."); response->print(t % 10);
    response->print(", \"positions\": [");
    for (int j = 0; j < SERVO_COUNT; j++) {
      if (j > 0) {
        response->print(", ");
      }
      response->print(program.steps[i].pos[j]);
    }
    response->print("]}");
    if (i < program.stepCount - 1) {
      response->print(",");
    }
    response->println("");
  }
  response->println("]");
}

String getRequestQuery(String s) {
  int i = s.indexOf(' ');
  int j = s.lastIndexOf(' ');
  return s.substring(i + 1, j);
}

void runProgram() {
  while (1) {
    for (int i = 0; i < program.stepCount; i++) {
      program.steps[i].moveTo();
    }
  }
}

int parseIntUntil(String s, int &intResult, int &startI, char endChar) {
  int i = endChar == 0 ? s.length() : s.indexOf(endChar, startI);
  if (i != -1) {
    intResult = s.substring(startI, i).toInt();
    startI = i + 1;
    return 1;
  }
  return 0;
}

int parseStringToEnd(String s, String &stringResult, int &startI) {
  stringResult = s.substring(startI);
  startI = s.length();
  return 1;
}

void handleNotFound(AsyncWebServerRequest *request) {
  String query = request->url();
  if (query.startsWith("/add/")) {
    // /add/i/t,p,p,p,p,p,p
    // inserts a new step at specified pos
    int stepi;
    String stepString;
    int i = 5;
    if (parseIntUntil(query, stepi, i, '/')
        && parseStringToEnd(query, stepString, i)) {
      if (0 <= stepi && stepi <= program.stepCount) {
        if (stepi < program.stepCount) {
          for (int i = program.stepCount; i > stepi; i--) {
            program.steps[i] = program.steps[i - 1];
          }
        }
        program.stepCount++;
        stringToStep(stepString, program.steps[stepi]);
        request->send(200, "application/json", "1");
        return;
      }
    }
    request->send(400);
  }
  else if (query.startsWith("/remove/")) {
    int stepi, stepn;
    int i = 8;
    if (parseIntUntil(query, stepi, i, '/')
        && parseIntUntil(query, stepn, i, 0)
        && 0 <= stepi && stepi < program.stepCount
        && 0 < stepn && stepi + stepn <= program.stepCount) {
      for (int j = 0; j < stepn; j++) {
        program.steps[stepi] = program.steps[stepi + stepn];
        stepi++;
      }
      program.stepCount -= stepn;
      request->send(200, "application/json", "1");
      return;
    }
    request->send(400);
  } else if (query.startsWith("/set/")) {
    // move all positions to specified value at specified time
    int stepi, stepn;
    Step step;
    int i = 5;
    if (parseIntUntil(query, stepi, i, '/')
        && 0 <= stepi && stepi < program.stepCount
        && stringToStep(query.substring(i), step)) {
      program.steps[stepi] = step;
      request->send(200, "application/json", "1");
    } else {
      request->send(500, "application/json", "\"invalid step index\"");
    }
  } else if (query.startsWith("/move/")) {
    // move all positions to specified value at specified time
    Step step;
    stringToStep(query.substring(5), step);
    step.moveTo();
    request->send(200, "application/json", "1");
#if WEBAPP
  } else {
    String path = query.substring(1);
    Serial.println(path);
    loadFromFlash(server.client(), path);
#endif
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  while (!Serial) {}  // wait for serial ready
  Serial.println("\nBooting");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("annaservo", "hosianna"); // http://192.168.4.1/
  WiFi.begin(ssid, password);

  Serial.print("connecting to: ");
  Serial.println(ssid);

  int n = 5;
  while (WiFi.status() != WL_CONNECTED && n-- > 0) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  SPIFFS.begin();


  // server.on("/stepCount", [](){
  webServer.on("/stepCount", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(program.stepCount));
  });

  // server.on("/save", [](){
  webServer.on("/save", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(saveProgram() ? 200 : 500, "application/json", "1");
  });

  // server.on("/restore", [](){
  webServer.on("/restore", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(restoreProgram() ? 200 : 500, "application/json", "1");
  });

  // server.on("/start", [](){
  webServer.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    runMode = true;
    request->send(200, "application/json", "1");
  });

  // server.on("/stop", [](){
  webServer.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    runMode = false;
    request->send(200, "application/json", "1");
  });

  webServer.on("/steps", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->addHeader("Server","ESP Async Web Server");
    response->printf("<!DOCTYPE html><html><head><title>Webpage at %s</title></head><body>", request->url().c_str());

    printStepsJson(response);

    request->send(response);
  });

  webServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  webServer.onNotFound(handleNotFound);

  webServer.begin();
  // Serial.println("HTTP server started");

  attachServos();
  restoreProgram();
  runMode = false;
}


void loop() {
  if (runMode && program.stepCount > 0) {
    if (nextStep >= program.stepCount) {
      nextStep = 0;
    }
    program.steps[nextStep++].moveTo();
  }
}
