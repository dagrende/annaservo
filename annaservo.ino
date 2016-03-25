#include <ESP8266WiFi.h>
#include <Servo.h>
#include "webapp.h"
#include "secrets.h"

WiFiServer server(80);
Servo myservo;

void setup() {
  myservo.attach(14);
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

bool loadFromFlash(WiFiClient &client, String path, String lastModified, String ifModifiedSince) {
  if (path.endsWith("/")) path += "index.html";
  int NumFiles = sizeof(files)/sizeof(struct t_websitefiles);
  for (int i=0; i<NumFiles; i++) {
    if (path.endsWith(String(files[i].filename))) {
      if (ifModifiedSince.equals(files[i].lastModified)) {
        client.println("HTTP/1.1 304 Not Modified");
        client.println(""); //  do not forget this one
        return false;
      }
      client.println("HTTP/1.1 200 OK");
      client.print("Content-Type: "); client.println(files[i].mime);
      client.print("Last-Modified: "); client.println(files[i].lastModified);
      client.print("Content-Length"); client.println(String(files[i].len));
      client.println(""); //  do not forget this one
      _FLASH_ARRAY<uint8_t>* filecontent = (_FLASH_ARRAY<uint8_t>*)files[i].content;
      filecontent->open();
      client.write(*filecontent, 100);
      return true;
    }
  }
  client.println("HTTP/1.1 404 OK");
  client.println("");
  return false;
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
  String lastModified = "";
  String ifModifiedSince = "";
  const char *lastModifiedName = "Last-Modified: ";
  const char *ifModifiedSinceName = "If-Modified-Since: ";
  while (client.available()) {
    String header = client.readStringUntil('\r');
    if (header.startsWith(lastModifiedName)) {
      lastModified = header.substring(strlen(lastModifiedName), header.length() - 1);
    } else if (header.startsWith(ifModifiedSinceName)) {
      ifModifiedSince = header.substring(strlen(ifModifiedSinceName), header.length() - 1);
    }
  }
  client.flush();

  // Match the request
  int i;

  if ((i = request.indexOf("/set/")) != -1){
    i += 5;
    int j = request.indexOf("/", i);
    if (j != -1) {
      int v = request.substring(i, j).toInt();
      myservo.write(v);
      Serial.println(v);
    }
    // Return the response
    client.println("HTTP/1.1 201 OK");
    client.println(""); //  do not forget this one
  } else {
    String path = request.substring(4, request.length() - 9);
    loadFromFlash(client, path, lastModified, ifModifiedSince);
  }

  delay(1);
  Serial.println("Client disonnected");
  Serial.println("");

}
