#include <SoftwareSerial.h>

#include <TinyGPS.h>
#include <WiFi.h>
#include <ArduinoJson.h>            // https://github.com/bblanchon/ArduinoJson 
#include <ESP32Firebase.h>

const char* ssid = "Galaxy M32 5GAFEE";
const char* password = "tyyb1208";
#define REFERENCE_URL "https://iot-wearable-device-babdb-default-rtdb.firebaseio.com/"  // Your Firebase project reference url 

/* This sample code demonstrates the normal use of a TinyGPS object.
   It requires the use of SoftwareSerial, and assumes that you have a
   4800-baud serial GPS device hooked up on pins 4(rx) and 3(tx).
*/

#define RX 22
#define TX 23
#define googleMapsUrl "https://www.google.com/maps/search/?api=1&query="

TinyGPS gps;
SoftwareSerial ss(RX, TX);
Firebase firebase(REFERENCE_URL);

void setup()
{
  Serial.begin(115200);
  ss.begin(9600);
  
  
  Serial.println();

  delay(1000);

  WiFi.mode(WIFI_STA); //Optional
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");

  while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(100);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());


  // Write some data to the realtime database.
  // firebase.setString("Example/setString", "It's Working");
  // firebase.setInt("Example/setInt", 123);
  // firebase.setFloat("Example/setFloat", 45.32);

  // firebase.json(true);              // Make sure to add this line.
  
  // String data = firebase.getString("Example");  // Get data from the database.

  // // Deserialize the data.
  // // Consider using Arduino Json Assistant- https://arduinojson.org/v6/assistant/
  // const size_t capacity = JSON_OBJECT_SIZE(3) + 50;
  // DynamicJsonDocument doc(capacity);

  // deserializeJson(doc, data);

  // // Store the deserialized data.
  // const char* received_String = doc["setString"]; // "It's Working"
  // int received_int = doc["setInt"];               // 123
  // float received_float = doc["setFloat"];         // 45.32

  // // Print data
  // Serial.print("Received String:\t");
  // Serial.println(received_String);

  // Serial.print("Received Int:\t\t");
  // Serial.println(received_int);

  // Serial.print("Received Float:\t\t");
  // Serial.println(received_float);

  // Delete data from the realtime database.
  // firebase.deleteData("Example");
}

void loop()
{
  bool newData = false;
  // unsigned long chars;
  // unsigned short sentences, failed;

  // For one second we parse GPS data and report some key values
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (ss.available())
    {
      char c = ss.read();
      // Serial.write(c); // uncomment this line if you want to see the GPS data flowing
      // Serial.println("Waiting for gps data......");
      if (gps.encode(c)) // Did a new valid sentence come in?
        newData = true;
    }
  }

  if (newData)
  {
    float flat, flon;
    unsigned long age;
    gps.f_get_position(&flat, &flon, &age);
    Serial.print("LAT=");
    Serial.print(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6);
    Serial.print(" LON=");
    Serial.println(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);
    // Serial.print(" SAT=");
    // Serial.print(gps.satellites() == TinyGPS::GPS_INVALID_SATELLITES ? 0 : gps.satellites());
    // Serial.print(" PREC=");
    // Serial.print(gps.hdop() == TinyGPS::GPS_INVALID_HDOP ? 0 : gps.hdop());

    String locationUrl = googleMapsUrl + String(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6)  + " " + String(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);
    firebase.setString("user-location", locationUrl);
    firebase.json(true);
  }
  
  // gps.stats(&chars, &sentences, &failed);
  // Serial.print(" CHARS=");
  // Serial.print(chars);
  // Serial.print(" SENTENCES=");
  // Serial.print(sentences);
  // Serial.print(" CSUM ERR=");
  // Serial.println(failed);
  // if (chars == 0)
    // Serial.println("** No characters received from GPS: check wiring **");
}