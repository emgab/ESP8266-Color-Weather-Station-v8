/**The MIT License (MIT)
Copyright (c) 2015 by Daniel Eichhorn
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
See more at http://blog.squix.ch

Modifed to include configuration portal on-demand when the WeMos reset button is pressed twice.
    Modified by DK Fowler ... 20-Nov-2016
    
    This method works well on Wemos boards which have a single reset button on board. It avoids using a pin for launching the configuration portal.

    How It Works
    When the ESP8266 loses power all data in RAM is lost but when it is reset the contents of a small region of RAM is preserved. So when the device 
    starts up it checks this region of ram for a flag to see if it has been recently reset. If so it launches a configuration portal, if not it sets 
    the reset flag. After running for a while this flag is cleared so that it will only launch the configuration portal in response to closely spaced resets.

    Settings
    There are two values to be set in the sketch.

    DRD_TIMEOUT - Number of seconds to wait for the second reset. Set to 10 in the example.
    DRD_ADDRESS - The address in RTC RAM to store the flag. This memory must not be used for other purposes in the same sketch. Set to 0 in the example.

    This example, contributed by DataCute needs the Double Reset Detector library from https://github.com/datacute/DoubleResetDetector .

Modified by DK Fowler ... 22-Nov-2016
    Added code to allow configuration of additional parameters, including Weather Underground parameters (API key, language, city/PWS,
    and country), and UTC offset for adjusting time from GMT.
    Note:  This version works with the exception of the time-setting routines.  The TimeClient class is instantiated with the UTC offset, which
    is an issue when the UTC offset is obtained within the SETUP function (vs. globally; the local scope causes issues when the time is obtained
    later on update).  Rather than resolve this, I've moved to another version that uses a different method and library to get the date/time,
    but more importantly, also adjusts for DST.

Modified by DK Fowler ... 30-Nov-2016
    Added a number of customizations made by Neptune, as per the latest example on Github for the WeatherStation.  This hopefully addresses
    two issues from the last version:  the global scope for the TimeClient class (no longer used), and the automatic adjustment for DST.
    Following are comments from this release made by Neptune (though not all of these changes have been added here).
 * Customizations by Neptune (NeptuneEng on Twitter, Neptune2 on Github)
 *  
 *  Added Wifi Splash screen and credit to Squix78
 *  Modified progress bar to a thicker and symmetrical shape
 *  Replaced TimeClient with built-in lwip sntp client (no need for external ntp client library)
 *  Added Daylight Saving Time Auto adjuster with DST rules using simpleDSTadjust library
 *  https://github.com/neptune2/simpleDSTadjust
 *  Added Setting examples for Boston, Zurich and Sydney
 *  Selectable NTP servers for each locale
 *  DST rules and timezone settings customizable for each locale
 *  See https://www.timeanddate.com/time/change/ for DST rules
 *  Added AM/PM or 24-hour option for each locale
 *  Changed to 7-segment Clock font from http://www.keshikan.net/fonts-e.html   (***Note: font not implemented for the ILI9341 TFT display here...DKF)
 *  Added Forecast screen for days 4-6 (requires 1.1.3 or later version of esp8266_Weather_Station library)
 *  Added support for DHT22, DHT21 and DHT11 Indoor Temperature and Humidity Sensors
 *  Fixed bug preventing display.flipScreenVertically() from working
 *  Slight adjustment to overlay

Modified by DK Fowler ... 01-Dec-2016
    This version implements timers (tickers) for setting when updates are required for the current
    weather, forecast, and astronomy information.  This allows setting different frequencies for 
    the updates, as the forecast data only needs updating every couple hours, and the astronomy
    data only a couple of times each day.  This helps to cut down on the number of calls to the
    API for WeatherUnderground, which is restricted for free use.

    Also implemented a timer for scrolling through the various mid-panel displays, alowing for 
    cycling through 9-day forecast information (vs. the original 3-days).

    Finally, implemented a timer for scrolling through various bottom-panel displays, allowing for
    display of current observation details and detailed current forecast text (future) along with
    astronomy data.

Modified by DK Fowler ... 04-Dec-2016
    Added significant additional information and tweaks to the display.  Added routines to display
    panels for the forecast text for the next 3 forecast periods (36 hours or so).  Added feels-like
    temperature and weather station to the display.  Display of the forecast text required implementing
    a word-wrap function to allow cleanly breaking the text on the display panel.

Modified by DK Fowler ... 08-Dec-2016
    Added custom parameter to the configuration screen for WiFiManager to allow designating city locale for
    setting time zone and DST rules.  Added custom labels for the new input fields on the form, including
    a new header file (TimeZone.h) that displays several lines of input help for this field.  Note that
    the only current legitimate options for this field are for the US time-zones, though this can easily
    be extended for international locales/rules.  Also cleaned up a couple of other display bugs, including
    overwrites on the text for the progress bar for .BMP downloads.

    Finally, corrected counts for the downloaded icons (currrent 19 specified for the weather icons, large
    and small; 23 for the moon-phase icons.)
    
*/
#include <FS.h>
#include <Arduino.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h> // Hardware-specific library
#include <SPI.h>
// Additional UI functions
#include "GfxUi.h"

