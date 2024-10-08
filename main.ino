#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <ArduinoJson.h>
#include <FirebaseClient.h>
#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>

#define gpsRX 12
#define gpsTX 14
#define gsmRX 32
#define gsmTX 33
#define pushButton 0
#define buzzer 2
#define googleMapsUrl "Current Location https://www.google.com/maps/search/?api=1&query="
String SMS_TARGETS[5] = { "+44XXXXXXXXXX", "+44XXXXXXXXXX" };


//Select camera model
#define CAMERA_MODEL_WROVER_KIT  // Has PSRAM
#include "camera_pins.h"

#define TINY_GSM_MODEM_SIM900
#include <TinyGSM.h>

#define TINY_GSM_USE_GPRS true

TinyGPS gps;
SoftwareSerial gpsSerial(gpsRX, gpsTX);
SoftwareSerial gsmSerial(gsmRX, gsmTX);

#define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(gsmSerial, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(gsmSerial);
#endif

TinyGsmClient gsmClient(modem);

// Your GPRS credentials, if any
#define GSM_PIN ""
const char apn[] = "uk.lebara.mobi";
const char gprsUser[] = "wap";
const char gprsPass[] = "wap";

bool newData = false;
String locationUrl;
float flat, flon;
unsigned long age;
bool taskCompleted = false;
#define FILE_PHOTO_PATH "/photo.jpg"


// Replace with your Firebase project credentials
#define FIREBASE_HOST "https://iot-wearable-device-babdb-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "dMGqdcWtp6ShZQLhtqe8WBwFru8kptTVDklYsfMb"
#define API_KEY "AIzaSyDhJCj-dkcY9Z_EGAtFzdRnmq2237iufPc"
#define STORAGE_BUCKET_ID "iot-wearable-device-babdb.appspot.com"

ESP_SSLClient sslClient;

GSMNetwork gsmNetwork(&modem, GSM_PIN, apn, gprsUser, gprsPass);

NoAuth noAuth;

FirebaseApp app;

using AsyncClient = AsyncClientClass;

AsyncClient aClient(sslClient, getNetwork(gsmNetwork));

Storage storage;

void asyncCB(AsyncResult &aResult);

void printResult(AsyncResult &aResult);

void startCameraServer();

void sendSms(String messageToBeSent);

void fileCallback(File &file, const char *filename, file_operating_mode mode);

FileConfig media_file("/photo.jpg", fileCallback);


// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS(void) {
  // Dispose first pictures because of bad quality
  camera_fb_t *fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  // Take a new photo
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
  }

  // temporarily saving the image
  File file = LittleFS.open(FILE_PHOTO_PATH, FILE_WRITE);

  // Insert the data in the photo file
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb->buf, fb->len);  // payload (image), payload length
    Serial.print("The picture has been saved in ");
    Serial.print(FILE_PHOTO_PATH);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
    uploadToFirebase();
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  } else {
    delay(500);
    Serial.println("LittleFS mounted successfully \n");
  }
}

void cameraInit() {

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }


  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  } else {
    Serial.println("Camera Initialized");
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Initializing..........................................................");

  //Begin serial communication with Neo-7M
  gpsSerial.begin(9600);
  initGPS();


  //Begin serial communication with SIM900A
  gsmSerial.begin(9600);
  initGSM();
  
  cameraInit();

  //Configure Firebase
  initializeApp(aClient, app, getAuth(noAuth), asyncCB, "authTask");

  app.getApp<Storage>(storage);

  //Initialize LittleFS
  initLittleFS();

  // Configure button pin as input
  pinMode(pushButton, INPUT_PULLUP);

  //configure buzzer pin as output
  pinMode(buzzer, OUTPUT);
}

void loop() {

  app.loop();

  storage.loop();
  //checking if button is pressed
  if (digitalRead(pushButton) == LOW) {
    delay(10);
    digitalWrite(buzzer, HIGH);
    sendSms(locationUrl);
    digitalWrite(buzzer, LOW);
    capturePhotoSaveLittleFS();
  }
  //updating location coordinates
  gps.f_get_position(&flat, &flon, &age);
  locationUrl = googleMapsUrl + String(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6) + "%2C" + String(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);
}


