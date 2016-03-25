#include <ESP8266WiFi.h>
#include <Servo.h>
#include "secrets.h"

#define WEBAPP 0  // use 1 to enable web application serving
#if WEBAPP
#include "webapp.h"
#endif

const byte SERVO_COUNT = 6;
const int SERVO_TICK_DELAY_MS = 15;

Servo servos[SERVO_COUNT];

// handles one program step to a set of positions, with timed moves
class Step {
  public:
    int stepTime;  // tenth of seconds
    byte pos[SERVO_COUNT];
    Step() {
      stepTime = 0;
      for (int i = 0; i < SERVO_COUNT; i++) {
        pos[i] = 90;
      }
    }
    Step(const Step &step) {
      stepTime = step.stepTime;
      for (int i = 0; i < SERVO_COUNT; i++) {
        pos[i] = step.pos[i];
      }
    }
    void moveTo() {
      if (stepTime == 0) {
        // no time - go directly to destination
        for (int i = 0; i < SERVO_COUNT; i++) {
          servos[i].write(pos[i]);
        }
      } else {
        int tickCount = 100L * stepTime / SERVO_TICK_DELAY_MS;
        struct posDist {
          float currPos;
          float tickDist;
        } posDists[SERVO_COUNT];

        for (int i = 0; i < SERVO_COUNT; i++) {
          struct posDist &posDist = posDists[i];
          posDist.currPos = (float)servos[i].read();
          posDist.tickDist = (pos[i] - posDist.currPos) / tickCount;
        }
        while (tickCount-- > 0) {
          for (int i = 0; i < SERVO_COUNT; i++) {
            struct posDist &posDist = posDists[i];
            posDist.currPos += posDist.tickDist;
            servos[i].write((int)posDist.currPos);
          }
          delay(SERVO_TICK_DELAY_MS);
        }
      }
    }
};

const int MAX_STEPS = 4090 / sizeof(Step);

WiFiServer server(80);

Step currentStep;
Step steps[MAX_STEPS];
int stepCount = 0;

void setup() {
  // assign GPIO pins to bits (arduino numbering standard)
  servos[0].attach(16);
  servos[1].attach(14);
  servos[2].attach(12);
  servos[3].attach(13);
  servos[4].attach(15);
  servos[5].attach(4);

  Serial.begin(115200);
  delay(10);

  // Connect to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void httpRespond(WiFiClient client, int status) {
  client.print("HTTP/1.1 ");
  client.print(status);
  client.println(" OK");
  client.println(""); // mark end of headers
}

void httpRespond(WiFiClient client, int status, char *contentType) {
  client.print("HTTP/1.1 ");
  client.print(status);
  client.println(" OK");
  client.print("Content-Type: "); client.println(contentType);
  client.println(""); // mark end of headers
}

#if WEBAPP
bool loadFromFlash(WiFiClient &client, String path) {
  if (path.endsWith("/")) path += "index.html";
  int NumFiles = sizeof(files)/sizeof(struct t_websitefiles);
  for (int i=0; i<NumFiles; i++) {
    if (path.endsWith(String(files[i].filename))) {
      client.println("HTTP/1.1 200 OK");
      client.print("Content-Type: "); client.println(files[i].mime);
      client.print("Content-Length"); client.println(String(files[i].len));
      client.println(""); //  do not forget this one
      _FLASH_ARRAY<uint8_t>* filecontent = (_FLASH_ARRAY<uint8_t>*)files[i].content;
      filecontent->open();
      client.write(*filecontent, 100);
      return true;
    }
  }
  httpRespond(client, 201);
  return false;
}
#endif

// convert string representation into referenced Step
// s is t,p,p,p,p,p,p,p
// omittes numbers are not set
void stringToStep(String s, Step &step) {
  int i = 0, j;
  j = s.indexOf(',', i);
  if (i < j) {
    float t = s.substring(i, j).toFloat();    
    step.stepTime = (int)(t * 10 + 0.5);
  }
  i = j + 1;
  for (int posi = 0; posi < SERVO_COUNT; posi++) {
    if (j < s.length()) {
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
}

void printStepsJson(WiFiClient client) {
  client.println("[");
  for (int i = 0; i < stepCount; i++) {
    client.print("  {\"timeToStep\": ");
    int t = steps[i].stepTime;
    client.print(t / 10); client.print("."); client.print(t % 10);
    client.print(", \"positions\": [");
    for (int j = 0; j < SERVO_COUNT; j++) {
      if (j > 0) {
        client.print(", ");
      }
      client.print(steps[i].pos[j]);
    }
    client.print("]}");
    if (i < stepCount - 1) {
      client.print(",");
    }
    client.println("");
  }
  client.println("]");
}

String getRequestQuery(String s) {
  int i = s.indexOf(' ');
  int j = s.lastIndexOf(' ');
  return s.substring(i + 1, j);
}


int parseIntUntil(String s, int &intResult, int &startI, char endChar) {
  int i = s.indexOf('/', startI);
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

void loop() {
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  while(!client.available()){
    delay(1);
  }

  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  // Match the request
  String query = getRequestQuery(request);
  if (query.equals("/stepcount")) {
    httpRespond(client, 200, "application/json");
    client.println(stepCount);
  } else if (query.equals("/steps")) {
    httpRespond(client, 200, "application/json");
    printStepsJson(client);
  } else if (query.startsWith("/add/")) {
    int stepi;
    String stepString;
    int i = 5;
    if (parseIntUntil(query, stepi, i, '/')
        && parseStringToEnd(query, stepString, i)) {
      if (0 <= stepi && stepi <= stepCount) {
        if (stepi < stepCount) {
          for (int i = stepCount; i > stepi; i--) {
            steps[i] = steps[i - 1];
          }
        }
        stepCount++;
        stringToStep(stepString, steps[stepi]);
        httpRespond(client, 201);
        return;
      }
    }
    httpRespond(client, 500);
  } else if (query.startsWith("/remove/")) {
    httpRespond(client, 201);
  } else if (query.equals("/run")) {
    httpRespond(client, 201);
  } else if (query.startsWith("/set/")) {
    Step step;
    stringToStep(query.substring(5), step);
    step.moveTo();
    httpRespond(client, 201);
#if WEBAPP
  } else {
    String path = request.substring(4, request.length() - 9);
    loadFromFlash(client, path);
#endif
  }
}
