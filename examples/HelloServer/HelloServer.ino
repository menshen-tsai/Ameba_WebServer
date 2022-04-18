// AmebaD 3.1.2
#include <AmebaWebServer.h>

char ssid[] = "........";    // your network SSID (name)
char pass[] = "........";       // your network password
int keyIndex = 0;               // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

AmebaWebServer server(80);

void handleRoot() {
  digitalWrite(LED_B, 1);
  server.send(200, "text/plain", "hello from Ameba RTL8722DM_MINI!");
  delay(1000);
  digitalWrite(LED_B, 0);
}

void handleNotFound(){
  digitalWrite(LED_G, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  delay(1000);
  digitalWrite(LED_G, 0);
}


void setup() {
    //Initialize serial and wait for port to open:
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    pinMode(LED_G, OUTPUT);
    pinMode(LED_B, OUTPUT);
    // check for the presence of the shield:
    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("WiFi shield not present");
        // don't continue:
        while (true);
    }

    // attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(ssid, pass);

        // wait 10 seconds for connection:
        delay(10000);
    }

    server.on("/", handleRoot);
    server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

    server.onNotFound(handleNotFound);

    Serial.println();
    Serial.print("Firmware Version: ");
    Serial.println(server.firmwareVersion());
    server.begin();
    // connected, print out the status:
    printWifiStatus();
}


void loop() {
  server.handleClient();
  delay(100);
}


void printWifiStatus() {
    // print the SSID of the network attached to:
    Serial.println();
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
}