//for sending sms to the emergency contacts
void sendSms(String messageToBeSent) {
  for (int i = 0; i < 5; i++) {
    if (SMS_TARGETS[i] != "") {
      modem.sendSMS(SMS_TARGETS[i], messageToBeSent);
      Serial.print("SMS sent to ");
      Serial.println(SMS_TARGETS[i]);
    }
  }
}

//receiving response from GSM
void waitForResponse(int delaytime) {
  delay(delaytime);
  while (gsmSerial.available()) {
    Serial.println(gsmSerial.readString());
  }
  gsmSerial.read();
}

void initGPS() {
  while (1) {
    while (gpsSerial.available()) {
      char c = gpsSerial.read();
      Serial.write(c);
      if (gps.encode(c)) {
        newData = true;
      }
    }

    if (newData) {
      Serial.println("\nGPS connected.......");
      gps.f_get_position(&flat, &flon, &age);
      locationUrl = googleMapsUrl + String(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6) + "%2C" + String(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);
      break;
    }
  }
}

void initGSM() {
  modem.init();

  if (!modem.waitForNetwork(600000L, true)) {
    delay(1000);
    return;
  }

  if (modem.isNetworkConnected()) { Serial.println("Network connected"); }
  modem.gprsConnect(apn, gprsUser, gprsPass);

  if (modem.isGprsConnected()) {
    Serial.println("GPRS connected");
  }

  sslClient.setInsecure();

  sslClient.setDebugLevel(1);

  sslClient.setBufferSizes(2048 /* rx */, 1024 /* tx */);

  sslClient.setClient(&gsmClient);

}

//Time stamp
String getCurrentTime() {
  int year;
  byte month, day, hour, minute, second, hundredths;
  char timeDate[32];
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age == TinyGPS::GPS_INVALID_AGE)
    Serial.print("********** ******** ");
  else {
    sprintf(timeDate, "%02d_%02d_%02d_%02d:%02d:%02d",
            day, month, year, hour, minute, second);
    Serial.print(timeDate);
  }
  String currentTime = String(timeDate);
  return currentTime;
}

void uploadToFirebase() {
  if (!taskCompleted) {
    taskCompleted = true;
    Serial.print("Uploading picture... ");

    storage.upload(aClient, FirebaseStorage::Parent(STORAGE_BUCKET_ID, "/data/IMG_" + String(getCurrentTime()) + ".jpg"), getFile(media_file), "image/jpg", asyncCB);

    taskCompleted = false;
  }

}


void asyncCB(AsyncResult &aResult)
{
    printResult(aResult);
}

void printResult(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.downloadProgress())
    {
        Firebase.printf("Download task: %s, downloaded %d%s (%d of %d)\n", aResult.uid().c_str(), aResult.downloadInfo().progress, "%", aResult.downloadInfo().downloaded, aResult.downloadInfo().total);
        if (aResult.downloadInfo().total == aResult.downloadInfo().downloaded)
        {
            Firebase.printf("Download task: %s, completed!\n", aResult.uid().c_str());
        }
    }

    if (aResult.uploadProgress())
    {
        Firebase.printf("Upload task: %s, uploaded %d%s (%d of %d)\n", aResult.uid().c_str(), aResult.uploadInfo().progress, "%", aResult.uploadInfo().uploaded, aResult.uploadInfo().total);
        if (aResult.uploadInfo().total == aResult.uploadInfo().uploaded)
        {
            Firebase.printf("Upload task: %s, completed!\n", aResult.uid().c_str());
            Serial.print("Download URL: ");
            Serial.println(aResult.uploadInfo().downloadUrl);
            String downloadUrl = aResult.uploadInfo().downloadUrl;
            sendSms(downloadUrl);

        }
    }
}


void fileCallback(File &file, const char *filename, file_operating_mode mode)
{
    // FILE_OPEN_MODE_READ, FILE_OPEN_MODE_WRITE and FILE_OPEN_MODE_APPEND are defined in this library
    switch (mode)
    {
    case file_mode_open_read:
        file = LittleFS.open(filename, FILE_READ);
        break;
    case file_mode_open_write:
        file = LittleFS.open(filename, FILE_WRITE);
        break;
    case file_mode_open_append:
        file = LittleFS.open(filename, FILE_APPEND);
        break;
    case file_mode_remove:
        LittleFS.remove(filename);
        break;
    default:
        break;
    }
}
