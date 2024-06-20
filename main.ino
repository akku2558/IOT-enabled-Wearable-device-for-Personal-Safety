#include <SoftwareSerial.h>

#define rxPin 23
#define txPin 22
SoftwareSerial sim800L(rxPin,txPin); 

String buff;
void setup()
{
  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);

  //Begin serial communication with Arduino and SIM800L
  sim800L.begin(9600);

  Serial.println("Initializing...");

  sim800L.println("AT");
  waitForResponse();

  sim800L.println("ATE1");
  waitForResponse();

  sim800L.println("AT+CMGF=1");
  waitForResponse();

  sim800L.println("AT+CNMI=1,2,0,0,0");
  waitForResponse();
}

void loop()
{
  while(sim800L.available()){
    buff = sim800L.readString();
    Serial.println(buff);
  }
  while(Serial.available())  {
    buff = Serial.readString();
    buff.trim();
    if(buff == "s")
      send_sms();
    else if(buff== "c")
      make_call();
    else
      sim800L.println(buff);
  }
}

void send_sms(){
  sim800L.print("AT+CMGS=\"+447741875858\"\r");
  waitForResponse();

  sim800L.print("My Location https://www.google.com/maps/search/?api=1&query=47.5951518%2C-122.3316393");
  sim800L.write(0x1A);
  waitForResponse();
}


void make_call(){
  sim800L.println("ATD+447741875858;");
  waitForResponse();
}

void waitForResponse(){
  delay(1000);
  while(sim800L.available()){
    Serial.println(sim800L.readString());
  }
  sim800L.read();
}