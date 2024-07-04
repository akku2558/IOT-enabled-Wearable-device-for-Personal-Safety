#include <SoftwareSerial.h>
#include <TinyGPS.h>

#define gpsRX 22
#define gpsTX 23
#define pushButton 0
#define buzzer 2
#define googleMapsUrl "Current Location https://www.google.com/maps/search/?api=1&query="

TinyGPS gps;
SoftwareSerial gpsSerial(gpsRX,gpsTX); 

bool newData = false;
String locationUrl;
void setup()
{
  // Configure button pin as input
  pinMode(pushButton, INPUT_PULLUP);

  //configure buzzer pin as output
  pinMode(buzzer, OUTPUT);

  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);

  //Begin serial communication with Arduino and SIM800L
  gpsSerial.begin(9600);

  // Serial.println("Initializing...");

  Serial.println("AT");
  waitForResponse(2000);

  Serial.println("ATE1");
  waitForResponse(2000);

  Serial.println("AT+CMGF=1");
  waitForResponse(2000);

  Serial.println("AT+CNMI=1,2,0,0,0");
  waitForResponse(2000);

  // simSerial.println("AT+CREG?");
  // waitForResponse(2000);
 
  // simSerial.println("AT+CGATT?");
  // waitForResponse(2000);
 
  // simSerial.println("AT+CIPSHUT");
  // waitForResponse(2000);
 
  // simSerial.println("AT+CIPSTATUS");
  // waitForResponse(2000);
 
  // simSerial.println("AT+CIPMUX=0");
  // waitForResponse(2000);

  // simSerial.println("AT+CSTT=\"uk.lebara.mobi\"");
  // waitForResponse(3000);

  // simSerial.println("AT+CIICR");//bring up wireless connection
  // waitForResponse(3000);

  // simSerial.println("AT+CIFSR");//get local IP adress
  // waitForResponse(3000);

  // simSerial.println("AT+CIPSPRT=0");
  // waitForResponse(3000);

  // simSerial.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"");//start up the connection
  // waitForResponse(3000);

  // simSerial.println("AT+CIPSEND");//begin send data to remote server
  // waitForResponse(3000);

  // String str="GET https://api.thingspeak.com/update?api_key=FY81WSAPS6XX4NSC&field1=50";
  // Serial.println(str);
  // simSerial.println(str);//begin send data to remote server
  // waitForResponse(5000);

  // simSerial.println((char)26);//sending
  // waitForResponse(3000);//waitting for reply, important! the time is base on the condition of internet 
  // simSerial.println();

  // simSerial.println("AT+CIPSHUT");//close the connection
  // waitForResponse(3000);
  
}

void loop()
{
  locationUrl = "Not detected";

  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (gpsSerial.available())
    {
      char c = gpsSerial.read();
      // Serial.write(c); // uncomment this line if you want to see the GPS data flowing
      if (gps.encode(c)) // Did a new valid sentence come in?
        newData = true;
    }
  }

  if (newData)
  {
    float flat, flon;
    unsigned long age;
    gps.f_get_position(&flat, &flon, &age);
    // Serial.print("LAT=");
    // Serial.print(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6);
    // Serial.print(" LON=");
    // Serial.println(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);
    // Serial.print(" SAT=");
    // Serial.print(gps.satellites() == TinyGPS::GPS_INVALID_SATELLITES ? 0 : gps.satellites());
    // Serial.print(" PREC=");
    // Serial.print(gps.hdop() == TinyGPS::GPS_INVALID_HDOP ? 0 : gps.hdop());

    locationUrl = googleMapsUrl + String(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6)  + "%2C" + String(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6) + "\"";
    // firebase.setString("user-location", locationUrl);
    // firebase.json(true);
  }

  if(digitalRead(pushButton) == LOW){
    digitalWrite(buzzer, HIGH);
    send_sms();
    digitalWrite(buzzer, LOW);

  }
}

void send_sms(){
  Serial.print("AT+CMGS=\"+447741875858\"\r");
  waitForResponse(1000);

  Serial.print(locationUrl);
  Serial.write(0x1A);
  waitForResponse(1000);
}


// void make_call(){
//   simSerial.println("ATD+447741875858;");
//   waitForResponse(2000);
// }

void waitForResponse(int delaytime){
  delay(delaytime);
  // while(simSerial.available()){
  //   Serial.println(simSerial.readString());
  // }
  Serial.read();
}