// Fonts created by http://oleddisplay.squix.ch/
#include "ArialRoundedMTBold_14.h"
#include "ArialRoundedMTBold_36.h"
#include "Droid_Sans_10.h"
#include "Droid_Sans_12.h"
//#include "DSEG7Classic-BoldFont.h"

// Download helper
#include "WebResource.h"

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// Helps with connecting to internet
#include <WiFiManager.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

// check settings.h for adapting to your needs
#include "settings.h"
#include <JsonListener.h>
#include <WundergroundClient.h>
#include "time.h"
#include <DoubleResetDetector.h>  //https://github.com/datacute/DoubleResetDetector
#include <simpleDSTadjust.h>
#include "TimeZone.h"

/*********************************************************
 * Important: see settings.h to configure your settings!!!
 * *******************************************************/

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

//flag for saving data
bool shouldSaveConfig = false;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
GfxUi ui = GfxUi(&tft);

// flag changed in the ticker function every 1 minute
bool readyForDHTUpdate = false;

WebResource webResource;

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);

// Initialize the temperature/ humidity sensor
DHT dht(DHTPIN, DHTTYPE);
float humidity = 0.0;
float temperature = 0.0;

// Instantiate tickers for updating current conditions, forecast, and astronomy info
volatile int forecastTimer  = 0;  // Part of workaround for ticker timer limit
volatile int astronomyTimer = 0;
Ticker ticker;
Ticker forecastTicker;
Ticker astronomyTicker;
Ticker midPanelTicker;
Ticker botPanelTicker;


//declaring prototypes
void configModeCallback (WiFiManager *myWiFiManager);
void saveConfigCallback ();
void downloadCallback(String filename, int16_t bytesDownloaded, int16_t bytesTotal);
ProgressCallback _downloadCallback = downloadCallback;
void downloadResources();
void updateData();
void setDSTRules(String TZCity);
void updateForecastData();
void updateAstronomyData();
void updateMidPanel();
void updateBotPanel();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawCurrentWeather();
void drawCurrentObservations(int16_t x, int16_t y);
void drawForecast();
void drawForecast2();
void drawForecast3();
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
String getMeteoconIcon(String iconText);
void drawAstronomy();
void drawForecastText(int forecastTextPeriod);
void drawSeparator(uint16_t y);
void setReadyForWeatherUpdate();
void setReadyForForecastUpdate();
void setReadyForAstronomyUpdate();
void setReadyForMidPanelUpdate();
void setReadyForBotPanelUpdate();
void stringWordWrap(String displayString);

long lastDownloadUpdate = millis();

