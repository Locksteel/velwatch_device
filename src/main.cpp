#include <Arduino.h>
#include <SparkFunLSM6DSO.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <cJSON.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <HttpClient.h>


// NOTE: Eventually will have ability to networks on the fly, currently out of scope
char ssid[50];
char pass[50];
int runCount = 0;

LSM6DSO myIMU;

float accel;
float biasX, biasY, biasZ;
float dv, v, vOld;
long timeAccel, timeAccelLast;

int stationaryCount = 0;
int printCount = 0;

const float NOTIFY_THRESH = 2.0;
int notifyFlag = 0;

void nvs_access() {
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Open
  Serial.printf("\n");
  Serial.printf("Opening Non-Volatile Storage (NVS) handle... ");

  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else {
    Serial.printf("Done\n");
    Serial.printf("Retrieving SSID/PASSWD\n");
    size_t ssid_len;
    size_t pass_len;
    err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    err |= nvs_get_str(my_handle, "pass", pass, &pass_len);

    switch (err) {
      case ESP_OK:
      Serial.printf("Done\n");
      //Serial.printf("SSID = %s\n", ssid);
      //Serial.printf("PASSWD = %s\n", pass);
      break;
      case ESP_ERR_NVS_NOT_FOUND:
      Serial.printf("The value is not initialized yet!\n");
      break;
      default:
      Serial.printf("Error (%s) reading!\n", esp_err_to_name(err));
    }
  }
  // Close
  nvs_close(my_handle);
}


void calibrateBias()
{
  for (int i = 0; i < 100; i++)
  {
    biasX += myIMU.readFloatAccelX();
    biasY += myIMU.readFloatAccelY();
    biasZ += myIMU.readFloatAccelZ();
    delay(10);
  }
  biasX /= 100;
  biasY /= 100;
  biasZ /= 100;
}

void normalize(float &x, float &y, float &z)
{
  if (x < 0.3 && x > -0.3)
    x = 0;
  if (y < 0.3 && y > -0.3)
    y = 0;
  if (z < 0.3 && z > -0.3)
    z = 0;
}

float getTotalAccel(float x, float y, float z)
{
  return sqrt(pow(x, 2) + pow(y, 2) + pow(z - 9.8, 2));
}


void setup() {
  Serial.begin(9600);
  delay(500);

  // Wi-Fi connection
  nvs_access();
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());

  Wire.begin();
  delay(10);
  if( myIMU.begin() )
    Serial.println("Ready.");
  else { 
    Serial.println("Could not connect to IMU.");
    Serial.println("Freezing");
  }
  if( myIMU.initialize(BASIC_SETTINGS) )
    Serial.println("Loaded Settings.");

  Serial.println("Beginning calibration");
  calibrateBias();
  Serial.println("Calibration complete");

  vOld = 0;
  timeAccelLast = millis();
}

void loop() {
  float x, y, z;
  float dt;
  x = (myIMU.readFloatAccelX() - biasX) * 9.8;
  y = (myIMU.readFloatAccelY() - biasY) * 9.8;
  z = (myIMU.readFloatAccelZ() - biasZ) * 9.8;
  normalize(x, y, z);
  //accel = getTotalAccel(x, y, z);
  accel = x;

  // if (accel < 0.3 && accel > -0.3)
  //   accel = 0;

  timeAccel = millis();
  dt = ((float)timeAccel - (float)timeAccelLast) / 1000;
  dv = accel * dt;
  v = vOld + dv;

  if (accel < 1.0 && accel > -1.0)
    stationaryCount++;
  else
    stationaryCount = 0;

  if (stationaryCount >= 100)
  {
    v = 0;
    stationaryCount = 0;
  }

  if (printCount++ >= 100)
  {
    Serial.print("x: "); Serial.println(x);
    Serial.print("y: "); Serial.println(y);
    Serial.print("z: "); Serial.println(z);
    Serial.println();
    Serial.print("Acceleration: "); Serial.println(accel); 
    Serial.print("Speed: "); Serial.println(v);
    Serial.println();

    if (notifyFlag)
    {
      Serial.println("Notification threshold exceeded. Please slow down.");
      Serial.println();
      notifyFlag = 0;
    }

    printCount = 0;
  }

  timeAccelLast = timeAccel;
  vOld = v;

  
  if (v >= NOTIFY_THRESH || v <= (0.0 - NOTIFY_THRESH))
    notifyFlag = 1;

  runCount++;

  // check if 2 seconds have passed since beginning or last data send
  if (runCount >= 200)
  {
    runCount = 0;

    WiFiClient c;
    HttpClient http(c);

    char uri[256];

    // send speed data and temp vid of 1 (would give appropriate vid in full version)
    snprintf(uri, sizeof(uri), "/?speed=%f&vid=1", vOld);

    // send to server
    http.get("54.227.47.82", 1080, uri, NULL);
  }


  delay(10);
}