#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <ArduinoJson.h>            // https://github.com/bblanchon/ArduinoJson 
#include <Firebase_ESP_Client.h>
#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include <WiFi.h>

#include <addons/TokenHelper.h>  //Provide the token generation process info.

const char *ssid = "Galaxy M32 5GAFEE";
const char *password = "tyyb1208";

#define gpsRX 12
#define gpsTX 14
#define gsmRX 32
#define gsmTX 33
#define pushButton 0
#define buzzer 2
#define googleMapsUrl "Current Location https://www.google.com/maps/search/?api=1&query="
//Select camera model
#define CAMERA_MODEL_WROVER_KIT  // Has PSRAM 
#include "camera_pins.h"

TinyGPS gps;
SoftwareSerial gpsSerial(gpsRX, gpsTX);
SoftwareSerial gsmSerial(gsmRX, gsmTX);

bool newData = false;
String locationUrl;
float flat, flon;
unsigned long age;
const char *filename = "/picture.jpg";

// Replace with your Firebase project credentials
#define FIREBASE_HOST "https://iot-wearable-device-babdb-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "dMGqdcWtp6ShZQLhtqe8WBwFru8kptTVDklYsfMb"
// #define REFERENCE_URL "gs://iot-wearable-device-babdb.appspot.com"  // Your Firebase project reference url
// Insert Firebase project API Key
#define API_KEY "AIzaSyDhJCj-dkcY9Z_EGAtFzdRnmq2237iufPc"
#define USER_EMAIL "firebase-adminsdk-4t8f4@iot-wearable-device-babdb.iam.gserviceaccount.com"
#define STORAGE_BUCKET_ID "iot-wearable-device-babdb.appspot.com"

// Photo File Name to save in LittleFS
#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO "/data/photo.jpg"

void startCameraServer();

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fconfig;

void fcsUploadCallback(FCS_UploadStatusInfo info);

bool taskCompleted = false;

void uploadToFirebase(){
  // if (Firebase.ready() && !taskCompleted) {
        taskCompleted = true;
        Serial.print("Uploading picture... ");

        //MIME type should be valid to avoid the download problem.
        //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
        if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, FILE_PHOTO_PATH /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, BUCKET_PHOTO /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */, fcsUploadCallback)) {
          Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
        } else {
          Serial.println(fbdo.errorReason());
        }
      // }
}

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS(void) {
  // Dispose first pictures because of bad quality
  camera_fb_t *fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++) {
    fb = esp_camera_fb_get();
    uploadToFirebase();
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  // Take a new photo
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    // ESP.restart();
  }

  // Photo file name
  Serial.printf("Picture file name: %s\n", FILE_PHOTO_PATH);
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
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    // ESP.restart();
  } else {
    delay(500);
    Serial.println("LittleFS mounted successfully \n");
  }
}

void cameraInit(){

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
  //setup wifi
   WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected"); 

  //Begin serial communication with Neo-7M
  gpsSerial.begin(9600);
  initGPS();

  //Begin serial communication with SIM800L
  gsmSerial.begin(9600);

  cameraInit();

   // Configure Firebase
  fconfig.host = FIREBASE_HOST;
  fconfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(512);
  Firebase.begin(&fconfig, &auth);
  Serial.println("Firebase connected successful........");

  initLittleFS();

 
  // Configure button pin as input
  pinMode(pushButton, INPUT_PULLUP);

  //configure buzzer pin as output
  pinMode(buzzer, OUTPUT);

  gsmSerial.println("AT");
  waitForResponse(2000);

  gsmSerial.println("ATE1");
  waitForResponse(2000);

  gsmSerial.println("AT+CMGF=1");
  waitForResponse(2000);

  gsmSerial.println("AT+CNMI=1,2,0,0,0");
  waitForResponse(2000);

}

void loop() {

  if (digitalRead(pushButton) == LOW) {
    digitalWrite(buzzer, HIGH);
    send_sms();
    digitalWrite(buzzer, LOW);
    capturePhotoSaveLittleFS();
    
  }

  gps.f_get_position(&flat, &flon, &age);
  locationUrl = googleMapsUrl + String(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6) + "%2C" + String(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);


}

void send_sms() {
  gsmSerial.print("AT+CMGS=\"+447741875858\"\r");
  waitForResponse(1000);

  gsmSerial.print(locationUrl);
  gsmSerial.write(0x1A);
  waitForResponse(1000);
}


// void make_call(){
//   simSerial.println("ATD+447741875858;");
//   waitForResponse(2000);
// }

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
      if (gps.encode(c))  // Did a new valid sentence come in?
        newData = true;
    }

    if (newData) {
      gps.f_get_position(&flat, &flon, &age);
      locationUrl = googleMapsUrl + String(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6) + "%2C" + String(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);
      break;
    }
  }
}

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info){
    if (info.status == firebase_fcs_upload_status_init){
        Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
    }
    else if (info.status == firebase_fcs_upload_status_upload)
    {
        Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_upload_status_complete)
    {
        Serial.println("Upload completed\n");
        FileMetaInfo meta = fbdo.metaData();
        Serial.printf("Name: %s\n", meta.name.c_str());
        Serial.printf("Bucket: %s\n", meta.bucket.c_str());
        Serial.printf("contentType: %s\n", meta.contentType.c_str());
        Serial.printf("Size: %d\n", meta.size);
        Serial.printf("Generation: %lu\n", meta.generation);
        Serial.printf("Metageneration: %lu\n", meta.metageneration);
        Serial.printf("ETag: %s\n", meta.etag.c_str());
        Serial.printf("CRC32: %s\n", meta.crc32.c_str());
        Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
        Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
    }
    else if (info.status == firebase_fcs_upload_status_error){
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}