void setup() {
  Serial.begin(115200);

    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_WUNDERGROUND_API_KEY("WUkey", "WU API key", WUNDERGROUND_API_KEY, 18, "<p>Weather Underground API Key</p");
    WiFiManagerParameter custom_WUNDERGROUND_LANGUAGE("WUlanguage", "WU language", WUNDERGROUND_LANGUAGE, 4, "<p>Weather Underground Language</p");
    WiFiManagerParameter custom_WUNDERGROUND_COUNTRY("WUcountry", "WU country", WUNDERGROUND_COUNTRY, 4, "<p>Weather Underground Country</p");
    WiFiManagerParameter custom_WUNDERGROUND_CITY("WUcity", "WU city/PWS", WUNDERGROUND_CITY, 20, "<p>Weather Underground City/PWS</p");
    WiFiManagerParameter custom_TZ_CITY("TZCity", "TZ City", TZ_CITY, 30, TimeZoneConfig);
  
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    // Uncomment following lines to delete the config.json file...
    // if (SPIFFS.remove("/config.json")) {
    //   Serial.println("***Successfully deleted config.json file...***");
    // } else {
    //   Serial.println("***...error deleting config.json file...***");
    // }
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(WUNDERGROUND_API_KEY, json["WUNDERGROUND_API_KEY"]);
          strcpy(WUNDERGROUND_LANGUAGE, json["WUNDERGROUND_LANGUAGE"]);
          strcpy(WUNDERGROUND_COUNTRY, json["WUNDERGROUND_COUNTRY"]);
          strcpy(WUNDERGROUND_CITY, json["WUNDERGROUND_CITY"]);
          Serial.println("Retrieved all WU variables, now attempting time zone...");
          strcpy(TZ_CITY, json["TZ_CITY"]);
          Serial.println("...finished...");
        } else {
          Serial.println("failed to load json config");
        }
      }
    } else {
      Serial.println("/config.json does not exist.");
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.setTextAlignment(CENTER);
//  ui.drawString(120, 160, "Connecting to WiFi");

  String hostname;
  hostname = "ESP" + String(ESP.getChipId(), HEX);
//  hostname.remove(3,1);
  Serial.print("Hostname:  "); Serial.println(hostname);
  
  if (WiFi.SSID()==""){
    Serial.println("No stored access-point credentials; initiating configuration portal.");   
    tft.fillScreen(ILI9341_BLACK);
    ui.drawString(120, 56, "No stored access-point credentials...");
    ui.drawString(120, 70, "Initiating configuration portal.");
    delay(1000);
    initialConfig = true;
  }
  if (drd.detectDoubleReset()) {
    Serial.println("Double-reset detected...");
    tft.fillScreen(ILI9341_BLACK);
    ui.drawString(120, 56, "Double-reset detected...");
    ui.drawString(120, 70, "Initiating configuration portal.");
    delay(1000);
    initialConfig = true;
  }
  if (initialConfig) {
    Serial.println("Starting configuration portal.");

    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

// Uncomment for testing wifi manager
//  wifiManager.resetSettings();
    wifiManager.setAPCallback(configModeCallback);

    //add all your parameters here
    wifiManager.addParameter(&custom_WUNDERGROUND_API_KEY);
    wifiManager.addParameter(&custom_WUNDERGROUND_LANGUAGE);
    wifiManager.addParameter(&custom_WUNDERGROUND_COUNTRY);
    wifiManager.addParameter(&custom_WUNDERGROUND_CITY);
    wifiManager.addParameter(&custom_TZ_CITY);

    //it starts an access point 
    //and goes into a blocking loop awaiting configuration
    
    // Serial.print("Portal: "); Serial.print(hostname); Serial.print("  pass:  "); Serial.println(configPortalPassword);
    if (!wifiManager.startConfigPortal(hostname.c_str(), configPortalPassword)) {
      Serial.println("Not connected to WiFi but continuing anyway.");
    } else {
      //if you get here you have connected to the WiFi
      Serial.println("Connected to WiFi.");

        //read updated parameters
      strcpy(WUNDERGROUND_API_KEY, custom_WUNDERGROUND_API_KEY.getValue());
      strcpy(WUNDERGROUND_LANGUAGE, custom_WUNDERGROUND_LANGUAGE.getValue());
      strcpy(WUNDERGROUND_COUNTRY, custom_WUNDERGROUND_COUNTRY.getValue());
      strcpy(WUNDERGROUND_CITY, custom_WUNDERGROUND_CITY.getValue());
      strcpy(TZ_CITY, custom_TZ_CITY.getValue());

  //save the custom parameters to FS
      if (shouldSaveConfig) {
        Serial.println("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();
        json["WUNDERGROUND_API_KEY"] = WUNDERGROUND_API_KEY;
        json["WUNDERGROUND_LANGUAGE"] = WUNDERGROUND_LANGUAGE;
        json["WUNDERGROUND_COUNTRY"] = WUNDERGROUND_COUNTRY;
        json["WUNDERGROUND_CITY"] = WUNDERGROUND_CITY;
        json["TZ_CITY"] = TZ_CITY;

        Serial.print("TZ City:  "); Serial.println(TZ_CITY);

        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile) {
          Serial.println("failed to open config file for writing");
        }

        json.printTo(Serial);
        json.printTo(configFile);
        Serial.println("fnished saving configuration file, closing");
        configFile.close();
      //end save

      }

    }
    ESP.reset(); // This is a bit crude. For some unknown reason webserver can only be started once per boot up 
                 // so resetting the device allows to go back into config mode again when it reboots.
    delay(5000);
  }

  WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
  unsigned long startedAt = millis();
  Serial.print("After waiting ");
  tft.fillScreen(ILI9341_BLACK);
  ui.drawString(120, 160, "Connecting to WiFi");

  int connRes = WiFi.waitForConnectResult();
  float waited = (millis()- startedAt);
  Serial.print(waited/1000);
  Serial.print(" secs in setup() connection result is ");
  Serial.println(connRes);
  if (WiFi.status()!=WL_CONNECTED){
    Serial.println("Failed to connect, finishing setup anyway.");
  } else {
    Serial.print("Connected...local ip: ");
  
    Serial.println(WiFi.localIP());
    tft.fillScreen(ILI9341_BLACK);
    ui.drawString(120, 160, "Connected...");
    delay(1000);
  }
  
  // OTA Setup
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
//  SPIFFS.begin();
  
  //Uncomment if you want to update all internet resources
  //SPIFFS.format();

  // download images from the net. If images already exist don't download
  downloadResources();

  // set the start/end rules for Daylight Savings Time based on the city selected
  setDSTRules(TZ_CITY);
  
  // load the weather information
  updateData();               // Current observations
  updateForecastData();       // 10-day forecast data
  updateAstronomyData();      // Moon phase, etc.
  
  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);            // Now for current observations update only
  midPanelTicker.attach(UPDATE_MID_PANEL_INTERVAL_SECS, setReadyForMidPanelUpdate); // To trigger updating mid-panel on screen
  botPanelTicker.attach(UPDATE_BOT_PANEL_INTERVAL_SECS, setReadyForBotPanelUpdate); // To trigger updating bottom-panel on screen

//  Note the triggering times (60 secs) for each of the following tickers.  This is a workaround for the
//  maximum timer limitation for the ticker function (approximately 71 minutes).
  forecastTicker.attach(60, setReadyForForecastUpdate);           // For controlling 10-day forecast update
  astronomyTicker.attach(60, setReadyForAstronomyUpdate);         // For controlling astronomy data update
  
}

long lastDrew = 0;
void loop() {
  // Handle OTA update requests
  ArduinoOTA.handle();

  // Call the double reset detector loop method every so often,
  // so that it can recognise when the timeout expires.
  // You can also call drd.stop() when you wish to no longer
  // consider the next reset as a double reset.
  drd.loop();

  // Check if we should update the clock
//  if (millis() - lastDrew > 30000 && wunderground.getSeconds() == "00") {
  if (millis() - lastDrew > 15000) {
    drawDateTime();
    lastDrew = millis();
  }

  // Check if we should update weather information
//  if (millis() - lastDownloadUpdate > 1000 * UPDATE_INTERVAL_SECS) {
  if (readyForWeatherUpdate) {
      updateData();
//      lastDownloadUpdate = millis();
  }
  if (readyForForecastUpdate) {
    updateForecastData();
  }

  if (readyForAstronomyUpdate) {
    updateAstronomyData();
  }

  if (readyForMidPanelUpdate) {
    updateMidPanel();
  }

  if (readyForBotPanelUpdate) {
    updateBotPanel();
  }

}

