
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define SSD1306
//#define SH110X


#if defined SSD1306 && defined SH110X
#error "only one displaytype allowed"
#endif

#include <Ticker.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#if defined(SSD1306)
#include <Adafruit_SSD1306.h>
#else
#include <Adafruit_SH110X.h>
#endif
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Adafruit_NeoPixel.h>
#include "bsec.h"
#include "SparkFun_SCD30_Arduino_Library.h"
#include "Color.h"
#include "icons.h"

const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};

Color iaqLevel1(0,228,0);
Color iaqLevel2(146,208,80);
Color iaqLevel3(255,255,0);
Color iaqLevel4(255,126,0);
Color iaqLevel5(255,0,0);
Color iaqLevel6(153,0,76);
Color iaqLevel7(102,51,0);

Color co2Level1(0,228,0);
Color co2Level2(0,154,53);
Color co2Level3(146,194,24);
Color co2Level4(254,215,0);
Color co2Level5(238,98,36);
Color co2Level6(232,38,46);

#define BME680_I2C_ADDR BME680_I2C_ADDR_PRIMARY  // BME680_I2C_ADDR_PRIMARY or  BME680_I2C_ADDR_SECONDARY 

#define STATE_SAVE_PERIOD    UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day

void checkIaqSensorStatus(void);
void loadState(void);
void updateState(void);
void displayMeasurements();

uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;

#define LED_PIN        2
#define ALTITUDE       330

Bsec             iaqSensor;
SCD30            airSensor;

#if defined(SSD1306)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
#define OLED_WHITE SSD1306_WHITE
#elif defined(SH110X)
Adafruit_SH110X  display(SCREEN_HEIGHT, SCREEN_WIDTH, &Wire);
#define OLED_WHITE SH110X_WHITE
#endif

Adafruit_NeoPixel strip(2, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long          previousMillis  = 0;

Ticker currentDisplayTicker;

// used to store the data that should be displayed
struct displayData_t {
    float bme680Temperature;
    float bme680Humidity;
    float bme680Pressure;
    float bme680Iaq;
    float bme680StaticIaq;
    float bme680BreathVocEquivalent;
    float bme680Co2Equivalent;
    float scd30Temperature;
    float scd30Humidity;
    float scd30Co2;
};    

volatile displayData_t displayData = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

const int     displaysCount      = 5;
volatile int  currentDisplayIndx = 0;
volatile bool readyForDisplay    = true;

const int icon_data_pos_x = 0;  
const int icon_data_pos_y = 1;  
const int data_pos_x = 30;

void displayTemperature() {
    display.setFont(&FreeSans18pt7b);
    display.setCursor(data_pos_x,(display.height() + 24) / 2);
    display.setTextSize(1);
    display.print((displayData.bme680Temperature + displayData.scd30Temperature) / 2,1);
    display.drawBitmap(icon_data_pos_x,icon_data_pos_y, temperature_icon_bmp, temperature_icon_width, temperature_icon_height, OLED_WHITE); 
    display.setFont();
}

void displayHumidity() {
    display.setFont(&FreeSans18pt7b);
    display.setCursor(data_pos_x,(display.height() + 24) / 2);
    display.setTextSize(1);
    display.print((displayData.bme680Humidity + displayData.scd30Humidity) / 2,0);
    display.drawBitmap(icon_data_pos_x,icon_data_pos_y, humidity_icon_bmp, humidity_icon_width, humidity_icon_height, OLED_WHITE); 
    display.setFont();
}

void displayPressure() {
    display.setFont(&FreeSans18pt7b);
    display.setCursor(data_pos_x,(display.height() + 24) / 2);
    display.setTextSize(1);
    display.print(getSeaLevelPressure(displayData.bme680Pressure),0);
    display.drawBitmap(icon_data_pos_x,icon_data_pos_y, pressure_icon_bmp, pressure_icon_width, pressure_icon_height, OLED_WHITE); 
    display.setFont();
}

void displayStaticIaq() {
    display.setFont(&FreeSans18pt7b);
    display.setCursor(data_pos_x,(display.height() + 24) / 2);
    display.setTextSize(1);
    display.print(displayData.bme680StaticIaq,0);
    display.drawBitmap(icon_data_pos_x,icon_data_pos_y, iaq_icon_bmp, iaq_icon_width, iaq_icon_height, OLED_WHITE); 
    display.setFont();
}

void displayCO2() {
    display.setFont(&FreeSans18pt7b);
    display.setCursor(data_pos_x,(display.height() + 24) / 2);
    display.setTextSize(1);
    display.print(displayData.scd30Co2,0);
    display.drawBitmap(icon_data_pos_x,icon_data_pos_y, co2_icon_bmp, co2_icon_width, co2_icon_height, OLED_WHITE); 
    display.setFont();
}

void (*displays[displaysCount])(void)= {displayTemperature, displayHumidity, displayPressure, displayStaticIaq, displayCO2};

void updateDisplay() {

    display.clearDisplay();

    Serial.println(currentDisplayIndx);
    
    (*displays[currentDisplayIndx])();    
        
    display.display();   
}

void errLeds(void)
{
    strip.setPixelColor(0, strip.Color(0,255,0)); 
    strip.show();
    delay(100);
    strip.setPixelColor(0, strip.Color(0,0,0)); 
    strip.show();
    delay(100);
}

void setup(void) {
    EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1); // 1st address for the length
    Serial.begin(115200);
    while (!Serial) delay(10);

    strip.begin();          
    strip.show();           
    strip.setBrightness(10);

    Wire.begin();

    delay(50); // give the display some time on startup (https://learn.adafruit.com/monochrome-oled-breakouts/troubleshooting-2)
#if defined(SSD1306)
    display.begin(SSD1306_SWITCHCAPVCC, 0x3D,true);
    display.setRotation(2);    
#elif defined(SH110X)
    display.begin(0x3C,true);
    display.setRotation(1);    
#endif



    display.setTextColor(OLED_WHITE);
    display.clearDisplay();
    display.display();
  
  //display.setFont(&FreeMono9pt7b);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("CO2 Alerter 1.0");
    display.display();

    iaqSensor.begin(BME680_I2C_ADDR, Wire);

    display.println("BSEC: " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix));
    display.display();

    checkIaqSensorStatus();

    iaqSensor.setConfig(bsec_config_iaq);
    checkIaqSensorStatus();

    loadState();

    bsec_virtual_sensor_t sensorList[10] = {
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };

    iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
    checkIaqSensorStatus();

    if (airSensor.begin() == false) {
        display.println("Air sensor not detected. Please check wiring. Freezing...");
        display.display();
        errLeds();
    }

    airSensor.setMeasurementInterval(10);
    airSensor.setAltitudeCompensation(330);
    
    currentDisplayTicker.attach(2,nextDisplay);
}

