#include <Arduino.h>
#include <vector>
#include <DHT20.h>
#include <MQ135.h>
#include <TFT_eSPI.h> // Hardware-specific library


#include <Arduino.h>
#include <HttpClient.h>
#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

TFT_eSPI tft = TFT_eSPI();

#define PIN_MQ135 39
#define PIN_PIRSENSOR 36
#define PIN_LED 15
#define PIN_LIGHTSENSOR 33
#define PIN_AMPSENSOR 32
 
long StartTime {millis()};
bool LEGACY {false};
std::string Canister;
char SERVERIP[] = "18.117.97.196";
char ssid[] = "123456"; // your network SSID (name)
char pass[] = "12345678"; // your network password (use for WPA, or use
// as key for WEP)
const int kNetworkTimeout = 30 * 1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

void nvs_access()
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
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
    if (err != ESP_OK)
    {
        Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        Serial.printf("Done\n");
        Serial.printf("Retrieving SSID/PASSWD\n");
        size_t ssid_len;
        size_t pass_len;
        err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
        err |= nvs_get_str(my_handle, "pass", pass, &pass_len);
        switch (err)
        {
        case ESP_OK:
            Serial.printf("Done\n");
            // Serial.printf("SSID = %s\n", ssid);
            // Serial.printf("PASSWD = %s\n", pass);
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

void connectToWiFi()
{
    nvs_access();
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("MAC address: ");
    Serial.println(WiFi.macAddress());
}



struct Button
{
    const uint8_t PIN;
    bool pressed;
};

Button button1 = {13, false};

// variables to keep track of the timing of recent interrupts
unsigned long button_time = 0;
unsigned long last_button_time = 0;

void IRAM_ATTR isr()
{
    button_time = millis();
    if (button_time - last_button_time > 250)
    {
        button1.pressed = (button1.pressed == true) ? false : true;
        last_button_time = button_time;
    }
}

DHT20 DHT(&Wire);

MQ135 mq135_sensor(PIN_MQ135);
float THRESHOLD;

const int NoiseWindow = 50;
float temperature = 21.0; // Assume current temperature. Recommended to measure with DHT22
float humidity = 25.0;    // Assume current humidity. Recommended to measure with DHT22
bool MOTIONSTATE = false;
bool DIM = false;
int LIGHTMAX = 0;
int LIGHTMIN = 4096;
// put function definitions here:
void Calibration()
{
    int start = millis();
    int duration{0};

    while (duration < 5 * 1000)
    {
        pinMode(PIN_PIRSENSOR, INPUT);
        digitalWrite(PIN_LED, HIGH);
        delay(100);
        int cur{analogRead(PIN_LIGHTSENSOR)};
        if (cur > LIGHTMAX)
        {
            LIGHTMAX = cur;
        }
        else if (cur < LIGHTMIN)
        {
            LIGHTMIN = cur;
        }

        digitalWrite(PIN_LED, LOW);
        delay(100);
        duration = millis() - start;
    }
    LIGHTMAX == 0 ? THRESHOLD = 1 : THRESHOLD = LIGHTMAX * 0.8;
    Serial.print(THRESHOLD);
}

void measureMovement(int movement)
{
    if (movement == HIGH)
    {
        if (MOTIONSTATE == false)
        {
            Serial.println("Motion detected!");
            MOTIONSTATE = true;
        }
    }
    else
    {
        if (MOTIONSTATE == true)
        {
            Serial.println("Motion ended!");
            MOTIONSTATE = false;
        }
    }
}

/*
long measureLight(){
  return map(analogRead(PIN_LIGHTSENSOR) , LIGHTMAX, LIGHTMIN, 0, 255);
}
*/

unsigned measureNoise(int sampleWindow)
{
    unsigned long startMillis = millis();
    unsigned int signalMax = 0;
    unsigned int signalMin = 4096;

    // collect data for 50 mS and then plot data
    while (millis() - startMillis < sampleWindow)
    {
        unsigned int sample = analogRead(PIN_AMPSENSOR);
        if (sample < 4096) // toss out spurious readings
        {
            if (sample > signalMax)
            {
                signalMax = sample; // save just the max levels
            }
            else if (sample < signalMin)
            {
                signalMin = sample; // save just the min levels
            }
        }
    }
    int diff = signalMax - signalMin;
    return diff >= 0 ? diff : 0; // max - min = peak-peak amplitude
}

void setup()
{
    // put your setup code here, to run once:
    tft.init();
    tft.setRotation(3);
    Serial.begin(9600);
    pinMode(PIN_PIRSENSOR, INPUT);
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_LIGHTSENSOR, INPUT);
    pinMode(PIN_AMPSENSOR, INPUT);
    Wire.begin();
    attachInterrupt(button1.PIN, isr, FALLING);
    Calibration();
    connectToWiFi();
}

void measurePrintSent()
{
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 0);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);

    DHT.read();
    float humd = DHT.getHumidity();
    float temp = DHT.getTemperature();
    float rzero = mq135_sensor.getRZero();
    float correctedRZero = mq135_sensor.getCorrectedRZero(temperature, humidity);
    float resistance = mq135_sensor.getResistance();
    float ppm = mq135_sensor.getPPM();
    float correctedPPM = mq135_sensor.getCorrectedPPM(temperature, humidity);
    int light = analogRead(PIN_LIGHTSENSOR);
    int movement = digitalRead(PIN_PIRSENSOR);
    measureMovement(movement);
    unsigned int peakToPeak = measureNoise(NoiseWindow); // peak-to-peak level
    double volts = (peakToPeak * 3.3) / 4096;

    Serial.print("MQ135 RZero: ");
    Serial.print(rzero);
    Serial.print("\t Corrected RZero: ");
    Serial.print(correctedRZero);
    Serial.print("\t Resistance: ");
    Serial.print(resistance);
    Serial.print("\t PPM: ");
    Serial.print(ppm);
    Serial.print("\t Corrected PPM: ");
    Serial.print(correctedPPM);
    Serial.println("ppm");

    Serial.print("Humdity, Tempreture: ");
    Serial.print(humd, 1);
    Serial.print(", ");
    Serial.println(temp, 1);

    Serial.print("Noise Level:");
    Serial.println(volts);

    Serial.print("Light Level:");
    Serial.println(light);

    Serial.print("Movement:");
    Serial.println(MOTIONSTATE);

    Serial.println(analogRead(PIN_LIGHTSENSOR));
    Serial.println(analogRead(PIN_AMPSENSOR));
    Serial.println((THRESHOLD));
    Serial.println((DIM));

    tft.println("Pollutants: ");
    tft.print(correctedPPM);
    tft.println("ppm");
    tft.print("Humdity: ");
    tft.println(humd);
    tft.print("Tempreture: ");
    tft.print(temp);
    tft.println("C");
    tft.print("Noise Level: ");
    tft.println(volts);
    tft.print("Light Level: ");
    tft.println(light);
    tft.print("Movement: ");
    tft.println(MOTIONSTATE == 1 ? "True" : "False");

    std::string capsule {'(' + std::to_string(humd) + ',' + std::to_string(temp) + ','+
                        std::to_string(correctedPPM) + ',' + std::to_string(light) + ',' + std::to_string(movement) + 
                        ',' + std::to_string(volts) + ','+ std::to_string((millis() - StartTime)) + ')'};
    Canister += (capsule + ';');
    if (millis() >= (10*1000 + StartTime) or LEGACY == true)
    {
        int err = 0;
        std::string inquiry = "/?var="+ Canister;
        WiFiClient c;
        HttpClient http(c);
        // err = http.get(kHostname, kPath);
        err = http.post(SERVERIP, 5000, inquiry.c_str(), NULL);
        if (err == 0)
        {
            Serial.println("startedRequest ok");
            err = http.responseStatusCode();
            if (err >= 0)
            {
                Serial.print("Got status code: ");
                Serial.println(err);
                // Usually you'd check that the response code is 200 or a
                // similar "success" code (200-299) before carrying on,
                // but we'll print out whatever response we get
                err = http.skipResponseHeaders();
                if (err >= 0)
                {
                    int bodyLen = http.contentLength();
                    Serial.print("Content length is: ");
                    Serial.println(bodyLen);
                    Serial.println();
                    Serial.println("Body returned follows:");
                    // Now we've got to the body, so we can print it out
                    unsigned long timeoutStart = millis();
                    char c;
                    // Whilst we haven't timed out & haven't reached the end of the body
                    while ((http.connected() || http.available()) &&
                        ((millis() - timeoutStart) < kNetworkTimeout))
                    {
                        if (http.available())
                        {
                            c = http.read();
                            // Print out this character
                            Serial.print(c);
                            bodyLen--;
                            // We read something, reset the timeout counter
                            timeoutStart = millis();
                            Canister = "";
                            StartTime = millis();
                            LEGACY = false;
                        }
                        else
                        {
                            // We haven't got any data, so let's pause to allow some to arrive
                            LEGACY = true;
                        }
                    }
                }
                else
                {
                    Serial.print("Failed to skip response headers: ");
                    Serial.println(err);
                }
            }
            else
            {
                Serial.print("Getting response failed: ");
                Serial.println(err);
            }
        }
        else
        {
            Serial.print("Connect failed: ");
            Serial.println(err);
        }
        http.stop();
    }
}

void loop()
{
    if (analogRead(PIN_LIGHTSENSOR) < THRESHOLD && !DIM)
    {
        Serial.print("On");
        digitalWrite(PIN_LED, HIGH);
    }
    else
    {
        digitalWrite(PIN_LED, LOW);
    }
    measurePrintSent();
    delay(500);
}