// Called if WiFi has not been configured yet
void configModeCallback (WiFiManager *myWiFiManager) {
  ui.setTextAlignment(CENTER);
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  tft.setTextColor(ILI9341_ORANGE);
  ui.drawString(120, 28, "Wifi Manager");
  ui.drawString(120, 42, "Please connect to AP");
  tft.setTextColor(ILI9341_WHITE);
  ui.drawString(120, 56, myWiFiManager->getConfigPortalSSID());
  // ui.drawString(120, 56, WiFi.hostname());
  tft.setTextColor(ILI9341_ORANGE);
  ui.drawString(120, 70, "To setup Wifi Configuration");
  tft.setFont(&Droid_Sans_12);
  tft.setTextColor(ILI9341_CYAN);
  ui.drawString(120, 98,  "If configuration page does not");
  ui.drawString(120, 112, "appear automatically, connect to");
  ui.drawString(120, 126, "address 192.168.4.1 in browser.");
}

// callback called during download of files. Updates progress bar
void downloadCallback(String filename, int16_t bytesDownloaded, int16_t bytesTotal) {
  Serial.println(String(bytesDownloaded) + " / " + String(bytesTotal));

  int percentage = 100 * bytesDownloaded / bytesTotal;
  if (percentage == 0) {
    ui.drawString(120, 160, filename);
  }
  if (percentage % 5 == 0) {
    ui.setTextAlignment(CENTER);
    ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
    //ui.drawString(120, 160, String(percentage) + "%");
    ui.drawProgressBar(10, 165, 240 - 20, 15, percentage, ILI9341_WHITE, ILI9341_BLUE);
  }

}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Download the bitmaps
void downloadResources() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  char id[5];
  for (int i = 0; i < 19; i++) {
    sprintf(id, "%02d", i);
    tft.fillRect(0, 120, 240, 45, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/wunderground/" + wundergroundIcons[i] + ".bmp", wundergroundIcons[i] + ".bmp", _downloadCallback);
  }
  for (int i = 0; i < 19; i++) {
    sprintf(id, "%02d", i);
    tft.fillRect(0, 120, 240, 45, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/wunderground/mini/" + wundergroundIcons[i] + ".bmp", "/mini/" + wundergroundIcons[i] + ".bmp", _downloadCallback);
  }
  for (int i = 0; i < 23; i++) {
    tft.fillRect(0, 120, 240, 45, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/moonphase_L" + String(i) + ".bmp", "/moon" + String(i) + ".bmp", _downloadCallback);
  }
}

// Update the internet based information and update screen
void updateData() {
//  tft.fillScreen(ILI9341_BLACK);        // This seems to sometimes leave the screen in an "all-bright" condition??  Maybe a power issue supplying TFT backlight??
  tft.setFont(&ArialRoundedMTBold_14);
  drawProgress(33, "Updating time...");
  delay(500);
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
  drawProgress(66, "Updating conditions...");
  wunderground.updateConditions(WUNDERGROUND_API_KEY, WUNDERGROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(100, "Done...");
  delay(1000);
//  tft.fillScreen(ILI9341_BLACK);              // This seems to sometimes leave the screen in an "all-bright" condition??
  drawDateTime();
  tft.fillRect(0, 63, 240, 63, ILI9341_BLACK);  // Wipe prior data in the "current weather" panel
  drawCurrentWeather();
  readyForMidPanelUpdate = true;                // Update mid-panel since progress bar overwrites this.
  readyForWeatherUpdate = false;
}

void updateMidPanel() {
  midPanelNumber++;
  if (midPanelNumber > maxMidPanels) midPanelNumber = 1;
  switch (midPanelNumber) {
    case 1:
      drawForecast();
      break;
    case 2:
      drawForecast2();
      break;
    case 3:
      drawForecast3();
      break;
  }
  readyForMidPanelUpdate = false;
}

void updateBotPanel() {
  botPanelNumber++;
  if (botPanelNumber > maxBotPanels) botPanelNumber = 1;
  switch (botPanelNumber) {
    case 1:
      drawAstronomy();
      break;
    case 2:
      drawCurrentObservations();
      break;
    case 3:
      drawForecastText(0);
      break;
    case 4:
      drawForecastText(1);
      break;
    case 5:
      drawForecastText(2);
      break;
  }
  readyForBotPanelUpdate = false;
}

void updateForecastData() {
  drawProgress(50, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGROUND_API_KEY, WUNDERGROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(100, "Done...");
  delay(1000);
  readyForMidPanelUpdate = true;       // Update mid-panel since progress bar overwrites this.
  readyForForecastUpdate = false;
}

void updateAstronomyData() {
  drawProgress(50, "Updating astronomy...");
  wunderground.updateAstronomy(WUNDERGROUND_API_KEY, WUNDERGROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(100, "Done...");
  delay(1000);
  drawAstronomy();
  readyForMidPanelUpdate = true;       // Update mid-panel since progress bar overwrites this.
  readyForAstronomyUpdate = false;
}

// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
  tft.setFont(&ArialRoundedMTBold_14); 
  ui.setTextAlignment(CENTER);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  tft.fillRect(0, 140, 240, 45, ILI9341_BLACK);
  ui.drawString(120, 160, text);
  ui.drawProgressBar(10, 165, 240 - 20, 15, percentage, ILI9341_WHITE, ILI9341_BLUE);
}

// Called every 1 minute
void updateDHT() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature(!IS_METRIC);
  readyForDHTUpdate = false;
}

void setDSTRules(String TZ_CITY) {
  if (TZ_CITY == "Boston") {
        UTC_OFFSET = -5;
        StartRule = (dstRule) {"EDT", Second, Sun, Mar, 2, 3600}; // Eastern Daylight time = UTC/GMT -4 hours
        EndRule = (dstRule) {"EST", First, Sun, Nov, 1, 0};       // Eastern Standard time = UTC/GMT -5 hour
        #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
        IS_METRIC = false;
  } else if (TZ_CITY == "Louisville") {
        UTC_OFFSET = -5;
        StartRule = (dstRule) {"EDT", Second, Sun, Mar, 2, 3600}; // Eastern Daylight time = UTC/GMT -4 hours
        EndRule = (dstRule) {"EST", First, Sun, Nov, 1, 0};       // Eastern Standard time = UTC/GMT -5 hour
        #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
        IS_METRIC = false;
  } else if (TZ_CITY == "Chicago") {
        UTC_OFFSET = -6;
        StartRule = (dstRule) {"CDT", Second, Sun, Mar, 2, 3600}; // Central Daylight time = UTC/GMT -5 hours
        EndRule = (dstRule) {"CST", First, Sun, Nov, 1, 0};       // Central Standard time = UTC/GMT -6 hour
        #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
        IS_METRIC = false;
  } else if (TZ_CITY == "Mountain") {
        UTC_OFFSET = -7;
        StartRule = (dstRule) {"MDT", Second, Sun, Mar, 2, 3600}; // Mountain Daylight time = UTC/GMT -6 hours
        EndRule = (dstRule) {"MST", First, Sun, Nov, 1, 0};       // Mountain Standard time = UTC/GMT -7 hour
        #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
        IS_METRIC = false;
  } else if (TZ_CITY == "Pacific") {
        UTC_OFFSET = -8;
        StartRule = (dstRule) {"PDT", Second, Sun, Mar, 2, 3600}; // Pacific Daylight time = UTC/GMT -7 hours
        EndRule = (dstRule) {"PST", First, Sun, Nov, 1, 0};       // Pacific Standard time = UTC/GMT -8 hour
        #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
        IS_METRIC = false;
  } else if (TZ_CITY == "Alaska") {
        UTC_OFFSET = -9;
        StartRule = (dstRule) {"ADT", Second, Sun, Mar, 2, 3600}; // Alaskan Daylight time = UTC/GMT -8 hours
        EndRule = (dstRule) {"AST", First, Sun, Nov, 1, 0};       // Alaskan Standard time = UTC/GMT -9 hour
        #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
        IS_METRIC = false;
  } else if (TZ_CITY == "Hawaii") {
        UTC_OFFSET = -10;
        StartRule = (dstRule) {"HDT", Second, Sun, Mar, 2, 3600}; // Hawaiian Daylight time = UTC/GMT -9 hours
        EndRule = (dstRule) {"HST", First, Sun, Nov, 1, 0};       // Hawaiian Standard time = UTC/GMT -10 hour
        #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
        IS_METRIC = false;
  } else if (TZ_CITY == "Zurich") {
        UTC_OFFSET = +1;
        StartRule = (dstRule) {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
        EndRule = (dstRule) {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour
        #define NTP_SERVERS "0.ch.pool.ntp.org", "1.ch.pool.ntp.org", "2.ch.pool.ntp.org"
        IS_METRIC = true;
  } else if (TZ_CITY == "Sydney") {
        UTC_OFFSET = +10;
        StartRule = (dstRule) {"AEDT", First, Sun, Oct, 2, 3600}; // Australia Eastern Daylight time = UTC/GMT +11 hours
        EndRule = (dstRule) {"AEST", First, Sun, Apr, 2, 0};      // Australia Eastern Standard time = UTC/GMT +10 hour
        #define NTP_SERVERS "0.au.pool.ntp.org", "1.au.pool.ntp.org", "2.au.pool.ntp.org"
        IS_METRIC = true;
  } else {
        UTC_OFFSET = 0;
        StartRule = (dstRule) {"GMT", Second, Sun, Mar, 2, 0};    // GMT default
        EndRule = (dstRule) {"GMT", First, Sun, Nov, 1, 0};
        #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
        IS_METRIC = false;
  }
}

void drawDateTime() {
  // Setup simpleDSTadjust Library rules
  simpleDSTadjust dstAdjusted(StartRule, EndRule);

  char *dstAbbrev;
  char time_str[11];
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  ui.setTextAlignment(CENTER);
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  String date = ctime(&now);
  date = date.substring(0,11) + String(1900+timeinfo->tm_year);
  ui.drawString(120, 20, date);
//  tft.setFont(&DSEG7_Classic_Bold_21);
  tft.setFont(&ArialRoundedMTBold_36);
  
#ifdef STYLE_24HR
  sprintf(time_str, "%02d:%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
//  tft.drawRect(36, 26, 164, 36, ILI9341_WHITE);
  tft.fillRect(36, 26, 164, 36, ILI9341_BLACK);
  ui.drawString(120, 56, time_str);
  drawSeparator(65);
#else
  int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
  sprintf(time_str, "%2d:%02d:%02d\n", hour, timeinfo->tm_min, timeinfo->tm_sec);
//  tft.drawRect(36, 26, 164, 36, ILI9341_WHITE);
  tft.fillRect(36, 26, 164, 36, ILI9341_BLACK);
  ui.drawString(120, 56, time_str);
  drawSeparator(65);
#endif

//  ui.setTextAlignment(RIGHT);
  ui.setTextAlignment(LEFT);
//  ui.setFont(&ArialMT_Plain_10);
  tft.setFont(&ArialRoundedMTBold_14);
#ifdef STYLE_24HR
  sprintf(time_str, "%s", dstAbbrev);
  ui.drawString(202, 40, time_str);
#else
  sprintf(time_str, "%s", dstAbbrev);
  ui.drawString(202, 40, time_str);
  sprintf(time_str, "%s", timeinfo->tm_hour>=12?"pm":"am");
  ui.drawString(202, 56, time_str);
#endif

// Weather Station
ui.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
ui.setTextAlignment(RIGHT);
tft.setFont(&Droid_Sans_10);
String temp = "PWS: " + String(WUNDERGROUND_CITY);
ui.drawString(220, 75, temp);

}

// draws current weather information
void drawCurrentWeather() {
  // Weather Icon
  String weatherIcon = getMeteoconIcon(wunderground.getTodayIcon());
  ui.drawBmp(weatherIcon + ".bmp", 0, 55);
  
  // Weather Text
  tft.setFont(&ArialRoundedMTBold_14);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.setTextAlignment(RIGHT);
  ui.drawString(220, 90, wunderground.getWeatherText());

  // Temperature display
  tft.setFont(&ArialRoundedMTBold_36);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.setTextAlignment(RIGHT);
  String degreeSign = "F";
  if (IS_METRIC) {
    degreeSign = "C";
  }
  String temp = wunderground.getCurrentTemp() + degreeSign;
  ui.drawString(220, 125, temp);
  drawSeparator(135);

  // Feels-like temperature display
  ui.setTextAlignment(RIGHT);
  tft.setFont(&Droid_Sans_10);
  temp = "Feels like: " + wunderground.getFeelsLike();
  ui.drawString(220, 138, temp);

}

// Draw current observations panel.
void drawCurrentObservations () {
  // In order to make it easy to adjust the position of this
  // display, the following x/y coordinates set the baseline
  // position for the remaining display lines.
  int x = 0;
  int y = 243;
  // The following control the current line/column/
  // font height and column width.  Adjust as desired
  // based on font used and column appearance.
  int fontHeight = 12;
  int fontWidth = 5;      // used to set position of dynamic information after labels
  int maxLabelWidth = 9;  // used as basedline for width of static lables
  int columnWidth = 120;
  int lineNumber = 1;
  int columnNumber = 1;
  
  tft.fillRect(0, 233, 240, 87, ILI9341_BLACK);
  ui.setTextAlignment(CENTER);
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&Droid_Sans_10);
  String temp = "Current Observations";
  ui.drawString(120, y, temp);
  lineNumber = lineNumber + 2;    // add blank line after heading

  ui.setTextAlignment(LEFT);
  temp = "Humid:  ";
  temp = temp;
  ui.drawString(x+(columnNumber-1), y+(fontHeight*(lineNumber-1)), temp);
  temp = wunderground.getHumidity();
  ui.drawString(x+(columnNumber-1)+(maxLabelWidth*fontWidth), y+(fontHeight*(lineNumber-1)), temp);
  lineNumber++;

  String windString;
  if (wunderground.getWindSpeed() == "0.0") {
    windString = "Calm";
  } else {
    windString = wunderground.getWindDir() + " " + wunderground.getWindSpeed(); 
  }
  temp = "Wind:   ";
  ui.drawString(x+(columnNumber-1), y+(fontHeight*(lineNumber-1)), temp);
  temp = windString;
  ui.drawString(x+(columnNumber-1)+(maxLabelWidth*fontWidth), y+(fontHeight*(lineNumber-1)), temp);
  lineNumber++;

  temp = "Precip: ";
  ui.drawString(x+(columnNumber-1), y+(fontHeight*(lineNumber-1)), temp);
  temp = wunderground.getPrecipitationToday();
  ui.drawString(x+(columnNumber-1)+(maxLabelWidth*fontWidth), y+(fontHeight*(lineNumber-1)), temp);
  lineNumber++;

  temp = "Moon:   ";
  ui.drawString(x+(columnNumber-1), y+(fontHeight*(lineNumber-1)), temp);
  temp = wunderground.getMoonPhase();
  ui.drawString(x+(columnNumber-1)+(maxLabelWidth*fontWidth), y+(fontHeight*(lineNumber-1)), temp);
  lineNumber++;

// Column 2...

  columnNumber++;
  lineNumber = 3;     // reset for second column

  temp = "Press: ";
  ui.drawString(x+((columnNumber-1)*columnWidth), y+(fontHeight*(lineNumber-1)), temp);
  temp = wunderground.getPressure();
  ui.drawString(x+((columnNumber-1)*columnWidth)+(maxLabelWidth*fontWidth), y+(fontHeight*(lineNumber-1)), temp);
  lineNumber++;

  temp = "UV:    ";
  ui.drawString(x+((columnNumber-1)*columnWidth), y+(fontHeight*(lineNumber-1)), temp);
  temp = wunderground.getUV();
  ui.drawString(x+((columnNumber-1)*columnWidth)+(maxLabelWidth*fontWidth), y+(fontHeight*(lineNumber-1)), temp);
  lineNumber++;

  temp = wunderground.getObservationTime();
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setFont(&Droid_Sans_10);
  ui.setTextAlignment(CENTER);
  temp = temp.substring(5);
  ui.drawString(120, 315, temp);

}

// draws the three forecast columns
void drawForecast() {
//  tft.drawRect(0, 165, 240, 50, ILI9341_WHITE);  // May leave this for future; adds a nice effect 
//  tft.drawRect(0, 150, 240, 82, ILI9341_WHITE);  // Frames the entire forecast area
  tft.fillRect(0, 150, 240, 82, ILI9341_BLACK);    // Clear prior forecast panel
  drawForecastDetail(10, 165, 0);
  drawForecastDetail(95, 165, 2);
  drawForecastDetail(178, 165, 4);
  drawSeparator(165 + 65 + 10);
}

void drawForecast2() {
//  tft.drawRect(0, 165, 240, 50, ILI9341_WHITE);  // May leave this for future; adds a nice effect 
//  tft.drawRect(0, 150, 240, 82, ILI9341_WHITE);
  tft.fillRect(0, 150, 240, 82, ILI9341_BLACK);   // Clear prior forecast panel

  drawForecastDetail(10, 165, 6);
  drawForecastDetail(95, 165, 8);
  drawForecastDetail(178, 165, 10);
  drawSeparator(165 + 65 + 10);
}

void drawForecast3() {
//  tft.drawRect(0, 165, 240, 50, ILI9341_WHITE);  // May leave this for future; adds a nice effect
//  tft.drawRect(0, 150, 240, 82, ILI9341_WHITE);
  tft.fillRect(0, 150, 240, 82, ILI9341_BLACK);   // Clear prior forecast panel

  drawForecastDetail(10, 165, 12);
  drawForecastDetail(95, 165, 14);
  drawForecastDetail(178, 165, 16);
  drawSeparator(165 + 65 + 10);
}

// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
//  tft.setFont(&ArialRoundedMTBold_14);
  tft.setFont(&Droid_Sans_12);
  ui.setTextAlignment(CENTER);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  String forecastDate = wunderground.getForecastMonth(dayIndex/2) + "/" + 
                        wunderground.getForecastDay(dayIndex/2);
  String dayDate = day + "-" + forecastDate;
  ui.drawString(x + 25, y, dayDate);

  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(x + 25, y + 14, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  
  String weatherIcon = getMeteoconIcon(wunderground.getForecastIcon(dayIndex));
  ui.drawBmp("/mini/" + weatherIcon + ".bmp", x, y + 15);
    
}

// draw moonphase and sunrise/set and moonrise/set
void drawAstronomy() {
  tft.fillRect(0, 233, 240, 87, ILI9341_BLACK);
  int moonAgeImage = 24 * wunderground.getMoonAge().toInt() / 30.0;
  Serial.print("Moon age image:  "); Serial.println(moonAgeImage);
  ui.drawBmp("/moon" + String(moonAgeImage) + ".bmp", 120 - 30, 250);
  
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);  
  ui.setTextAlignment(LEFT);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.drawString(20, 270, "Sun");
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(20, 285, wunderground.getSunriseTime());
  ui.drawString(20, 300, wunderground.getSunsetTime());

  ui.setTextAlignment(RIGHT);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.drawString(220, 270, "Moon");
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(220, 285, wunderground.getMoonriseTime());
  ui.drawString(220, 300, wunderground.getMoonsetTime());

//  tft.drawRect(0, 233, 240, 80, ILI9341_WHITE);
  
}

// display current forecast text
void drawForecastText(int forecastTextPeriod) {
  tft.fillRect(0, 231, 240, 87, ILI9341_BLACK);
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&Droid_Sans_10);  
  ui.setTextAlignment(LEFT);
  tft.setTextWrap(true);
  String temp;

  tft.setCursor(0,245);
  temp = wunderground.getForecastTitle(forecastTextPeriod) + ":";             // Current period
  tft.println(temp);
  temp = wunderground.getForecastText(forecastTextPeriod);
  stringWordWrap(temp);

}
  
// Helper function, should be part of the weather station library and should disappear soon
String getMeteoconIcon(String iconText) {
  if (iconText == "F") return "chanceflurries";
  if (iconText == "Q") return "chancerain";
  if (iconText == "W") return "chancesleet";
  if (iconText == "V") return "chancesnow";
  if (iconText == "S") return "chancetstorms";
  if (iconText == "B") return "clear";
  if (iconText == "Y") return "cloudy";
  if (iconText == "F") return "flurries";
  if (iconText == "M") return "fog";
  if (iconText == "E") return "hazy";
  if (iconText == "Y") return "mostlycloudy";
  if (iconText == "H") return "mostlysunny";
  if (iconText == "H") return "partlycloudy";
  if (iconText == "J") return "partlysunny";
  if (iconText == "W") return "sleet";
  if (iconText == "R") return "rain";
  if (iconText == "W") return "snow";
  if (iconText == "B") return "sunny";
  if (iconText == "0") return "tstorms";
  

  return "unknown";
}

// if you want separators, uncomment the tft-line
void drawSeparator(uint16_t y) {
   //tft.drawFastHLine(10, y, 240 - 2 * 10, 0x4228);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void setReadyForForecastUpdate() {
/*
    This routine is triggered every 60 secs to increment and check to see
    if the 10-day forecast interval has been reached, triggering an update
    of the forecast information. 
*/
  forecastTimer++;
//  Serial.print("***Checking 10-day forecast update timer, timer="); Serial.println(forecastTimer);
  if (forecastTimer >= (UPDATE_INTERVAL_FORECAST_SECS / 60)) {
    Serial.println("Setting readyForForecastUpdate to true");
    readyForForecastUpdate = true;
    forecastTimer = 0;
  }
}

void setReadyForAstronomyUpdate() {
/*
    This routine is triggered every 60 secs to increment and check to see
    if the astronomy data update interval has been reached, triggering an update
    of this information. 
*/
  astronomyTimer++;
//  Serial.print("***Checking astronomy update timer, timer="); Serial.println(astronomyTimer);
  if (astronomyTimer >= (UPDATE_INTERVAL_ASTRO_SECS / 60)) {
    Serial.println("Setting readyForAstronomyUpdate to true");
    readyForAstronomyUpdate = true;
    astronomyTimer = 0;
  }
}

void setReadyForMidPanelUpdate() {
  Serial.println("Setting readyForMidPanelUpdate to true");
  readyForMidPanelUpdate = true;
}

void setReadyForBotPanelUpdate() {
  Serial.println("Setting readyForBotPanelUpdate to true");
  readyForBotPanelUpdate = true;
}

// Routine to perform word-wrap display of text string
void stringWordWrap(String displayString) {
  // The approach in this routine is as follows:
  //    1) Estimate the possible number of chars that can be displayed per line with the
  //       current font.  This is done by first calculating the width in pixels of a 10-character
  //       sample string using the current font.
  //    2) Divide the maximum pixels per line by the average pixel width/char.
  //    3) Now begin scanning the display string starting at the maximum character position per line,
  //       moving backwards from there looking for the first space.  If the char is a space,
  //       then this is an acceptable place to break the line, so display it.
  //    4) Otherwise, keep moving backwards until a space is found; break here and display the
  //       segment.
  //    5) Rinse and repeat starting with the character following the break, through the end of
  //       the display string.
  //    6) Of course, need to handle null strings and lines with no break-points (break anyway).
  // 
  char sampleString[] = "aaaaaaaaaa";  // Adjust sample string as appropriate based on type of typical data
  int16_t x1, y1;
  uint16_t w, h, x, y;
  int breakLocation;

//  Serial.println("*** stringWordWrap called ***");
  if (displayString == "") return;     // Just return and do nothing if the passed display line is null.
  x = 0; y = 20;                      // Sample coordinates used to calculate the max chars per line.
  tft.getTextBounds(sampleString, x, y, &x1, &y1, &w, &h);
  float pixelsChar = w/10;            // 10 characters in the sample string.
//  Serial.print("Sample string width (pixels):  "); Serial.println(w);
//  Serial.print("Average number of pixels/char (w):  "); Serial.println(pixelsChar,3);

  int charsPerLine = (240 / pixelsChar) - 1;
  int estBreak = charsPerLine;
  int beginLineIndex = 0;

  int displayStringLen = displayString.length();
//  Serial.println("***Beginning word wrap of passed string***");
//  Serial.print("Length of passed string:  "); Serial.println(displayStringLen);
//  Serial.print("Calc chars / line:  "); Serial.println(charsPerLine);
  while (estBreak < displayStringLen) {
    // Search backwards from the estimated line break for the first space character
//    Serial.print("Beginning line index:  "); Serial.println(beginLineIndex);
//    Serial.print("Estimated break location:  "); Serial.println(estBreak);
    breakLocation = (displayString.substring(beginLineIndex,estBreak)).lastIndexOf(" ",estBreak);
//    Serial.print("Found break location at character:  "); Serial.println(breakLocation);
    // If no space is found in the line segment, force a break at the max chars / line position.
    if (breakLocation == -1) {
      breakLocation = estBreak;
    } else {
      breakLocation = breakLocation + beginLineIndex;
    }
    tft.println(displayString.substring(beginLineIndex,breakLocation));
//    Serial.print("["); Serial.print(displayString.substring(beginLineIndex,breakLocation)); Serial.println("]");
    beginLineIndex = breakLocation + 1;
    estBreak = breakLocation + charsPerLine; 
  }
  if (estBreak > displayStringLen) {
    // Display the last string segment
    tft.println(displayString.substring(beginLineIndex));
//    Serial.print("["); Serial.print(displayString.substring(beginLineIndex)); Serial.println("]");
  }
} 