void loop(void) {

    unsigned long currentMillis = millis();
    
    if (iaqSensor.run()) { // If new data is available

        displayData.bme680Temperature         = iaqSensor.temperature;
        displayData.bme680Humidity            = iaqSensor.humidity;
        displayData.bme680Pressure            = iaqSensor.pressure / 100.0f;
        displayData.bme680Iaq                 = iaqSensor.iaq;
        displayData.bme680StaticIaq           = iaqSensor.staticIaq;
        displayData.bme680BreathVocEquivalent = iaqSensor.breathVocEquivalent;
        displayData.bme680Co2Equivalent       = iaqSensor.co2Equivalent;

        airSensor.setAmbientPressure(displayData.bme680Pressure);
        
        updateState();
    } else {
        checkIaqSensorStatus();
    }    

    if (currentMillis - previousMillis >= 5000) {
        if (airSensor.dataAvailable()) {

            displayData.scd30Temperature = airSensor.getTemperature();
            displayData.scd30Humidity    = airSensor.getHumidity();
            displayData.scd30Co2         = airSensor.getCO2();
        }

        strip.setPixelColor(0, getIAQColor(displayData.bme680StaticIaq));
        strip.setPixelColor(1, getCO2Color(displayData.scd30Co2));
        strip.show();
        
        previousMillis = currentMillis;    
    }

    if (readyForDisplay) {
        updateDisplay();
        readyForDisplay = false;
    }
}

/**
 * Functions used by the different ticker
 */
void nextDisplay() {
    currentDisplayIndx = (currentDisplayIndx + 1) % displaysCount;
    readyForDisplay    = true;
}


// Helper function definitions
void checkIaqSensorStatus(void) {
    
    if (iaqSensor.status != BSEC_OK) {
        display.clearDisplay();
        display.setCursor(0, 0);
        if (iaqSensor.status < BSEC_OK) {
            display.print("BSEC error code : "); display.println(iaqSensor.status);
            display.display();
            for (;;)
                errLeds(); /* Halt in case of failure */
        } else {
            display.print("BSEC warning code : "); display.println(iaqSensor.status);
            display.display();
        }
    }

    if (iaqSensor.bme680Status != BME680_OK) {
        display.clearDisplay();
        display.setCursor(0, 0);
        if (iaqSensor.bme680Status < BME680_OK) {          
            display.print("BME680 error code : "); display.println(iaqSensor.bme680Status);
            display.display();      
            for (;;)
                errLeds(); /* Halt in case of failure */
        } else {
            display.print("BME680 warning code : "); display.println(iaqSensor.bme680Status);
            display.display();
        }
    }
    iaqSensor.status = BSEC_OK;
}

