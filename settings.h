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
*/
#include "DHT.h"
#include <simpleDSTadjust.h>

// DHT Settings
// Uncomment whatever type you're using!

// #define DHTPIN D2 // NodeMCU
#define DHTPIN D8 // Wemos D1R2 Mini (DHT not currently implemented here)

//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321 (DHT not currently implemented here)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#if DHTTYPE == DHT22
#define DHTTEXT "DHT22"
#elif DHTTYPE == DHT21
#define DHTTEXT "DHT21"
#elif DHTTYPE == DHT11
#define DHTTEXT "DHT11"
#endif
char FormattedTemperature[10];
char FormattedHumidity[10];

// -----------------------------------
// Example Locales (uncomment only 1)
//#define Zurich
//#define Boston
//#define Louisville
//#define Sydney
//------------------------------------

//#ifdef Zurich
//DST rules for Central European Time Zone
//#define UTC_OFFSET +1
//struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
//struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour

// Uncomment for 24 Hour style clock
//#define STYLE_24HR

//#define NTP_SERVERS "0.ch.pool.ntp.org", "1.ch.pool.ntp.org", "2.ch.pool.ntp.org"

//#endif

// *** The following are default values for the configuration...
//      DST rules for US Eastern Time Zone (New York, Boston, Louisville)
int UTC_OFFSET = -5;
dstRule StartRule = {"EDT", Second, Sun, Mar, 2, 3600}; // Eastern Daylight time = UTC/GMT -4 hours;
dstRule EndRule = {"EST", First, Sun, Nov, 1, 0};       // Eastern Standard time = UTC/GMT -5 hour;

// Uncomment for 24 Hour style clock
//#define STYLE_24HR

#define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"

// Wunderground Settings (default)
boolean IS_METRIC = false;
char WUNDERGROUND_API_KEY[18] = "<WU API key>";
char WUNDERGROUND_LANGUAGE[4] = "EN";
char WUNDERGROUND_COUNTRY[4] = "US";
char WUNDERGROUND_CITY[20] = "<WU City>";
char TZ_CITY[30] = "Louisville";

//#ifdef Sydney
//DST Rules for Australia Eastern Time Zone (Sydney)
//#define UTC_OFFSET +10
//struct dstRule StartRule = {"AEDT", First, Sun, Oct, 2, 3600}; // Australia Eastern Daylight time = UTC/GMT +11 hours
//struct dstRule EndRule = {"AEST", First, Sun, Apr, 2, 0};      // Australia Eastern Standard time = UTC/GMT +10 hour

// Uncomment for 24 Hour style clock
//#define STYLE_24HR

//#define NTP_SERVERS "0.au.pool.ntp.org", "1.au.pool.ntp.org", "2.au.pool.ntp.org"

// Wunderground Settings
//const boolean IS_METRIC = true;
//const String WUNDERGRROUND_API_KEY = "<WUNDERGROUND KEY HERE>";
//const String WUNDERGRROUND_LANGUAGE = "EN";
//const String WUNDERGROUND_COUNTRY = "AU";
//const String WUNDERGROUND_CITY = "Sydney";
//#endif

//Thingspeak Settings
//const String THINGSPEAK_CHANNEL_ID = "67284";
//const String THINGSPEAK_API_READ_KEY = "L2VIW20QVNZJBLAK";

//  Various time constants used for updating
const int UPDATE_INTERVAL_SECS = 10 * 60;                // Update current observations every 10 minutes
const int UPDATE_INTERVAL_FORECAST_SECS = 2 * 60 * 60;   // Update forecast every 2 hours
const int UPDATE_INTERVAL_ASTRO_SECS = 12 * 60 * 60;     // Update astronomy data 2 times daily
const int UPDATE_MID_PANEL_INTERVAL_SECS = 6;            // Update mid-panel every 6 seconds
const int UPDATE_BOT_PANEL_INTERVAL_SECS = 10;           // Update bottom-panel every 15 seconds

// flags changed in the ticker functions based on frequency above
bool readyForWeatherUpdate = false;
bool readyForForecastUpdate = false;
bool readyForAstronomyUpdate = false;
bool readyForMidPanelUpdate = false;
bool readyForBotPanelUpdate = false;

// Pins for the ILI9341
#define TFT_DC 2
#define TFT_CS 5

// For controlling rotating mid-panel
int midPanelNumber = 0;
int maxMidPanels = 3;

// For controlling rotating bottom-panel
int botPanelNumber = 0;
int maxBotPanels = 5;

// List, so that the downloader knows what to fetch
String wundergroundIcons [] = {"chanceflurries","chancerain","chancesleet","chancesnow","clear","cloudy","flurries","fog","hazy","mostlycloudy","mostlysunny","partlycloudy","partlysunny","rain","sleet","snow","sunny","tstorms","unknown"};

/***************************
 * End Settings
 **************************/