void loadState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
    // Existing state in EEPROM
    Serial.println("Reading state from EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
      bsecState[i] = EEPROM.read(i + 1);
      Serial.println(bsecState[i], HEX);
    }

    iaqSensor.setState(bsecState);
    checkIaqSensorStatus();
  } else {
    // Erase the EEPROM with zeroes
    Serial.println("Erasing EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
  }
}

void updateState(void)
{
  bool update = false;
  /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
  if (stateUpdateCounter == 0) {
    if (iaqSensor.iaqAccuracy >= 3) {
      update = true;
      stateUpdateCounter++;
    }
  } else {
    /* Update every STATE_SAVE_PERIOD milliseconds */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update) {
    iaqSensor.getState(bsecState);
    checkIaqSensorStatus();

    Serial.println("Writing state to EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
      EEPROM.write(i + 1, bsecState[i]);

      Serial.println(bsecState[i], HEX);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }
}

float mapValue(float value, float in_min, float in_max) {
  return (value - in_min) / (in_max - in_min);
}

/**
   gets a color value for the given IAQ value
*/
uint32_t getIAQColor(float iaqValue) {

    Color color1;
    Color color2;

    float normalizedValue = 0.0;

    // determine color one and color two
    if (iaqValue <= 50) {

        color1 = iaqLevel1;
        color2 = iaqLevel2;
        
        normalizedValue = mapValue(iaqValue, 0, 50);
    } else if (iaqValue > 50 && iaqValue <= 100) {

        color1 = iaqLevel2;
        color2 = iaqLevel3;
        
        normalizedValue = mapValue(iaqValue, 51, 100);
    } else if (iaqValue > 100 && iaqValue <= 150) {

        color1 = iaqLevel3;
        color2 = iaqLevel4;

        normalizedValue = mapValue(iaqValue, 101, 150);
    } else if (iaqValue > 150 && iaqValue <= 200) {
        
        color1 = iaqLevel4;
        color2 = iaqLevel5;

        normalizedValue = mapValue(iaqValue, 151, 200);
    } else if (iaqValue > 200 && iaqValue <= 250) {
        
        color1 = iaqLevel5;
        color2 = iaqLevel6;

        normalizedValue = mapValue(iaqValue, 201, 250);
    } else if (iaqValue > 250 && iaqValue <= 350) {
        color1 = iaqLevel6;
        color2 = iaqLevel7;

        normalizedValue = mapValue(iaqValue, 251, 350);
    } else {
        color1 = iaqLevel7;
        color2 = iaqLevel7;
    }

    const Color c = Color::interpolate(color1,color2,normalizedValue);
    return Adafruit_NeoPixel::gamma32(c.getPackedColorGRB());
}

/**
   gets a color value for the given CO2 value
*/
uint32_t getCO2Color(float co2Value) {

    Color color1;
    Color color2;

    float normalizedValue = 0.0;

    // determine color one and color two
    if (co2Value <= 650) {
        
        color1 = co2Level1;
        color2 = co2Level2;
        
        normalizedValue = mapValue(co2Value, 0, 650);
    } else if (co2Value > 650 && co2Value <= 950) {
        color1 = co2Level2;
        color2 = co2Level3;
        
        normalizedValue = mapValue(co2Value, 650, 950);
    } else if (co2Value > 950 && co2Value <= 1250) {
        color1 = co2Level3;
        color2 = co2Level4;
        
        normalizedValue = mapValue(co2Value, 950, 1250);
    } else if (co2Value > 1250 && co2Value <= 1500) {
        color1 = co2Level4;
        color2 = co2Level5;
        
        normalizedValue = mapValue(co2Value, 1250, 1500);
    } else if (co2Value > 1500 && co2Value <= 1850) {
        color1 = co2Level5;
        color2 = co2Level6;
        
        normalizedValue = mapValue(co2Value, 1500, 1850);
    } else {
        color1 = co2Level6;
        color2 = co2Level6;
    }

    const Color c = Color::interpolate(color1,color2,normalizedValue);
    
    return Adafruit_NeoPixel::gamma32(c.getPackedColorGRB());
}

float getSeaLevelPressure(float atmosphericPressure) {
    return atmosphericPressure / pow(1.0 - (ALTITUDE / 44330.0), 5.255);
}
