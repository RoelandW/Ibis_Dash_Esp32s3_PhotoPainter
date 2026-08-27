// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║                    🦤 IBIS DASH - USER READY VERSION 🦤                      ║
// ║                                                                              ║
// ║  VERSION 5.1.1 - SMART REFRESH + CYCLING SPORTS                                ║
// ║  • NEW: No unnecessary screen redraws (only draws when data changes)         ║
// ║  • NEW: Goal-exceeded display (+% and +km above goal)                        ║
// ║  • NEW: Cycling mode auto-includes all ride variants                       ║
// ║  • NEW: Cycling shows Average Speed instead of Pace                          ║
// ║  • USB Composite Device (CDC + HID) prevents PC power management             ║
// ║  • Board identifies as "Ibis Dash" in Device Manager!                        ║
// ║  • NEVER sleeps on USB - foolproof triple verification                       ║
// ║  • Ignores false low battery readings on USB                                 ║
// ║                                                                              ║
// ║  CHANGES FROM V4.0:                                                          ║
// ║  1. Prevent unnecessary redraws: screen only updates when new activity       ║
// ║     detected. Button press always redraws.                                   ║
// ║  2. Goal exceeded: shows +% and +km instead of "X km to go"                 ║
// ║  3. Cycling: sport=Ride auto-includes Ride, VirtualRide, EBikeRide,          ║
// ║     GravelRide, MountainBikeRide, Handcycle, Velomobile. Latest activity     ║
// ║     shows Average Speed (km/h) instead of Pace.                              ║
// ║                                                                              ║
// ║  This version has NO personal credentials - ready for end users!             ║
// ║  All configuration is stored in NVS (flash) and set via Ibis Setup app.      ║
// ║                                                                              ║
// ║  FIRST TIME SETUP:                                                           ║
// ║  1. Upload this sketch to your ESP32-S3-PhotoPainter                         ║
// ║  2. You'll see the setup screen with ibis logos                              ║
// ║  3. Connect board with USB-C and open the Ibis Setup app                     ║
// ║  4. Configure WiFi + Garmin Middleware and save                            ║
// ║  5. The board becomes a Garmin dashboard!                                   ║
// ║                                                                              ║
// ║  USB IDENTITY (how PCs see this board):                                      ║
// ║  • Product Name: "Ibis Dash"                                                 ║
// ║  • Manufacturer: "Ibis"                                                      ║
// ║  • Shows as composite device: USBSerial Port + HID                           ║
// ║  • PCs will NOT aggressively power-manage this device                        ║
// ║                                                                              ║
// ║  ARDUINO IDE SETTINGS:                                                       ║
// ║  >> Board: ESP32S3 Dev Module                                                ║
// ║  >> Flash Mode: DIO (NOT OPI!)                                               ║
// ║  >> USB CDC On Boot: "Disabled"                                              ║
// ║  >> USB Mode: "USB-OTG (TinyUSB)"                                            ║
// ║                                                                              ║
// ║  Hardware: Waveshare ESP32-S3-PhotoPainter                                   ║
// ║  ESP32 Board Version: 2.0.17                                                 ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

// =============================================================================
// SECTION 1: LIBRARY INCLUDES
// =============================================================================

#include <WiFi.h>
// #include <WiFiClientSecure.h>  // Only needed when MAPS_DISABLED is removed
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPI.h>
#include <GxEPD2_7C.h>
#include <TJpg_Decoder.h>

// Custom fonts
#include "Fonts/fonnts_com_Maison_Neue_Bold9pt7b.h"
#include "Fonts/fonnts_com_Maison_Neue_Bold18pt7b.h"
// #include "Fonts/fonnts_com_Maison_Neue_Bold24pt7b.h"  // Removed to save flash
#define fonnts_com_Maison_Neue_Bold24pt7b fonnts_com_Maison_Neue_Bold18pt7b
#include "Fonts/fonnts_com_Maison_Neue_Light9pt7b.h"
// Light15pt and Light18pt removed to save ~50KB flash — aliased to smaller fonts
// #include "Fonts/fonnts_com_Maison_Neue_Light15pt7b.h"
// #include "Fonts/fonnts_com_Maison_Neue_Light18pt7b.h"
#define fonnts_com_Maison_Neue_Light15pt7b fonnts_com_Maison_Neue_Light9pt7b
#define fonnts_com_Maison_Neue_Light18pt7b fonnts_com_Maison_Neue_Bold9pt7b

#include <Wire.h>
#include "XPowersLib.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_task_wdt.h"
// #include "esp_int_wdt.h"  // removed in newer ESP32 cores
#include "esp_private/brownout.h"
#include "esp_system.h"
#include <nvs_flash.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <vector>
#include <Preferences.h>
// Uncomment this if you need a flash/RAM emergency build that falls back to the
// vector route drawing and skips downloaded map images.
// #define MAPS_DISABLED

// USB serial transport:
// - Hardware CDC/JTAG mode uses the ESP32-S3 USB serial/JTAG port exposed by
//   the board and the Arduino core.
// - USB-OTG (TinyUSB) mode keeps the old composite CDC + HID identity.
#ifndef ARDUINO_USB_MODE
#define ARDUINO_USB_MODE 0
#endif
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif

#if ARDUINO_USB_MODE
  #define USBSerial Serial
#else
  #include "USB.h"
  #include "USBHID.h"
  #include "USBCDC.h"

  USBCDC IbisUSBSerial;
  #define USBSerial IbisUSBSerial
#endif

#if !ARDUINO_USB_MODE || ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial_setTxTimeoutMs(ms) USBSerial.setTxTimeoutMs(ms)
#else
  #define USBSerial_setTxTimeoutMs(ms) do {} while (0)
#endif

// Serial diagnostics. Keep this configurable so a release build can silence
// chatter, but a device that refuses to run can be made talkative again quickly.
#ifndef IBIS_DEBUG_SERIAL
#define IBIS_DEBUG_SERIAL 0
#endif
#if IBIS_DEBUG_SERIAL
  #define DBG_print(...)    USBSerial.print(__VA_ARGS__)
  #define DBG_println(...)  USBSerial.println(__VA_ARGS__)
#else
  #define DBG_print(...)    do {} while(0)
  #define DBG_println(...)  do {} while(0)
#endif

void ibisDisableBrownout() {
  esp_brownout_disable();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
}

void __attribute__((constructor(101))) ibisDisableBrownoutEarly() {
  ibisDisableBrownout();
}

// Include the ibis logo data (place ibis_logos.h in same folder as this .ino)
// #include "ibis_logos.h"  // Removed to save 138KB flash


// =============================================================================
// SECTION 2: HARDWARE PIN DEFINITIONS
// =============================================================================

#define PMU_SDA 47
#define PMU_SCL 48
#define PMU_IRQ 21

static const int EPD_BUSY = 13;
static const int EPD_RST  = 12;
static const int EPD_DC   = 8;
static const int EPD_CS   = 9;
static const int EPD_SCK  = 10;
static const int EPD_MOSI = 11;
static const int EPD_MISO = -1;

static const int ACT_LED_PIN = 42;
static const int BOOT_BUTTON_PIN = 0;
static const int USER_BUTTON_PIN = 4;  // KEY button
static const int PWR_BUTTON_PIN = 5;


// =============================================================================
// SECTION 3: CONFIGURATION CONSTANTS
// =============================================================================

static const float LOW_BATTERY_THRESHOLD = 15.0;
static const int W = 800;
static const int H = 480;
static const int WATCHDOG_TIMEOUT_SECONDS = 120;
static const int CRASH_LOOP_THRESHOLD = 3;
static const int FACTORY_RESET_HOLD_MS = 5000;
static const int MAP_IMAGE_WIDTH = 360;
static const int MAP_IMAGE_HEIGHT = 224;
static const int MAP_IMAGE_API_SCALE = 2;
static const int MAP_IMAGE_MAX_BYTES = 120000;
static const int STRAVA_PER_PAGE = 10;
static const int STRAVA_MAX_PAGES = 200;

// Pairing mode duration
#define PAIRING_MODE_DURATION_MS (30 * 60 * 1000UL)  // 30 minutes

// Default sleep durations (can be overridden by user settings)
#define SLEEP_DURATION_UNCONFIGURED_US (7 * 24 * 60 * 60 * 1000000ULL)  // 1 week
#define DISPLAY_REFRESH_UNCONFIGURED_MS (5 * 60 * 1000UL)              // 5 minutes

#define uS_TO_S_FACTOR 1000000ULL

// Tracking period options
#define TRACK_YEARLY   0
#define TRACK_MONTHLY  1
#define TRACK_WEEKLY   2

// Sport type options
#define SPORT_RUN   "Run"
#define SPORT_RIDE  "Ride"
#define SPORT_SWIM  "Swim"
#define SPORT_HIKE  "Hike"
#define SPORT_WALK  "Walk"

// Strava's newer API responses expose detailed variants in sport_type.
static const char* RUNNING_TYPES[] = {
  "Run",
  "TrailRun",
  "VirtualRun"
};
static const int RUNNING_TYPES_COUNT = 3;

// ── Cycling activity types that all count when SPORT_TYPE == "Ride" ──────────
// Strava uses these exact type strings in their API responses.
static const char* CYCLING_TYPES[] = {
  "Ride",
  "VirtualRide",
  "EBikeRide",
  "GravelRide",
  "MountainBikeRide",
  "Handcycle",
  "Velomobile"
};
static const int CYCLING_TYPES_COUNT = 7;


// =============================================================================
// SECTION 4: RTC MEMORY VARIABLES (survive deep sleep)
// =============================================================================

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool wasManualWake = false;
RTC_DATA_ATTR time_t lastStravaFetchEpoch = 0;
RTC_DATA_ATTR time_t lastNtpSyncEpoch = 0;  // Track when we last synced NTP
RTC_DATA_ATTR int dashYear = 0;
RTC_DATA_ATTR int dashMonth = 0;
RTC_DATA_ATTR int dashWeek = 0;
RTC_DATA_ATTR float kmDone = 0;
RTC_DATA_ATTR float timeHours = 0;
RTC_DATA_ATTR int activitiesCount = 0;
RTC_DATA_ATTR float kmDone2 = 0;
RTC_DATA_ATTR float timeHours2 = 0;
RTC_DATA_ATTR int activitiesCount2 = 0;
RTC_DATA_ATTR int rapidBootCount = 0;

// Token caching - survives deep sleep
RTC_DATA_ATTR char cachedAccessToken[256] = {0};
RTC_DATA_ATTR time_t tokenExpiresAt = 0;

// ── Change #1: Smart redraw tracking ─────────────────────────────────────────
// We compare these "snapshot" values against freshly-fetched data.
// If nothing changed, we skip the display refresh (saves e-ink life and time).
// Stored in RTC so they survive deep sleep.
RTC_DATA_ATTR float  lastDrawnKmDone         = -1.0;  // -1 = never drawn
RTC_DATA_ATTR int    lastDrawnActivitiesCount = -1;
RTC_DATA_ATTR float  lastDrawnKmDone2         = -1.0;
RTC_DATA_ATTR int    lastDrawnActivitiesCount2 = -1;


// =============================================================================
// SECTION 5: GLOBAL VARIABLES
// =============================================================================

XPowersAXP2101 PMU;
Preferences preferences;
String serialInputBuffer = "";

// A full 800x480 7-color framebuffer costs ~192 KB of internal RAM. Paged
// drawing keeps WiFi/TLS/JSON heap available on ESP32-S3 while preserving the
// same firstPage()/nextPage() drawing flow.
static const uint16_t EPD_PAGE_HEIGHT = GxEPD2_730c_GDEP073E01::HEIGHT / 4;

GxEPD2_7C<GxEPD2_730c_GDEP073E01, EPD_PAGE_HEIGHT> display(
  GxEPD2_730c_GDEP073E01(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ===== USB COMPOSITE DEVICE =====
#if !ARDUINO_USB_MODE
// Minimal HID report descriptor - presents as a "vendor defined" HID device
// This is intentionally a no-op: it never sends reports, it just exists
// so Windows treats the whole USB composite as a HID device and refuses
// to power-manage it aggressively (Windows never suspends keyboards/mice/HID)
static const uint8_t ibisHidReportDescriptor[] = {
  0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
  0x09, 0x01,        // Usage (Vendor Usage 1)
  0xA1, 0x01,        // Collection (Application)
  0x09, 0x01,        //   Usage (Vendor Usage 1)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x00,  //   Logical Maximum (255)
  0x75, 0x08,        //   Report Size (8)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x02,        //   Input (Data, Variable, Absolute)
  0xC0               // End Collection
};

// HID device instance
USBHID ibishid;
#endif
bool usbCompositeStarted = false;

// ===== USER CONFIGURATION (loaded from NVS, set via Ibis Setup app) =====
// These start BLANK - user must configure via app
String WIFI_SSID = "";
String WIFI_PASS = "";
String CLIENT_ID = "";
String CLIENT_SECRET = "";
String REFRESH_TOKEN = "";
String DATA_SOURCE = "garmin_middleware";
String MIDDLEWARE_URL = "";
String MIDDLEWARE_APP_KEY = "";
String IBIS_TOKEN = "";
String USER_NAME = "";
String SPORT_TYPE = "Run";
String SPORT_TYPE2 = "";         // Second sport (optional, empty = single-sport mode)
String CUSTOM_TITLE = "";        // Custom title for header (optional)
String MAPS_API_KEY = "";        // Google Maps Static API key (optional)
float YEARLY_GOAL = 1000.0;
float YEARLY_GOAL2 = 0;
int REFRESH_HOURS = 12;          // Refresh interval in hours
int TRACK_PERIOD = TRACK_YEARLY; // 0=yearly, 1=monthly, 2=weekly

// Runtime state
String accessToken = "";
String lastTitle = "";
String lastLine = "";
String lastPolyline = "";
float lastDistKm = 0;        // Last activity distance in km
int lastMovingSecs = 0;      // Last activity moving time in seconds
float lastAvgSpeedKph = 0.0; // Last activity average speed in km/h (cycling)
String lastDateStr = "";     // Last activity date e.g. "2 January"

// Sport 2 runtime state
String lastTitle2 = "";
String lastLine2 = "";
String lastPolyline2 = "";
float lastDistKm2 = 0;
int lastMovingSecs2 = 0;
float lastAvgSpeedKph2 = 0.0;
String lastDateStr2 = "";

bool isUsbConnected = false;
bool wasUsbConnected = false;
float batteryPercentage = 0.0;
float batteryVoltage = 0.0;
bool lowBatteryMode = false;
bool isCharging = false;
String lastUpdateTime = "";
unsigned long lastDisplayRefresh = 0;
unsigned long lastStravaCheck = 0;
uint16_t C_HEADER = GxEPD_RED;
uint16_t C_TEXT_DIM = GxEPD_RED;
bool inSafeMode = false;

// Setup state - determined by checking if WiFi credentials exist
bool isConfigured = false;

// Sleep control flag - set by serial commands to trigger sleep after command completes
bool sleepRequested = false;


// =============================================================================
// SECTION 6: POLYLINE DECODING
// =============================================================================

struct Point { 
  float lat; 
  float lon; 
};

std::vector<Point> decodePolyline(String encoded) {
  std::vector<Point> points;
  int index = 0, len = encoded.length();
  int lat = 0, lng = 0;

  while (index < len) {
    int b, shift = 0, result = 0;
    do { 
      b = encoded[index++] - 63; 
      result |= (b & 0x1f) << shift; 
      shift += 5; 
    } while (b >= 0x20);
    int dlat = ((result & 1) ? ~(result >> 1) : (result >> 1));
    lat += dlat;

    shift = 0; 
    result = 0;
    do { 
      b = encoded[index++] - 63; 
      result |= (b & 0x1f) << shift; 
      shift += 5; 
    } while (b >= 0x20);
    int dlng = ((result & 1) ? ~(result >> 1) : (result >> 1));
    lng += dlng;

    points.push_back({lat * 1e-5f, lng * 1e-5f});
  }
  return points;
}


// =============================================================================
// SECTION 6B: GOOGLE MAPS STATIC IMAGE FOR ROUTE
// =============================================================================
#ifndef MAPS_DISABLED

// Global offset for TJpg_Decoder callback to know where to draw
static int mapDrawX = 0;
static int mapDrawY = 0;

// Cached JPEG buffer — fetched once before the display loop, rendered in each page pass
static uint8_t *cachedMapJpeg = NULL;
static int cachedMapJpegLen = 0;
static String mapDebugMsg = "";  // Debug: shown on display if map fetch fails

// Pre-fetched map images (fetched while WiFi is connected, before drawDashboard)
static uint8_t *prefetchedMap1 = NULL;
static int prefetchedMap1Len = 0;
static bool prefetchedMap1Valid = false;
static uint8_t *prefetchedMap2 = NULL;
static int prefetchedMap2Len = 0;
static bool prefetchedMap2Valid = false;
static String mapDebugMsg1 = "";
static String mapDebugMsg2 = "";

bool fetchMapImage(int areaW, int areaH, const String& sport);
bool drawCachedMapImage(int areaX, int areaY, const uint8_t *jpeg, int jpegLen);

void prefetchMapImages() {
  bool ds = (SPORT_TYPE2.length() > 0);
  int mW = MAP_IMAGE_WIDTH, mH = MAP_IMAGE_HEIGHT;

  // Free old
  if (prefetchedMap1) { free(prefetchedMap1); prefetchedMap1 = NULL; }
  if (prefetchedMap2) { free(prefetchedMap2); prefetchedMap2 = NULL; }
  prefetchedMap1Len = 0; prefetchedMap2Len = 0;
  prefetchedMap1Valid = false; prefetchedMap2Valid = false;

  // Fetch sport1 map
  prefetchedMap1Valid = fetchMapImage(mW, mH, SPORT_TYPE);
  mapDebugMsg1 = mapDebugMsg;
  prefetchedMap1 = cachedMapJpeg;
  prefetchedMap1Len = cachedMapJpegLen;
  cachedMapJpeg = NULL; cachedMapJpegLen = 0;

  // Fetch sport2 map
  if (ds && lastPolyline2.length() > 0) {
    String savedPoly = lastPolyline;
    lastPolyline = lastPolyline2;
    prefetchedMap2Valid = fetchMapImage(mW, mH, SPORT_TYPE2);
    mapDebugMsg2 = mapDebugMsg;
    lastPolyline = savedPoly;
    prefetchedMap2 = cachedMapJpeg;
    prefetchedMap2Len = cachedMapJpegLen;
    cachedMapJpeg = NULL; cachedMapJpegLen = 0;
  }
}

// Map RGB565 pixels to a cleaner 7-color e-ink palette.
uint16_t rgb565ToEinkColor(uint16_t rgb565) {
  uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
  uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
  uint8_t b = (rgb565 & 0x1F) << 3;
  int maxC = max((int)r, max((int)g, (int)b));
  int minC = min((int)r, min((int)g, (int)b));
  int sat = maxC - minC;
  int lum = ((int)r * 299 + (int)g * 587 + (int)b * 114) / 1000;

  if (r > 150 && r > g + 35 && r > b + 35) return GxEPD_RED;
  if (r > 180 && g > 145 && b < 105 && sat > 70) return GxEPD_YELLOW;
  if (r > 165 && g > 75 && g < 175 && b < 105 && r > g + 24) return GxEPD_ORANGE;
  if (g > 135 && g >= r + 22 && g >= b + 18 && sat > 45 && lum < 226) return GxEPD_GREEN;
  if (b > 145 && b >= r + 26 && b >= g + 16 && sat > 45 && lum < 228) return GxEPD_BLUE;

  if (lum < 118) return GxEPD_BLACK;
  if (sat < 34) return (lum < 165) ? GxEPD_BLACK : GxEPD_WHITE;
  if (lum > 225) return GxEPD_WHITE;

  struct ColorEntry { uint8_t r, g, b; uint16_t epd; };
  static const ColorEntry palette[] = {
    {  0,   0,   0, GxEPD_BLACK },
    {255, 255, 255, GxEPD_WHITE },
    {  0, 128,   0, GxEPD_GREEN },
    {  0,   0, 255, GxEPD_BLUE  },
    {255,   0,   0, GxEPD_RED   },
    {255, 255,   0, GxEPD_YELLOW},
    {255, 165,   0, GxEPD_ORANGE},
  };

  uint32_t bestDist = UINT32_MAX;
  uint16_t bestColor = GxEPD_WHITE;
  for (auto &c : palette) {
    int32_t dr = (int32_t)r - c.r;
    int32_t dg = (int32_t)g - c.g;
    int32_t db = (int32_t)b - c.b;
    uint32_t dist = dr * dr + dg * dg + db * db;
    if (dist < bestDist) { bestDist = dist; bestColor = c.epd; }
  }
  return bestColor;
}

// TJpg_Decoder callback — called for each decoded MCU block
bool onJpegBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      uint16_t rgb565 = bitmap[row * w + col];
      uint16_t einkColor = rgb565ToEinkColor(rgb565);
      display.drawPixel(mapDrawX + x + col, mapDrawY + y + row, einkColor);
    }
  }
  return true;
}

// URL-encode a string (for polyline special chars)
String urlEncode(const String &str) {
  String encoded = "";
  encoded.reserve(str.length() * 2);
  for (int i = 0; i < (int)str.length(); i++) {
    char c = str.charAt(i);
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)c);
      encoded += buf;
    }
  }
  return encoded;
}

// Fetch Google Maps image into cachedMapJpeg buffer. Call BEFORE the display loop.
// Returns true if image is ready to render.
bool fetchMapImage(int areaW, int areaH, const String& sport) {
  // Free any previous cached image
  if (cachedMapJpeg) { free(cachedMapJpeg); cachedMapJpeg = NULL; cachedMapJpegLen = 0; }

  mapDebugMsg = "";
  if (lastPolyline.length() == 0) { mapDebugMsg = "No polyline"; return false; }
  if (MAPS_API_KEY.length() == 0) { mapDebugMsg = "No Maps key"; return false; }

  feedWatchdog();

  String encodedPoly = urlEncode(lastPolyline);

  if (encodedPoly.length() > 7500) {
    DBG_println("Polyline too long for Maps API");
    mapDebugMsg = "polyline too long";
    return false;
  }

  String routeColor = (sport == SPORT_RIDE) ? "0x009c35ff" : "0xff0000ff";
  String pathParams;
  if (encodedPoly.length() <= 3500) {
    pathParams = "&path=weight:8%7Ccolor:0x111111ff%7Cenc:" + encodedPoly;
    pathParams += "&path=weight:5%7Ccolor:" + routeColor + "%7Cenc:" + encodedPoly;
  } else {
    pathParams = "&path=weight:5%7Ccolor:" + routeColor + "%7Cenc:" + encodedPoly;
  }

  String url = "https://maps.googleapis.com/maps/api/staticmap?"
        "size=" + String(areaW) + "x" + String(areaH) +
        "&scale=" + String(MAP_IMAGE_API_SCALE) +
        "&maptype=roadmap"
        + pathParams +
        "&style=feature:all%7Celement:labels%7Cvisibility:off"
        "&style=feature:administrative%7Cvisibility:off"
        "&style=feature:landscape%7Celement:geometry%7Ccolor:0xffffff"
        "&style=feature:landscape.natural%7Celement:geometry%7Ccolor:0x86c96e"
        "&style=feature:poi.park%7Celement:geometry%7Ccolor:0x70bf63"
        "&style=feature:poi%7Cvisibility:off"
        "&style=feature:road%7Celement:labels%7Cvisibility:off"
        "&style=feature:road.local%7Celement:geometry%7Ccolor:0x9c9c9c"
        "&style=feature:road.arterial%7Celement:geometry%7Ccolor:0xf6cf45"
        "&style=feature:road.highway%7Celement:geometry%7Ccolor:0xee9b3a"
        "&style=feature:transit%7Cvisibility:off"
        "&style=feature:water%7Celement:geometry%7Ccolor:0xb9ddff"
        "&format=jpg-baseline"
        "&key=" + MAPS_API_KEY;

  DBG_println("Fetching Google Maps image...");
  DBG_print("  API key length: ");
  DBG_println(MAPS_API_KEY.length());
  DBG_print("  Polyline length: ");
  DBG_println(lastPolyline.length());
  DBG_print("  URL length: ");
  DBG_println(url.length());

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(3);
  http.setTimeout(15000);
  http.setUserAgent("IbisDash/5.1 ESP32");
  http.useHTTP10(true);
  const char* headerKeys[] = {"Content-Type"};
  http.collectHeaders(headerKeys, 1);
  if (!http.begin(url)) {
    mapDebugMsg = "http begin fail";
    return false;
  }
  int httpCode = http.GET();

  DBG_print("  HTTP response: ");
  DBG_println(httpCode);

  if (httpCode != 200) {
    DBG_print("Maps API error: ");
    DBG_println(httpCode);
    mapDebugMsg = "HTTP " + String(httpCode);
    http.end();
    return false;
  }

  String contentType = http.header("Content-Type");
  contentType.toLowerCase();
  if (contentType.length() > 0 &&
      contentType.indexOf("jpeg") < 0 &&
      contentType.indexOf("jpg") < 0) {
    mapDebugMsg = "not jpeg";
    http.end();
    return false;
  }

  int contentLen = http.getSize();
  DBG_print("  Image size: ");
  DBG_print(contentLen);
  DBG_println(" bytes");

  // Handle chunked transfer (contentLen == -1) by reading into a growing buffer
  if (contentLen > MAP_IMAGE_MAX_BYTES) {
    mapDebugMsg = "too large " + String(contentLen);
    http.end();
    return false;
  }

  NetworkClient *stream = http.getStreamPtr();
  if (!stream) {
    mapDebugMsg = "no stream";
    http.end();
    return false;
  }

  if (contentLen > 0) {
    // Known size — allocate and read
    cachedMapJpeg = (uint8_t *)malloc(contentLen);
    if (!cachedMapJpeg) {
      mapDebugMsg = "malloc fail " + String(contentLen);
      http.end();
      return false;
    }

    int bytesRead = 0;
    unsigned long startMs = millis();
    while (bytesRead < contentLen && (millis() - startMs) < 15000) {
      if (stream->available()) {
        int toRead = min((int)stream->available(), contentLen - bytesRead);
        int got = stream->readBytes(cachedMapJpeg + bytesRead, toRead);
        bytesRead += got;
      } else {
        delay(10);
      }
      feedWatchdog();
    }

    if (bytesRead != contentLen) {
      DBG_print("  Incomplete download: ");
      DBG_print(bytesRead);
      DBG_print("/");
      DBG_println(contentLen);
      mapDebugMsg = "incomplete " + String(bytesRead) + "/" + String(contentLen);
      free(cachedMapJpeg); cachedMapJpeg = NULL;
      http.end();
      return false;
    }
    cachedMapJpegLen = contentLen;
  } else {
    // Chunked transfer — read in chunks
    int bufSize = min(MAP_IMAGE_MAX_BYTES, 32000);
    cachedMapJpeg = (uint8_t *)malloc(bufSize);
    if (!cachedMapJpeg) {
      mapDebugMsg = "chunk malloc fail";
      http.end();
      return false;
    }

    int bytesRead = 0;
    unsigned long startMs = millis();
    while (http.connected() && (millis() - startMs) < 15000) {
      int avail = stream->available();
      if (avail > 0) {
        if (bytesRead + avail > bufSize) {
          bufSize = bytesRead + avail + 4096;
          if (bufSize > MAP_IMAGE_MAX_BYTES) { mapDebugMsg = "chunk too large"; free(cachedMapJpeg); cachedMapJpeg = NULL; http.end(); return false; }
          uint8_t *newBuf = (uint8_t *)realloc(cachedMapJpeg, bufSize);
          if (!newBuf) { mapDebugMsg = "realloc fail"; free(cachedMapJpeg); cachedMapJpeg = NULL; http.end(); return false; }
          cachedMapJpeg = newBuf;
        }
        int got = stream->readBytes(cachedMapJpeg + bytesRead, avail);
        bytesRead += got;
      } else {
        delay(10);
      }
      feedWatchdog();
    }
    cachedMapJpegLen = bytesRead;
  }

  http.end();

  if (cachedMapJpegLen < 4 || cachedMapJpeg[0] != 0xFF || cachedMapJpeg[1] != 0xD8) {
    mapDebugMsg = "bad jpeg";
    free(cachedMapJpeg);
    cachedMapJpeg = NULL;
    cachedMapJpegLen = 0;
    return false;
  }

  DBG_print("  Downloaded ");
  DBG_print(cachedMapJpegLen);
  USBSerial.println(" bytes OK");
  mapDebugMsg = "dl ok " + String(cachedMapJpegLen) + "b";
  return cachedMapJpegLen > 0;
}

// Render the cached JPEG onto the display. Call INSIDE the display page loop.
bool drawCachedMapImage(int areaX, int areaY, const uint8_t *jpeg, int jpegLen) {
  if (!jpeg || jpegLen == 0) return false;

  mapDrawX = areaX;
  mapDrawY = areaY;

  TJpgDec.setJpgScale(MAP_IMAGE_API_SCALE);
  TJpgDec.setCallback(onJpegBlock);

  JRESULT res = TJpgDec.drawJpg(0, 0, jpeg, jpegLen);

  if (res != JDR_OK) {
    DBG_print("  JPEG decode error: ");
    DBG_println(res);
    return false;
  }
  return true;
}

// Free the cached map image buffer
void freeMapImage() {
  if (cachedMapJpeg) { free(cachedMapJpeg); cachedMapJpeg = NULL; cachedMapJpegLen = 0; }
}
#endif // MAPS_DISABLED


// =============================================================================
// SECTION 7: CONFIGURATION MANAGEMENT
// =============================================================================

void loadConfiguration() {
  DBG_println("=== Loading Configuration from NVS ===");
  
  preferences.begin("config", true);  // Read-only
  
  // Load WiFi credentials
  WIFI_SSID = preferences.getString("ssid", "");
  WIFI_PASS = preferences.getString("password", "");
  
  // Load Strava API credentials
  CLIENT_ID = preferences.getString("clientID", "");
  CLIENT_SECRET = preferences.getString("clientSecret", "");
  REFRESH_TOKEN = preferences.getString("refreshToken", "");

  // Load Garmin middleware credentials
  DATA_SOURCE = preferences.getString("dataSource", "garmin_middleware");
  MIDDLEWARE_URL = preferences.getString("middlewareUrl", "");
  // NVS keys are limited to 15 chars — "middlewareAppKey" (16) silently failed to store
  MIDDLEWARE_APP_KEY = preferences.getString("mwAppKey", "");
  IBIS_TOKEN = preferences.getString("ibisToken", "");
  
  // Load user settings
  USER_NAME = preferences.getString("name", "");
  SPORT_TYPE = preferences.getString("sport", "Run");
  SPORT_TYPE2 = preferences.getString("sport2", "");
  CUSTOM_TITLE = preferences.getString("title", "");
  MAPS_API_KEY = preferences.getString("mapsApiKey", "");
  YEARLY_GOAL = preferences.getFloat("goal", 1000.0);
  YEARLY_GOAL2 = preferences.getFloat("goal2", 0);
  REFRESH_HOURS = preferences.getInt("refreshHours", 12);
  TRACK_PERIOD = preferences.getInt("trackPeriod", TRACK_YEARLY);
  
  preferences.end();
  
  // Determine if board is configured (has WiFi credentials)
  isConfigured = (WIFI_SSID.length() > 0);
  
  DBG_println("[OK] Configuration loaded:");
  DBG_print("  Configured: "); DBG_println(isConfigured ? "YES" : "NO");
  DBG_print("  WiFi SSID: "); DBG_println(WIFI_SSID.length() > 0 ? WIFI_SSID : "(not set)");
  DBG_print("  User Name: "); DBG_println(USER_NAME.length() > 0 ? USER_NAME : "(not set)");
  DBG_print("  Sport Type: "); DBG_println(SPORT_TYPE);
  DBG_print("  Data Source: "); DBG_println(DATA_SOURCE.length() > 0 ? DATA_SOURCE : "(not set)");
  DBG_print("  Middleware URL: "); DBG_println(MIDDLEWARE_URL.length() > 0 ? MIDDLEWARE_URL : "(not set)");
  DBG_print("  Goal: "); DBG_print(YEARLY_GOAL); DBG_println(" km");
  DBG_print("  Refresh Hours: "); DBG_println(REFRESH_HOURS);
  DBG_print("  Track Period: "); 
  switch(TRACK_PERIOD) {
    case TRACK_WEEKLY: DBG_println("Weekly"); break;
    case TRACK_MONTHLY: DBG_println("Monthly"); break;
    default: DBG_println("Yearly"); break;
  }
  DBG_println();
}

bool hasStravaCredentials() {
  return (CLIENT_ID.length() > 0 && CLIENT_SECRET.length() > 0 && REFRESH_TOKEN.length() > 0);
}

bool hasMiddlewareDashboardCredentials() {
  return (MIDDLEWARE_URL.length() > 0 && MIDDLEWARE_APP_KEY.length() > 0 && IBIS_TOKEN.length() > 0);
}

bool hasDashboardCredentials() {
  if (hasMiddlewareDashboardCredentials()) return true;
  return hasStravaCredentials();
}

// ── Change #4 helper: returns true if a given Strava type counts as "cycling" ─
bool isCyclingType(const String& actType) {
  for (int i = 0; i < CYCLING_TYPES_COUNT; i++) {
    if (actType == CYCLING_TYPES[i]) return true;
  }
  return false;
}

bool isRunningType(const String& actType) {
  for (int i = 0; i < RUNNING_TYPES_COUNT; i++) {
    if (actType == RUNNING_TYPES[i]) return true;
  }
  return false;
}

// Returns true if actType matches the given sport filter.
// Run/Ride accept detailed Strava sport_type variants as well.
bool activityMatchesSportType(const String& actType, const String& sport) {
  if (sport.length() == 0) return false;
  if (sport == SPORT_RUN) return isRunningType(actType);
  if (sport == SPORT_RIDE) return isCyclingType(actType);
  return (actType == sport);
}

// Backward-compatible wrapper for sport1
bool activityMatchesSport(const String& actType) {
  return activityMatchesSportType(actType, SPORT_TYPE);
}


// =============================================================================
// SECTION 8: WATCHDOG & STABILITY FUNCTIONS
// =============================================================================

void initWatchdog() {
  // Disabled — was causing crash on boot with newer ESP32 core
  DBG_println("Watchdog disabled");
}

void feedWatchdog() {
  // No-op — watchdog disabled
}

bool checkCrashLoop() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    rapidBootCount++;
    if (rapidBootCount >= 5) {
      DBG_println("WARNING: CRASH LOOP DETECTED!");
      return true;
    }
  } else {
    rapidBootCount = 0;
  }
  return false;
}

void resetCrashCounter() {
  rapidBootCount = 0;
}

bool checkFactoryReset() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    DBG_println("BOOT button held - checking for factory reset...");
    
    for (int i = 0; i < 5; i++) {
      digitalWrite(ACT_LED_PIN, HIGH);
      delay(100);
      digitalWrite(ACT_LED_PIN, LOW);
      delay(100);
      feedWatchdog();
    }
    
    unsigned long startTime = millis();
    
    while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      if (millis() - startTime >= FACTORY_RESET_HOLD_MS) {
        DBG_println("FACTORY RESET TRIGGERED!");
        
        for (int i = 0; i < 10; i++) {
          digitalWrite(ACT_LED_PIN, HIGH);
          delay(50);
          digitalWrite(ACT_LED_PIN, LOW);
          delay(50);
        }
        
        // Erase all configuration
        preferences.begin("config", false);
        preferences.clear();
        preferences.end();
        
        nvs_flash_erase();
        nvs_flash_init();
        
        bootCount = 0;
        rapidBootCount = 0;
        lastStravaFetchEpoch = 0;
        kmDone = 0;
        timeHours = 0;
        activitiesCount = 0;
        
        // Reset smart-redraw snapshot
        lastDrawnKmDone = -1.0;
        lastDrawnActivitiesCount = -1;
        
        USBSerial.println("[OK] Factory reset complete - restarting...");
        delay(500);
        ESP.restart();
        return true;
      }
      
      digitalWrite(ACT_LED_PIN, (millis() / 200) % 2);
      delay(10);
      feedWatchdog();
    }
  }
  return false;
}

void enterSafeMode() {
  USBSerial.println("*** SAFE MODE ***");
  inSafeMode = true;
  
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);
  display.setRotation(2);
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(0, 0, W, 100, GxEPD_RED);
    display.setFont(&fonnts_com_Maison_Neue_Bold24pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(200, 65);
    display.print("SAFE MODE");
    
    display.setFont(&fonnts_com_Maison_Neue_Light18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, 200);
    display.print("Hold BOOT button 5 seconds to reset");
  } while (display.nextPage());
  
  while (true) {
    feedWatchdog();
    checkFactoryReset();
    delay(100);
    digitalWrite(ACT_LED_PIN, (millis() / 1000) % 2);
  }
}


// =============================================================================
// SECTION 9: PMU FUNCTIONS
// =============================================================================

void pmu_irq_init() {
  pinMode(PMU_IRQ, INPUT_PULLUP);
  
  // Verify PMU IRQ pin state
  int irqState = digitalRead(PMU_IRQ);
  DBG_print("PMU IRQ pin (GPIO 21) state: ");
  DBG_println(irqState ? "HIGH" : "LOW");
  
  // Configure PMU to generate IRQ on USB events
  if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    PMU.clearIrqStatus();
    
    // Enable USB insertion/removal interrupts
    PMU.enableIRQ(XPOWERS_AXP2101_VBUS_INSERT_IRQ);
    PMU.enableIRQ(XPOWERS_AXP2101_VBUS_REMOVE_IRQ);
    
    DBG_println("  ✓ PMU USB interrupts enabled");
  } else {
    DBG_println("  ⚠ PMU interrupt setup failed");
  }
}

void pmu_configure_awake() {
  feedWatchdog();
  
  PMU.disableSleep();
  PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);
  PMU.disableVbusVoltageMeasure();
  
  PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  PMU.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
  PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
  PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);
  PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
  PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
  
  PMU.setDC1Voltage(3300);
  PMU.setALDO1Voltage(3300); PMU.enableALDO1();
  PMU.setALDO2Voltage(3300); PMU.enableALDO2();
  PMU.setALDO3Voltage(3300); PMU.enableALDO3();
  PMU.setALDO4Voltage(3300); PMU.enableALDO4();
  
  PMU.enableBattVoltageMeasure();
  PMU.enableBattDetection();
  PMU.clearIrqStatus();
  
  feedWatchdog();
}

void pmu_prepare_for_esp32_sleep() {
  feedWatchdog();
  
  DBG_println("Preparing PMU for deep sleep...");
  
  // Clear any pending interrupts
  PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  PMU.clearIrqStatus();
  
  // CRITICAL: Enable USB insertion interrupt to wake from sleep
  PMU.enableIRQ(XPOWERS_AXP2101_VBUS_INSERT_IRQ);  // Wake on USB plug-in
  PMU.enableIRQ(XPOWERS_AXP2101_VBUS_REMOVE_IRQ);  // Detect USB removal
  
  // Keep essential power rails on during sleep
  PMU.disableSleep();
  PMU.enableALDO3();  // Keep display rail alive
  PMU.enableALDO4();  // Keep other peripherals alive
  
  USBSerial.println("[OK] PMU ready - will wake on USB connection");
}

void printBatteryStatus() {
  feedWatchdog();
  
  // Re-read PMU to ensure fresh values
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  delay(100);
  
  isUsbConnected = PMU.isVbusIn();
  batteryVoltage = PMU.getBattVoltage() / 1000.0;
  
  // Check charging status - isCharging returns true when actively charging
  // When battery is full and USB connected, isCharging will be false
  isCharging = PMU.isCharging();
  
  // Battery percentage based on voltage
  if (isUsbConnected) {
    if (batteryVoltage >= 4.05) batteryPercentage = 100;
    else if (batteryVoltage >= 3.95) batteryPercentage = 90;
    else if (batteryVoltage >= 3.85) batteryPercentage = 75;
    else if (batteryVoltage >= 3.75) batteryPercentage = 55;
    else if (batteryVoltage >= 3.65) batteryPercentage = 35;
    else if (batteryVoltage >= 3.55) batteryPercentage = 20;
    else if (batteryVoltage >= 3.45) batteryPercentage = 10;
    else batteryPercentage = 5;
    
    // CRITICAL FIX: If battery shows <20% on USB, PMU reading is unreliable
    if (batteryPercentage < 20) {
      DBG_print(" [⚠️  PMU UNRELIABLE - ignoring false low reading] ");
      batteryPercentage = 50;
    }
  } else {
    if (batteryVoltage >= 4.10) batteryPercentage = 100;
    else if (batteryVoltage >= 4.00) batteryPercentage = 90;
    else if (batteryVoltage >= 3.90) batteryPercentage = 75;
    else if (batteryVoltage >= 3.80) batteryPercentage = 55;
    else if (batteryVoltage >= 3.70) batteryPercentage = 40;
    else if (batteryVoltage >= 3.60) batteryPercentage = 25;
    else if (batteryVoltage >= 3.50) batteryPercentage = 15;
    else batteryPercentage = 5;
  }
  
  // CRITICAL FIX: NEVER enable low battery mode when USB connected
  if (isUsbConnected || USBSerial) {
    lowBatteryMode = false;
  } else {
    lowBatteryMode = (batteryPercentage < LOW_BATTERY_THRESHOLD);
  }
  
  DBG_print("Battery: ");
  DBG_print(batteryPercentage);
  DBG_print("% (");
  DBG_print(batteryVoltage, 2);
  DBG_print("V)");
  
  if (isUsbConnected) {
    if (isCharging) {
      DBG_print(" [CHARGING]");
    } else if (batteryPercentage >= 95) {
      DBG_print(" [CHARGED]");
    } else {
      DBG_print(" [USB]");
    }
  }
  DBG_println();
}


// =============================================================================
// SECTION 10: USB COMPOSITE DEVICE INITIALIZATION
// =============================================================================

#if !ARDUINO_USB_MODE
class IbisDummyHID : public USBHIDDevice {
public:
  IbisDummyHID() {}
  
  uint16_t _onGetDescriptor(uint8_t* buffer) {
    memcpy(buffer, ibisHidReportDescriptor, sizeof(ibisHidReportDescriptor));
    return sizeof(ibisHidReportDescriptor);
  }
  
  void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) {
    // Intentionally empty
  }
};

IbisDummyHID ibisDummyDevice;
#endif

void initUSBComposite() {
#if !ARDUINO_USB_MODE
  USB.productName("Ibis Dash");
  USB.manufacturerName("Ibis");
  USB.VID(0x303A);
  USB.PID(0x8001);
  
  ibishid.addDevice(&ibisDummyDevice, sizeof(ibisHidReportDescriptor));
  ibishid.begin();
  
  USBSerial.begin();
  USBSerial_setTxTimeoutMs(0);
  
  USB.begin();
#else
  USBSerial.begin(115200);
  USBSerial_setTxTimeoutMs(0);
#endif
  
  usbCompositeStarted = true;
}


// =============================================================================
// SECTION 11: DISPLAY LOW-LEVEL FUNCTIONS
// =============================================================================

void epd_wait_busy() {
  unsigned long startTime = millis();
  unsigned long lastYield = millis();
  
  while (digitalRead(EPD_BUSY) == LOW) {
    delay(10);
    if (millis() - lastYield >= 100) {
      feedWatchdog();
      yield();
      lastYield = millis();
    }
    if (millis() - startTime > 60000) {
      DBG_println("Display timeout!");
      return;
    }
  }
  yield();
  feedWatchdog();
}

void epd_hardware_reset() {
  digitalWrite(EPD_RST, HIGH);
  delay(50);
  digitalWrite(EPD_RST, LOW);
  delay(20);
  digitalWrite(EPD_RST, HIGH);
  delay(50);
  epd_wait_busy();
}

void epd_deep_init() {
  feedWatchdog();
  epd_hardware_reset();
  delay(80);
  epd_wait_busy();
}

void blinkLED(int times) {
  for(int i = 0; i < times; i++) {
    digitalWrite(ACT_LED_PIN, HIGH);
    delay(100);
    digitalWrite(ACT_LED_PIN, LOW);
    delay(100);
  }
}


// SECTION 12: Logo removed to save flash


// =============================================================================
// SECTION 13: SETUP SCREEN (shown when not configured)
// =============================================================================

void drawSetupScreen() {
  DBG_println("=== Drawing Setup Screen ===");
  feedWatchdog();
  
  DBG_println("Stabilizing power...");
  delay(2000);
  feedWatchdog();
  
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  delay(500);
  feedWatchdog();
  
  // esp_task_wdt_delete(NULL);
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    
    display.fillRect(0, 0, W, 80, GxEPD_RED);
    display.setFont(&fonnts_com_Maison_Neue_Bold24pt7b);
    display.setTextColor(GxEPD_WHITE);
    
    int16_t tx, ty;
    uint16_t tw, th;
    String headerText = "IBIS SETUP";
    display.getTextBounds(headerText, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((W - tw) / 2, 55);
    display.print(headerText);
    
    int textStartY = 180;
    int lineSpacing = 45;
    
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    String line1 = "Finish setup on computer";
    display.getTextBounds(line1, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((W - tw) / 2, textStartY);
    display.print(line1);
    
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    
    String line2 = "Connect board with USB-C";
    display.getTextBounds(line2, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((W - tw) / 2, textStartY + lineSpacing);
    display.print(line2);
    
    String line3 = "Run ibis.exe and follow the instructions";
    display.getTextBounds(line3, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((W - tw) / 2, textStartY + lineSpacing * 2);
    display.print(line3);
    
  } while (display.nextPage());
  
  epd_wait_busy();
  
  delay(500);
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  pmu_configure_awake();
  delay(500);
  
  // esp_task_wdt_add(NULL);
  feedWatchdog();
  
  DBG_println("Setup screen complete!");
}


// =============================================================================
// SECTION 14: WIFI & TIME FUNCTIONS
// =============================================================================

void connectWiFi() {
  if (WIFI_SSID.length() == 0) {
    DBG_println("No WiFi credentials - skipping connection");
    return;
  }
  
  DBG_print("Connecting to WiFi: ");
  DBG_println(WIFI_SSID);
  
  for (int i = 0; i < 3; i++) {
    delay(1000);
    feedWatchdog();
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    handleSerialCommands();
    delay(500);
    DBG_print(".");
    attempts++;
    feedWatchdog();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    DBG_println(" Connected!");
    DBG_print("IP: ");
    DBG_println(WiFi.localIP());
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
  } else {
    USBSerial.println(" FAILED!");
  }
}

bool testWiFiConnection() {
  if (WIFI_SSID.length() == 0) return false;
  
  DBG_println("Testing WiFi connection...");
  
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    DBG_print(".");
    attempts++;
    feedWatchdog();
  }
  
  bool success = (WiFi.status() == WL_CONNECTED);
  
  if (success) USBSerial.println(" WiFi OK!");
  else         USBSerial.println(" WiFi FAILED!");
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  return success;
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void initTime() {
  time_t now;
  time(&now);
  if (lastNtpSyncEpoch > 0 && (now - lastNtpSyncEpoch) < 43200) {
    DBG_println("NTP sync skipped (synced within 12h)");
    return;
  }
  
  DBG_println("Syncing time with NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    attempts++;
    feedWatchdog();
  }
  
  if (getLocalTime(&timeinfo)) {
    DBG_print("Time synced: ");
    DBG_println(&timeinfo, "%Y-%m-%d %H:%M:%S");
    time(&lastNtpSyncEpoch);
  }
}


// =============================================================================
// SECTION 15: STRAVA API FUNCTIONS
// =============================================================================

bool refreshAccessToken() {
  if (!hasStravaCredentials()) {
    DBG_println("No Strava credentials - skipping token refresh");
    return false;
  }
  
  time_t now;
  time(&now);
  if (cachedAccessToken[0] != '\0' && tokenExpiresAt > 0 && now < (tokenExpiresAt - 300)) {
    DBG_println("Using cached access token");
    accessToken = String(cachedAccessToken);
    return true;
  }
  
  DBG_println("Refreshing Strava access token...");
  feedWatchdog();
  
  HTTPClient http;
  String url = "https://www.strava.com/oauth/token";
  url += "?client_id=" + CLIENT_ID;
  url += "&client_secret=" + CLIENT_SECRET;
  url += "&refresh_token=" + REFRESH_TOKEN;
  url += "&grant_type=refresh_token";
  
  http.begin(url);
  int code = http.POST("");
  
  if (code == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    accessToken = doc["access_token"].as<String>();
    int expiresIn = doc["expires_in"] | 21600;
    
    strncpy(cachedAccessToken, accessToken.c_str(), sizeof(cachedAccessToken) - 1);
    cachedAccessToken[sizeof(cachedAccessToken) - 1] = '\0';
    time(&now);
    tokenExpiresAt = now + expiresIn;
    
    USBSerial.println("[OK] Token refreshed and cached");
    http.end();
    return true;
  } else {
    DBG_print("[FAIL] Token refresh: ");
    DBG_println(code);
    http.end();
    return false;
  }
}

void getTrackingPeriodTimestamps(long &afterTS, long &beforeTS) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    DBG_println("Failed to get time!");
    afterTS = 0;
    beforeTS = 0;
    return;
  }
  
  int year = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon;
  int day = timeinfo.tm_mday;
  int wday = timeinfo.tm_wday;  // 0 = Sunday
  
  struct tm start = {0}, end = {0};
  
  switch (TRACK_PERIOD) {
    case TRACK_WEEKLY: {
      int daysSinceMonday = (wday == 0) ? 6 : wday - 1;
      
      start.tm_year = year - 1900;
      start.tm_mon = month;
      start.tm_mday = day - daysSinceMonday;
      start.tm_hour = 0;
      start.tm_min = 0;
      start.tm_sec = 0;
      
      end = start;
      end.tm_mday += 7;
      
      dashWeek = (timeinfo.tm_yday / 7) + 1;
      dashMonth = month + 1;
      dashYear = year;
      break;
    }
    
    case TRACK_MONTHLY: {
      start.tm_year = year - 1900;
      start.tm_mon = month;
      start.tm_mday = 1;
      
      end.tm_year = year - 1900;
      end.tm_mon = month + 1;
      end.tm_mday = 1;
      
      dashMonth = month + 1;
      dashYear = year;
      break;
    }
    
    default:
    case TRACK_YEARLY: {
      start.tm_year = year - 1900;
      start.tm_mon = 0;
      start.tm_mday = 1;
      
      end.tm_year = year - 1900 + 1;
      end.tm_mon = 0;
      end.tm_mday = 1;
      
      dashYear = year;
      break;
    }
  }
  
  afterTS = mktime(&start);
  beforeTS = mktime(&end);
}

void fetchStravaData() {
  if (!hasStravaCredentials()) {
    DBG_println("No Strava credentials - using placeholder data");
    kmDone = 0; timeHours = 0; activitiesCount = 0;
    kmDone2 = 0; timeHours2 = 0; activitiesCount2 = 0;
    lastTitle = "No Strava";
    lastLine = "Configure in Ibis Setup app";
    lastPolyline = "";
    lastTitle2 = ""; lastLine2 = ""; lastPolyline2 = "";
    return;
  }

  DBG_println("=== Fetching Strava Data ===");
  feedWatchdog();

  kmDone = 0; timeHours = 0; activitiesCount = 0;
  lastTitle = ""; lastLine = ""; lastPolyline = "";
  lastDistKm = 0; lastMovingSecs = 0; lastAvgSpeedKph = 0.0; lastDateStr = "";

  kmDone2 = 0; timeHours2 = 0; activitiesCount2 = 0;
  lastTitle2 = ""; lastLine2 = ""; lastPolyline2 = "";
  lastDistKm2 = 0; lastMovingSecs2 = 0; lastAvgSpeedKph2 = 0.0; lastDateStr2 = "";

  bool dualSport = (SPORT_TYPE2.length() > 0);
  
  if (accessToken == "") {
    DBG_println("No access token");
    return;
  }
  
  long afterTS, beforeTS;
  getTrackingPeriodTimestamps(afterTS, beforeTS);
  
  DBG_print("Tracking period: ");
  switch(TRACK_PERIOD) {
    case TRACK_WEEKLY: DBG_println("Weekly"); break;
    case TRACK_MONTHLY: DBG_println("Monthly"); break;
    default: DBG_println("Yearly"); break;
  }
  DBG_print("Filtering for sport type: ");
  DBG_print(SPORT_TYPE);
  if (SPORT_TYPE == SPORT_RUN) DBG_print(" (+ run variants)");
  if (SPORT_TYPE == SPORT_RIDE) DBG_print(" (+ cycling variants)");
  DBG_println();
  
  bool gotLast = false;
  bool gotLast2 = false;
  int page = 1;
  int totalActivities = 0;
  
  while (true) {
    feedWatchdog();
    
    HTTPClient http;
    String req = "https://www.strava.com/api/v3/athlete/activities?";
    req += "after=" + String(afterTS) + "&before=" + String(beforeTS);
    req += "&per_page=" + String(STRAVA_PER_PAGE) + "&page=" + String(page);
    
    http.setTimeout(25000);
    http.begin(req);
    http.addHeader("Authorization", "Bearer " + accessToken);
    
    int code = http.GET();
    
    if (code != 200) {
      DBG_print("API error: ");
      DBG_println(code);
      http.end();
      break;
    }

    String payload = http.getString();

    // Request only the fields needed for totals, labels, and map thumbnails.
    // Once the latest map(s) are found, drop polylines from later pages so
    // yearly totals can page through Strava without running out of JSON memory.
    bool needPolylines = !gotLast || (dualSport && !gotLast2);
    DynamicJsonDocument filter(1024);
    filter[0]["type"] = true;
    filter[0]["sport_type"] = true;
    filter[0]["name"] = true;
    filter[0]["start_date_local"] = true;
    filter[0]["distance"] = true;
    filter[0]["moving_time"] = true;
    filter[0]["average_speed"] = true;               // m/s from Strava
    if (needPolylines) filter[0]["map"]["summary_polyline"] = true;

    DynamicJsonDocument doc(49152);

    DeserializationError jsonError = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (jsonError) {
      USBSerial.print("JSON parse error: ");
      USBSerial.println(jsonError.c_str());
      http.end();
      break;
    }
    
    if (!doc.is<JsonArray>()) {
      DBG_println("Response is not an array");
      http.end();
      break;
    }
    
    JsonArray arr = doc.as<JsonArray>();
    
    if (arr.size() == 0) {
      DBG_println("No more activities");
      http.end();
      break;
    }
    
    DBG_print("Page ");
    DBG_print(page);
    DBG_print(": ");
    DBG_print(arr.size());
    DBG_println(" activities");
    
    for (JsonObject act : arr) {
      totalActivities++;

      String actType = act["sport_type"] | "";
      if (actType.length() == 0) actType = act["type"].as<String>();

      if (totalActivities <= 3) {
        DBG_print("  Activity sport/type: '");
        DBG_print(actType);
        DBG_println("'");
      }

      // Helper: parse date string into "day Month" format
      auto parseDate = [](const String& dt) -> String {
        if (dt.length() >= 10) {
          int month = dt.substring(5, 7).toInt();
          int day   = dt.substring(8, 10).toInt();
          const char* monthNames[] = {"January", "February", "March", "April",
                                      "May", "June", "July", "August",
                                      "September", "October", "November", "December"};
          if (month >= 1 && month <= 12) return String(day) + " " + monthNames[month - 1];
          return dt.substring(0, 10);
        }
        return dt;
      };

      // Check sport 1
      if (activityMatchesSportType(actType, SPORT_TYPE)) {
        if (!gotLast) {
          lastTitle = act["name"].as<String>();
          if (lastTitle.length() == 0) lastTitle = "Untitled " + SPORT_TYPE;
          lastDateStr = parseDate(act["start_date_local"].as<String>());
          float dkm = act["distance"].as<float>() / 1000.0;
          int movingSecs = act["moving_time"].as<int>();
          float avgSpeedKph = act["average_speed"].as<float>() * 3.6f;
          float hrs = movingSecs / 3600.0;
          if (dkm > 0 && dkm < 1000 && hrs > 0 && hrs < 100) {
            lastDistKm = dkm; lastMovingSecs = movingSecs; lastAvgSpeedKph = avgSpeedKph;
            lastLine = String(dkm, 1) + " km  " + String(hrs, 1) + "h  " + lastDateStr;
            gotLast = true;
            if (!act["map"]["summary_polyline"].isNull()) lastPolyline = act["map"]["summary_polyline"].as<String>();
          }
        }
        float distance = act["distance"].as<float>() / 1000.0;
        float time     = act["moving_time"].as<float>() / 3600.0;
        if (distance > 0 && distance < 1000 && time > 0 && time < 100) {
          kmDone += distance; timeHours += time; activitiesCount++;
        }
      }

      // Check sport 2
      if (dualSport && activityMatchesSportType(actType, SPORT_TYPE2)) {
        if (!gotLast2) {
          lastTitle2 = act["name"].as<String>();
          if (lastTitle2.length() == 0) lastTitle2 = "Untitled " + SPORT_TYPE2;
          lastDateStr2 = parseDate(act["start_date_local"].as<String>());
          float dkm = act["distance"].as<float>() / 1000.0;
          int movingSecs = act["moving_time"].as<int>();
          float avgSpeedKph = act["average_speed"].as<float>() * 3.6f;
          float hrs = movingSecs / 3600.0;
          if (dkm > 0 && dkm < 1000 && hrs > 0 && hrs < 100) {
            lastDistKm2 = dkm; lastMovingSecs2 = movingSecs; lastAvgSpeedKph2 = avgSpeedKph;
            lastLine2 = String(dkm, 1) + " km  " + String(hrs, 1) + "h  " + lastDateStr2;
            gotLast2 = true;
            if (!act["map"]["summary_polyline"].isNull()) lastPolyline2 = act["map"]["summary_polyline"].as<String>();
          }
        }
        float distance = act["distance"].as<float>() / 1000.0;
        float time     = act["moving_time"].as<float>() / 3600.0;
        if (distance > 0 && distance < 1000 && time > 0 && time < 100) {
          kmDone2 += distance; timeHours2 += time; activitiesCount2++;
        }
      }
    }
    
    http.end();
    page++;
    
    if (arr.size() < STRAVA_PER_PAGE) {
      DBG_println("Reached final short page");
      break;
    }
    
    if (page > STRAVA_MAX_PAGES) {
      DBG_print("Reached max pages (");
      DBG_print(STRAVA_MAX_PAGES);
      DBG_println(")");
      break;
    }
    
    delay(200);
  }
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[10];
    strftime(timeStr, sizeof(timeStr), "%d-%m-%y", &timeinfo);
    lastUpdateTime = String(timeStr);
    lastStravaFetchEpoch = mktime(&timeinfo);
  }
  
  DBG_println("=== Strava Fetch Complete ===");
  USBSerial.print("Total activities fetched: "); USBSerial.println(totalActivities);
  DBG_print("Matching activities ("); DBG_print(SPORT_TYPE); DBG_print("): ");
  DBG_println(activitiesCount);
  USBSerial.print("Sport1 activities: "); USBSerial.println(activitiesCount);
  USBSerial.print("Sport1 distance: "); USBSerial.print(kmDone, 1); USBSerial.println(" km");
  USBSerial.print("Sport1 time: "); USBSerial.print(timeHours, 1); USBSerial.println(" hours");
  if (dualSport) {
    USBSerial.print("Sport2 activities: "); USBSerial.println(activitiesCount2);
    USBSerial.print("Sport2 distance: "); USBSerial.print(kmDone2, 1); USBSerial.println(" km");
    USBSerial.print("Sport2 time: "); USBSerial.print(timeHours2, 1); USBSerial.println(" hours");
  }
  
  if (totalActivities > 0 && activitiesCount == 0) {
    DBG_println("WARNING: Got activities but none matched sport type!");
    DBG_print("Check that SPORT_TYPE='");
    DBG_print(SPORT_TYPE);
    DBG_println("' matches Strava activity types exactly");
  }
}

String middlewarePeriodParam() {
  switch (TRACK_PERIOD) {
    case TRACK_WEEKLY: return "weekly";
    case TRACK_MONTHLY: return "monthly";
    default: return "yearly";
  }
}

String normalizedMiddlewareUrl() {
  String url = MIDDLEWARE_URL;
  url.trim();
  while (url.endsWith("/")) url.remove(url.length() - 1);
  return url;
}

void resetDashboardGlobals() {
  kmDone = 0; timeHours = 0; activitiesCount = 0;
  lastTitle = ""; lastLine = ""; lastPolyline = "";
  lastDistKm = 0; lastMovingSecs = 0; lastAvgSpeedKph = 0.0; lastDateStr = "";

  kmDone2 = 0; timeHours2 = 0; activitiesCount2 = 0;
  lastTitle2 = ""; lastLine2 = ""; lastPolyline2 = "";
  lastDistKm2 = 0; lastMovingSecs2 = 0; lastAvgSpeedKph2 = 0.0; lastDateStr2 = "";
}

void applyMiddlewareSport(JsonObject item, bool secondSport) {
  String sportName = item["sport"] | "";
  JsonObject totals = item["totals"];
  float distanceKm = totals["distanceKm"] | 0.0f;
  int movingSeconds = totals["movingSeconds"] | 0;
  int count = totals["count"] | 0;

  float totalHours = movingSeconds / 3600.0f;
  String title = "Latest " + sportName;
  String line = "";
  String polyline = "";
  float latestDistKm = 0.0f;
  int latestMovingSecs = 0;
  float latestAvgSpeedKph = 0.0f;
  String dateLabel = "";

  if (!item["latest"].isNull()) {
    JsonObject latest = item["latest"];
    latestDistKm = latest["distanceKm"] | 0.0f;
    latestMovingSecs = latest["movingSeconds"] | 0;
    latestAvgSpeedKph = latest["avgSpeedKph"] | 0.0f;
    dateLabel = latest["dateLabel"] | "";
    polyline = latest["encodedPolyline"] | "";
    float hrs = latestMovingSecs / 3600.0f;
    line = String(latestDistKm, 1) + " km  " + String(hrs, 1) + "h  " + dateLabel;
  }

  if (secondSport) {
    kmDone2 = distanceKm;
    timeHours2 = totalHours;
    activitiesCount2 = count;
    lastTitle2 = title;
    lastLine2 = line;
    lastPolyline2 = polyline;
    lastDistKm2 = latestDistKm;
    lastMovingSecs2 = latestMovingSecs;
    lastAvgSpeedKph2 = latestAvgSpeedKph;
    lastDateStr2 = dateLabel;
  } else {
    kmDone = distanceKm;
    timeHours = totalHours;
    activitiesCount = count;
    lastTitle = title;
    lastLine = line;
    lastPolyline = polyline;
    lastDistKm = latestDistKm;
    lastMovingSecs = latestMovingSecs;
    lastAvgSpeedKph = latestAvgSpeedKph;
    lastDateStr = dateLabel;
  }
}

bool fetchMiddlewareDashboardData() {
  if (!hasMiddlewareDashboardCredentials()) {
    DBG_println("No Garmin middleware credentials - using placeholder data");
    resetDashboardGlobals();
    lastTitle = "No Garmin";
    lastLine = "Configure in Ibis Setup app";
    return false;
  }

  DBG_println("=== Fetching Garmin Middleware Dashboard ===");
  feedWatchdog();
  resetDashboardGlobals();

  String sportsParam = SPORT_TYPE;
  if (SPORT_TYPE2.length() > 0) sportsParam += "," + SPORT_TYPE2;

  String url = normalizedMiddlewareUrl();
  if (url.length() == 0) return false;
  url += "/api/ibis/dashboard?period=" + middlewarePeriodParam();
  url += "&sports=" + urlEncode(sportsParam);
  url += "&tz=Europe%2FAmsterdam";

  HTTPClient http;
  http.setTimeout(30000);
  http.setUserAgent("IbisDash/5.2 ESP32");
  if (!http.begin(url)) {
    DBG_println("Middleware HTTP begin failed");
    return false;
  }
  http.addHeader("X-App-Key", MIDDLEWARE_APP_KEY);
  http.addHeader("X-Ibis-Key", IBIS_TOKEN);

  int code = http.GET();
  if (code != 200) {
    DBG_print("Middleware API error: ");
    DBG_println(code);
    String err = http.getString();
    if (err.length() > 0) {
      USBSerial.print("Middleware error: ");
      USBSerial.println(err.substring(0, 160));
    }
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(24576);
  DeserializationError jsonError = deserializeJson(doc, payload);
  if (jsonError) {
    USBSerial.print("Dashboard JSON parse error: ");
    USBSerial.println(jsonError.c_str());
    return false;
  }

  JsonObject period = doc["period"];
  dashYear = period["year"] | dashYear;
  dashMonth = period["month"] | dashMonth;
  dashWeek = period["week"] | dashWeek;

  bool appliedSport1 = false;
  bool appliedSport2 = false;
  JsonArray sports = doc["sports"].as<JsonArray>();
  for (JsonObject item : sports) {
    String sportName = item["sport"] | "";
    if (sportName == SPORT_TYPE) {
      applyMiddlewareSport(item, false);
      appliedSport1 = true;
    } else if (SPORT_TYPE2.length() > 0 && sportName == SPORT_TYPE2) {
      applyMiddlewareSport(item, true);
      appliedSport2 = true;
    }
  }

  if (!appliedSport1 && sports.size() > 0) {
    applyMiddlewareSport(sports[0], false);
    appliedSport1 = true;
  }
  if (SPORT_TYPE2.length() > 0 && !appliedSport2 && sports.size() > 1) {
    applyMiddlewareSport(sports[1], true);
    appliedSport2 = true;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[10];
    strftime(timeStr, sizeof(timeStr), "%d-%m-%y", &timeinfo);
    lastUpdateTime = String(timeStr);
    lastStravaFetchEpoch = mktime(&timeinfo);
    if (dashYear == 0) dashYear = timeinfo.tm_year + 1900;
  }

  USBSerial.println("DASHBOARD_OK");
  USBSerial.print("Sport1 activities: "); USBSerial.println(activitiesCount);
  USBSerial.print("Sport1 distance: "); USBSerial.print(kmDone, 1); USBSerial.println(" km");
  USBSerial.print("Sport1 time: "); USBSerial.print(timeHours, 1); USBSerial.println(" hours");
  if (SPORT_TYPE2.length() > 0) {
    USBSerial.print("Sport2 activities: "); USBSerial.println(activitiesCount2);
    USBSerial.print("Sport2 distance: "); USBSerial.print(kmDone2, 1); USBSerial.println(" km");
    USBSerial.print("Sport2 time: "); USBSerial.print(timeHours2, 1); USBSerial.println(" hours");
  }

  return appliedSport1;
}

bool shouldFetchStrava() {
  if (lastStravaFetchEpoch == 0) return true;
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return true;
  
  time_t currentEpoch = mktime(&timeinfo);
  time_t timeSinceLastFetch = currentEpoch - lastStravaFetchEpoch;
  unsigned long fetchIntervalSeconds = (unsigned long)REFRESH_HOURS * 3600;
  
  return (timeSinceLastFetch >= (time_t)fetchIntervalSeconds);
}

// ── Change #1: decide whether the display actually needs to be redrawn ────────
// Returns true  → data changed, must draw
// Returns false → nothing new, skip draw (unless forceRedraw is true)
bool dataHasChangedSinceLastDraw() {
  if (lastDrawnActivitiesCount != activitiesCount) {
    DBG_println("[smart-redraw] Sport1 activity count changed → must draw");
    return true;
  }
  if (fabsf(lastDrawnKmDone - kmDone) >= 0.05f) {
    DBG_println("[smart-redraw] Sport1 distance changed → must draw");
    return true;
  }
  if (SPORT_TYPE2.length() > 0) {
    if (lastDrawnActivitiesCount2 != activitiesCount2) {
      DBG_println("[smart-redraw] Sport2 activity count changed → must draw");
      return true;
    }
    if (fabsf(lastDrawnKmDone2 - kmDone2) >= 0.05f) {
      DBG_println("[smart-redraw] Sport2 distance changed → must draw");
      return true;
    }
  }
  DBG_println("[smart-redraw] Data unchanged → skipping display refresh");
  return false;
}

void recordDrawnSnapshot() {
  lastDrawnKmDone          = kmDone;
  lastDrawnActivitiesCount = activitiesCount;
  lastDrawnKmDone2          = kmDone2;
  lastDrawnActivitiesCount2 = activitiesCount2;
}


// =============================================================================
// SECTION 16: MAIN DASHBOARD DISPLAY
// =============================================================================

String getHeaderTitle() {
  if (CUSTOM_TITLE.length() > 0) return CUSTOM_TITLE;
  
  String name = (USER_NAME.length() > 0) ? USER_NAME + "'s " : "";
  String title = "";
  
  switch (TRACK_PERIOD) {
    case TRACK_WEEKLY: {
      title = name + "Garmin Stats Week " + String(dashWeek);
      break;
    }
    case TRACK_MONTHLY: {
      const char* months[] = {"January", "February", "March", "April",
                               "May", "June", "July", "August",
                               "September", "October", "November", "December"};
      int mi = dashMonth - 1;
      if (mi < 0) mi = 0;
      if (mi > 11) mi = 11;
      title = name + "Garmin Stats " + months[mi];
      break;
    }
    default:
      // Yearly — keep original behaviour
      title = name + "Garmin Stats " + String(dashYear);
      break;
  }
  
  return title;
}

String getActivityLabelFor(const String& sport) {
  if (sport == "Run")  return "RUNS";
  if (sport == "Ride") return "RIDES";
  if (sport == "Swim") return "SWIMS";
  if (sport == "Hike") return "HIKES";
  if (sport == "Walk") return "WALKS";
  return "ACTIVITIES";
}
String getActivityLabel() { return getActivityLabelFor(SPORT_TYPE); }

String getSportTitle(const String& sport) {
  if (sport == "Run")  return "RUNNING";
  if (sport == "Ride") return "CYCLING";
  if (sport == "Swim") return "SWIMMING";
  if (sport == "Hike") return "HIKING";
  if (sport == "Walk") return "WALKING";
  return sport;
}

String getLastLabel(const String& sport) {
  if (sport == "Ride") return "Last ride";
  if (sport == "Run")  return "Last run";
  if (sport == "Swim") return "Last swim";
  if (sport == "Hike") return "Last hike";
  if (sport == "Walk") return "Last walk";
  return "Last " + sport;
}

String getLatestActivityLabel() {
  if (SPORT_TYPE == "Ride") return "LATEST RIDE";
  if (SPORT_TYPE == "Run")  return "LATEST RUN";
  if (SPORT_TYPE == "Swim") return "LATEST SWIM";
  if (SPORT_TYPE == "Hike") return "LATEST HIKE";
  if (SPORT_TYPE == "Walk") return "LATEST WALK";
  return "LATEST " + SPORT_TYPE;
}

void drawDashboard() {
  DBG_println("=== Drawing Dashboard ===");
  feedWatchdog();

#ifndef MAPS_DISABLED
  // Always fetch map images if not already done
  if (!prefetchedMap1Valid) {
    DBG_println("Maps not prefetched, fetching now...");
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
      delay(1000);
      feedWatchdog();
    }
    if (WiFi.status() == WL_CONNECTED) {
      prefetchMapImages();
      disconnectWiFi();
    } else {
      mapDebugMsg1 = "WiFi fail";
      mapDebugMsg2 = "WiFi fail";
    }
    feedWatchdog();
  }
#endif
  
  delay(1000);
  feedWatchdog();
  
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  delay(500);
  
  float currentVoltage = PMU.getBattVoltage() / 1000.0;
  bool usbOK = PMU.isVbusIn();
  
  if (usbOK && currentVoltage < 3.5) {
    delay(2000);
    feedWatchdog();
    currentVoltage = PMU.getBattVoltage() / 1000.0;
    if (currentVoltage < 3.3) {
      DBG_println("Power unstable - skipping refresh");
      return;
    }
  }
  
  DBG_println("Reinitializing display hardware...");
  SPI.end();
  delay(100);
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  epd_deep_init();
  display.init(115200, true, 2, false);
  display.setRotation(2);
  delay(200);
  feedWatchdog();
  
  bool dualSport = (SPORT_TYPE2.length() > 0);

  float pct = (YEARLY_GOAL > 0) ? (kmDone / YEARLY_GOAL) : 0;
  if (pct < 0) pct = 0;
  bool goalExceeded = (pct > 1.0f);
  float displayPct = (pct > 1.0f) ? 1.0f : pct;

  // Sport 2 progress
  float pct2 = (YEARLY_GOAL2 > 0) ? (kmDone2 / YEARLY_GOAL2) : 0;
  if (pct2 < 0) pct2 = 0;
  bool goalExceeded2 = (pct2 > 1.0f);
  float displayPct2 = (pct2 > 1.0f) ? 1.0f : pct2;

  if (lowBatteryMode) {
    C_HEADER = GxEPD_BLUE;
    C_TEXT_DIM = GxEPD_BLUE;
  } else {
    C_HEADER = GxEPD_RED;
    C_TEXT_DIM = GxEPD_RED;
  }

  DBG_println("Starting display refresh...");
  USBSerial.flush();
  delay(200);

  // esp_task_wdt_delete(NULL);

#include "dashboard_new.h"

  // (old dashboard code was here)

  epd_wait_busy();
  // esp_task_wdt_add(NULL);
  feedWatchdog();
  delay(1000);
  yield();
  USBSerial.flush();
  delay(500);

  // ── Record snapshot so we can detect future changes ─────────────────────
  recordDrawnSnapshot();
  
  DBG_println("Dashboard update complete!");
}


// =============================================================================
// SECTION 17: HIGH-LEVEL UPDATE FUNCTIONS
// =============================================================================

bool fetchStravaDataWithValidation() {
  DBG_println("\n=== FETCHING & VALIDATING STRAVA DATA ===");
  
  DBG_println("Step 1: Connecting WiFi...");
  connectWiFi();
  
  if (WiFi.status() != WL_CONNECTED) {
    DBG_println("WiFi connection failed!");
    return false;
  }
  USBSerial.println("[OK] WiFi connected");
  delay(1000);
  feedWatchdog();
  
  DBG_println("Step 2: Syncing time...");
  initTime();
  delay(500);
  feedWatchdog();
  
  DBG_println("Step 3: Refreshing access token...");
  if (!refreshAccessToken()) {
    DBG_println("[FAIL] Token refresh failed!");
    disconnectWiFi();
    return false;
  }
  USBSerial.println("[OK] Access token refreshed");
  delay(500);
  feedWatchdog();
  
  DBG_println("Step 4: Fetching Strava activities...");
  fetchStravaData();
  delay(500);
  feedWatchdog();

  DBG_println("Step 4b: Fetching map images...");
#ifndef MAPS_DISABLED
  prefetchMapImages();
#endif
  feedWatchdog();

  disconnectWiFi();
  
  DBG_println("Step 5: Validating results...");
  USBSerial.print("  Activities: "); DBG_println(activitiesCount);
  USBSerial.print("  Distance: "); USBSerial.print(kmDone, 1); DBG_println(" km");
  USBSerial.print("  Time: "); USBSerial.print(timeHours, 1); DBG_println(" hours");
  
  bool dataValid = (kmDone >= 0 && kmDone < 100000 && 
                    timeHours >= 0 && timeHours < 50000 &&
                    activitiesCount >= 0 && activitiesCount < 10000);
  
  if (dataValid) USBSerial.println("[OK] Data validation passed!");
  else           DBG_println("[FAIL] Data validation failed - values out of range!");
  
  return dataValid;
}

bool fetchDashboardDataWithValidation() {
  if (!hasMiddlewareDashboardCredentials()) {
    return fetchStravaDataWithValidation();
  }

  DBG_println("\n=== FETCHING & VALIDATING GARMIN DASHBOARD DATA ===");

  DBG_println("Step 1: Connecting WiFi...");
  connectWiFi();

  if (WiFi.status() != WL_CONNECTED) {
    DBG_println("WiFi connection failed!");
    return false;
  }
  USBSerial.println("[OK] WiFi connected");
  delay(1000);
  feedWatchdog();

  DBG_println("Step 2: Syncing time...");
  initTime();
  delay(500);
  feedWatchdog();

  DBG_println("Step 3: Fetching Garmin dashboard payload...");
  bool fetched = fetchMiddlewareDashboardData();
  delay(500);
  feedWatchdog();

  if (fetched) {
    DBG_println("Step 4: Fetching map images...");
#ifndef MAPS_DISABLED
    prefetchMapImages();
#endif
  }
  feedWatchdog();

  disconnectWiFi();

  DBG_println("Step 5: Validating results...");
  USBSerial.print("  Activities: "); DBG_println(activitiesCount);
  USBSerial.print("  Distance: "); USBSerial.print(kmDone, 1); DBG_println(" km");
  USBSerial.print("  Time: "); USBSerial.print(timeHours, 1); DBG_println(" hours");

  bool dataValid = fetched &&
                   kmDone >= 0 && kmDone < 100000 &&
                   timeHours >= 0 && timeHours < 50000 &&
                   activitiesCount >= 0 && activitiesCount < 10000;

  if (dataValid) USBSerial.println("[OK] Data validation passed!");
  else           DBG_println("[FAIL] Data validation failed - values out of range!");

  return dataValid;
}

// ── Change #1: forceRedraw bypasses the change-detection check ───────────────
void updateStravaAndDisplay(bool forceFetch, bool forceRedraw) {
  DBG_println("\n========== FULL UPDATE ==========");
  feedWatchdog();
  
  bool dataFetched = false;
  
  if (forceFetch || shouldFetchStrava()) {
    dataFetched = fetchDashboardDataWithValidation();
    
    if (!dataFetched) {
      DBG_println("WARNING: Could not fetch dashboard data!");
    }
  } else {
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) {
      initTime();
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char timeStr[10];
        strftime(timeStr, sizeof(timeStr), "%d-%m-%y", &timeinfo);
        lastUpdateTime = String(timeStr);
        dashYear = timeinfo.tm_year + 1900;
      }
#ifndef MAPS_DISABLED
      prefetchMapImages();
#endif
      feedWatchdog();
    }
    disconnectWiFi();
  }

  printBatteryStatus();
  
  // ── Change #1: skip draw if nothing changed (unless forced) ──────────────
  if (!forceRedraw && !dataHasChangedSinceLastDraw()) {
    DBG_println(">>> No new data - skipping screen redraw <<<");
    DBG_println("========== UPDATE COMPLETE (no draw) ==========\n");
    return;
  }
  
  DBG_println("Drawing dashboard...");
  delay(500);
  feedWatchdog();
  
  drawDashboard();
  resetCrashCounter();
  
  DBG_println("========== UPDATE COMPLETE ==========\n");
}

// Convenience overload — existing callers that pass only 2 args still work
void updateStravaAndDisplay(bool forceFetch) {
  updateStravaAndDisplay(forceFetch, false);
}

void updateDisplayOnly() {
  DBG_println("\n=== DISPLAY REFRESH (cached data) ===");
  feedWatchdog();
  printBatteryStatus();
  
  SPI.end();
  delay(100);
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  epd_deep_init();
  display.init(115200, true, 2, false);
  display.setRotation(2);
  feedWatchdog();
  
  drawDashboard();
}


// =============================================================================
// SECTION 18: SERIAL COMMAND HANDLER (for Ibis Setup app)
// =============================================================================

void handleSerialCommands() {
  while (USBSerial.available()) {
    char c = USBSerial.read();
    
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        processSerialCommand(serialInputBuffer);
        serialInputBuffer = "";
      }
    } else {
      if (serialInputBuffer.length() < 4096) {
        serialInputBuffer += c;
      }
    }
  }
}

void processSerialCommand(String command) {
  command.trim();
  
  DBG_print("CMD[");
  DBG_print(command.length());
  DBG_print("]: ");
  if (command.length() > 50) {
    DBG_print(command.substring(0, 50));
    DBG_println("...");
  } else {
    DBG_println(command);
  }
  
  if (command == "GET_CONFIG") {
    sendCurrentConfig();
  }
  else if (command.startsWith("SET_CONFIG:")) {
    String jsonStr = command.substring(11);
    saveConfigFromSerial(jsonStr);
  }
  else if (command == "WIPE_CONFIG") {
    wipeConfig();
  }
  else if (command == "DELETE_DATA") {
    wipeConfigAndShowSetup();
  }
  else if (command == "TEST_WIFI") {
    loadConfiguration();
    if (WIFI_SSID.length() == 0) {
      USBSerial.println("NO_WIFI_CREDENTIALS");
    } else if (testWiFiConnection()) {
      USBSerial.println("WIFI_OK");
    } else {
      USBSerial.println("WIFI_FAILED");
    }
  }
  else if (command == "FETCH_DASHBOARD") {
    DBG_println("\n=== FINISH SETUP DASHBOARD FETCH ===");
    loadConfiguration();

    if (WIFI_SSID.length() == 0) {
      USBSerial.println("NO_WIFI_CREDENTIALS");
    } else if (!hasDashboardCredentials()) {
      USBSerial.println("NO_DASHBOARD_CREDENTIALS");
    } else {
      bool ok = fetchDashboardDataWithValidation();
      if (!ok) {
        USBSerial.println("DASHBOARD_FETCH_FAILED");
      } else {
        printBatteryStatus();
        DBG_println("Drawing dashboard...");
        drawDashboard();
        USBSerial.println("DASHBOARD_DRAWN");

        if (!PMU.isVbusIn()) {
          DBG_println(">>> On battery - will sleep <<<");
          sleepRequested = true;
        }
      }
    }
  }
  else if (command == "FETCH_STRAVA") {
    DBG_println("\n=== FINISH SETUP ===");
    loadConfiguration();
    
    if (WIFI_SSID.length() == 0) {
      USBSerial.println("NO_WIFI_CREDENTIALS");
    } else if (!hasStravaCredentials()) {
      USBSerial.println("NO_STRAVA_CREDENTIALS");
    } else {
      DBG_println("Connecting to WiFi...");
      connectWiFi();
      
      if (WiFi.status() != WL_CONNECTED) {
        USBSerial.println("WIFI_CONNECT_FAILED");
      } else {
        DBG_println("Syncing time...");
        initTime();
        feedWatchdog();
        
        DBG_println("Refreshing token...");
        if (!refreshAccessToken()) {
          USBSerial.println("TOKEN_REFRESH_FAILED");
          disconnectWiFi();
        } else {
          DBG_println("Fetching Strava data...");
          feedWatchdog();
          fetchStravaData();
          feedWatchdog();
          DBG_println("Fetching map images...");
#ifndef MAPS_DISABLED
          prefetchMapImages();
#endif
          disconnectWiFi();
          
          USBSerial.println("STRAVA_OK");
          USBSerial.print("Activities: "); USBSerial.println(activitiesCount);
          USBSerial.print("Distance: "); USBSerial.print(kmDone); USBSerial.println(" km");
          USBSerial.print("Time: "); USBSerial.print(timeHours); USBSerial.println(" hours");
          if (SPORT_TYPE2.length() > 0) {
            USBSerial.print("Activities2: "); USBSerial.println(activitiesCount2);
            USBSerial.print("Distance2: "); USBSerial.print(kmDone2); USBSerial.println(" km");
            USBSerial.print("Time2: "); USBSerial.print(timeHours2); USBSerial.println(" hours");
          }
          
          printBatteryStatus();
          DBG_println("Drawing dashboard...");
          drawDashboard();  // Always draws after FETCH_STRAVA command
          USBSerial.println("DASHBOARD_DRAWN");
          
          if (!PMU.isVbusIn()) {
            DBG_println(">>> On battery - will sleep <<<");
            sleepRequested = true;
          }
        }
      }
    }
  }
  else if (command == "SHOW_SETUP_SCREEN") {
    DBG_println("Drawing setup screen...");
    SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    epd_deep_init();
    display.init(115200, true, 2, false);
    display.setRotation(2);
    drawSetupScreen();
    USBSerial.println("SETUP_SCREEN_DRAWN");
  }
  else if (command == "GO_SLEEP") {
    DBG_println("Sleep requested");
    USBSerial.println("OK");
    delay(100);
    sleepRequested = true;
  }
  else if (command == "RESTART") {
    USBSerial.println("OK");
    DBG_println("Restarting...");
    delay(500);
    ESP.restart();
  }
  else if (command == "PING") {
    USBSerial.println("PONG");
    USBSerial.println("IBIS_DASH_V51");
  }
  else if (command.length() > 0) {
    DBG_print("Unknown command: ");
    DBG_println(command);
    USBSerial.println("ERROR");
  }
}

void sendCurrentConfig() {
  DBG_println("Sending configuration...");
  
  preferences.begin("config", true);
  
  DynamicJsonDocument doc(6144);
  
  doc["ssid"]          = preferences.getString("ssid", "");
  doc["password"]      = preferences.getString("password", "");
  doc["name"]          = preferences.getString("name", "");
  doc["sport"]         = preferences.getString("sport", "Run");
  doc["sport2"]        = preferences.getString("sport2", "");
  doc["goal"]          = preferences.getFloat("goal", 1000.0);
  doc["goal2"]         = preferences.getFloat("goal2", 0);
  doc["clientID"]      = preferences.getString("clientID", "");
  doc["clientSecret"]  = preferences.getString("clientSecret", "");
  doc["refreshToken"]  = preferences.getString("refreshToken", "");
  doc["dataSource"]    = preferences.getString("dataSource", "garmin_middleware");
  doc["middlewareUrl"] = preferences.getString("middlewareUrl", "");
  doc["middlewareAppKey"] = preferences.getString("mwAppKey", "");
  doc["ibisToken"]     = preferences.getString("ibisToken", "");
  doc["refreshHours"]  = preferences.getInt("refreshHours", 12);
  doc["trackPeriod"]   = preferences.getInt("trackPeriod", TRACK_YEARLY);
  doc["title"]         = preferences.getString("title", "");
  doc["mapsApiKey"]    = preferences.getString("mapsApiKey", "");
  doc["configured"]    = (preferences.getString("ssid", "").length() > 0);
  doc["hasStrava"]     = (preferences.getString("clientID", "").length() > 0);
  doc["hasDashboard"]  = (preferences.getString("middlewareUrl", "").length() > 0 &&
                          preferences.getString("mwAppKey", "").length() > 0 &&
                          preferences.getString("ibisToken", "").length() > 0);
  doc["firmwareVersion"] = "5.1";
  doc["usbIdentity"]   = "Ibis Dash";
  
  preferences.end();

  if (doc.overflowed()) {
    USBSerial.println("{\"error\":\"config_json_overflow\"}");
    USBSerial.println("OK");
    return;
  }
  
  String output;
  serializeJson(doc, output);
  DBG_println(output);
  USBSerial.println(output);
  USBSerial.println("OK");
}

void saveConfigFromSerial(String jsonStr) {
  DBG_println("Parsing configuration...");
  USBSerial.print("JSON length: ");
  DBG_println(jsonStr.length());
  
  DynamicJsonDocument doc(6144);
  DeserializationError error = deserializeJson(doc, jsonStr);
  
  if (error) {
    USBSerial.print("JSON error: ");
    DBG_println(error.c_str());
    USBSerial.println("ERROR");
    return;
  }
  
  USBSerial.println("JSON parsed OK, saving to NVS...");
  
  preferences.begin("config", false);
  
  if (doc.containsKey("ssid"))          { preferences.putString("ssid",          doc["ssid"].as<String>());          DBG_println("  - ssid saved"); }
  if (doc.containsKey("password"))      { preferences.putString("password",      doc["password"].as<String>());      DBG_println("  - password saved"); }
  if (doc.containsKey("name"))          { preferences.putString("name",          doc["name"].as<String>()); }
  if (doc.containsKey("sport"))         { preferences.putString("sport",         doc["sport"].as<String>()); }
  if (doc.containsKey("sport2"))        { preferences.putString("sport2",        doc["sport2"].as<String>()); }
  if (doc.containsKey("goal"))          { preferences.putFloat ("goal",          doc["goal"].as<float>()); }
  if (doc.containsKey("goal2"))         { preferences.putFloat ("goal2",         doc["goal2"].as<float>()); }
  if (doc.containsKey("clientID"))      { preferences.putString("clientID",      doc["clientID"].as<String>());      DBG_println("  - clientID saved"); }
  if (doc.containsKey("clientSecret"))  { preferences.putString("clientSecret",  doc["clientSecret"].as<String>());  DBG_println("  - clientSecret saved"); }
  if (doc.containsKey("refreshToken"))  { preferences.putString("refreshToken",  doc["refreshToken"].as<String>());  DBG_println("  - refreshToken saved"); }
  if (doc.containsKey("dataSource"))    { preferences.putString("dataSource",    doc["dataSource"].as<String>()); }
  if (doc.containsKey("middlewareUrl")) { preferences.putString("middlewareUrl", doc["middlewareUrl"].as<String>()); DBG_println("  - middlewareUrl saved"); }
  if (doc.containsKey("middlewareAppKey")) { preferences.putString("mwAppKey", doc["middlewareAppKey"].as<String>()); DBG_println("  - middlewareAppKey saved"); }
  if (doc.containsKey("ibisToken"))     { preferences.putString("ibisToken",     doc["ibisToken"].as<String>());     DBG_println("  - ibisToken saved"); }
  if (doc.containsKey("refreshHours"))  { preferences.putInt   ("refreshHours",  doc["refreshHours"].as<int>()); }
  if (doc.containsKey("trackPeriod"))   { preferences.putInt   ("trackPeriod",   doc["trackPeriod"].as<int>()); }
  if (doc.containsKey("title"))         { preferences.putString("title",         doc["title"].as<String>()); }
  if (doc.containsKey("mapsApiKey"))   { preferences.putString("mapsApiKey",   doc["mapsApiKey"].as<String>());   DBG_println("  - mapsApiKey saved"); }

  preferences.end();
  
  loadConfiguration();
  
  if (doc.containsKey("clientID") || doc.containsKey("clientSecret") || doc.containsKey("refreshToken")) {
    cachedAccessToken[0] = '\0';
    tokenExpiresAt = 0;
    DBG_println("  - Token cache cleared");
  }
  
  // Changing config may change what data is shown — invalidate snapshot so
  // the next fetch always redraws
  lastDrawnKmDone          = -1.0;
  lastDrawnActivitiesCount = -1;
  
  DBG_println("Configuration saved!");
  USBSerial.println("SUCCESS");
}

void wipeConfig() {
  DBG_println("Wiping all configuration...");
  
  preferences.begin("config", false);
  preferences.clear();
  preferences.end();
  
  WIFI_SSID = "";
  WIFI_PASS = "";
  CLIENT_ID = "";
  CLIENT_SECRET = "";
  REFRESH_TOKEN = "";
  DATA_SOURCE = "garmin_middleware";
  MIDDLEWARE_URL = "";
  MIDDLEWARE_APP_KEY = "";
  IBIS_TOKEN = "";
  USER_NAME = "";
  CUSTOM_TITLE = "";
  isConfigured = false;
  
  kmDone = 0; timeHours = 0; activitiesCount = 0;
  kmDone2 = 0; timeHours2 = 0; activitiesCount2 = 0;
  lastStravaFetchEpoch = 0;
  lastNtpSyncEpoch = 0;

  lastTitle = ""; lastLine = ""; lastPolyline = "";
  lastUpdateTime = "";
  lastDistKm = 0; lastMovingSecs = 0; lastAvgSpeedKph = 0.0; lastDateStr = "";

  lastTitle2 = ""; lastLine2 = ""; lastPolyline2 = "";
  lastDistKm2 = 0; lastMovingSecs2 = 0; lastAvgSpeedKph2 = 0.0; lastDateStr2 = "";

  cachedAccessToken[0] = '\0';
  tokenExpiresAt = 0;

  lastDrawnKmDone = -1.0; lastDrawnActivitiesCount = -1;
  lastDrawnKmDone2 = -1.0; lastDrawnActivitiesCount2 = -1;
  
  DBG_println("Configuration wiped!");
  USBSerial.println("WIPED");
}

void wipeConfigAndShowSetup() {
  wipeConfig();
  DBG_println("Drawing setup screen...");
  drawSetupScreen();
  USBSerial.println("SETUP_SCREEN_DRAWN");
}


// =============================================================================
// SECTION 19: SLEEP & WAKE
// =============================================================================

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  wasManualWake = false;
  
  DBG_println("\n========== WAKE UP REASON ==========");
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      DBG_println("Wakeup: USB connected! (GPIO 21 - PMU IRQ)");
      wasManualWake = false;
      
      delay(500);
      if (PMU.isVbusIn()) {
        DBG_println("  ✓ USB confirmed connected");
        DBG_println("  → Staying idle, waiting for serial commands");
      } else {
        DBG_println("  ⚠ USB not detected (false wake?)");
      }
      break;
      
    case ESP_SLEEP_WAKEUP_EXT1:
      DBG_println("Wakeup: BOOT button pressed");
      wasManualWake = true;
      DBG_println("  → Will fetch fresh Strava data");
      break;
      
    case ESP_SLEEP_WAKEUP_TIMER:
      DBG_println("Wakeup: Timer expired (scheduled refresh)");
      DBG_println("  → Will fetch fresh Strava data");
      break;
      
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      DBG_println("Wakeup: Power-on or Reset button");
      break;
      
    default:
      DBG_print("Wakeup: Unknown reason (");
      DBG_print(wakeup_reason);
      DBG_println(")");
      break;
  }
  
  DBG_println("====================================\n");
}

void go_to_deep_sleep() {
  DBG_println("\n========== ENTERING DEEP SLEEP ==========");
  
  bool usbNowConnected = PMU.isVbusIn();
  DBG_print("USB status before sleep: ");
  DBG_println(usbNowConnected ? "CONNECTED" : "DISCONNECTED");
  
  if (usbNowConnected) {
    DBG_println("WARNING: USB is connected - should not sleep!");
    DBG_println("Returning to loop() instead of sleeping...");
    return;
  }
  
  preferences.begin("config", true);
  REFRESH_HOURS = preferences.getInt("refreshHours", 12);
  bool hasConfig = (preferences.getString("ssid", "").length() > 0);
  preferences.end();
  
  uint64_t sleepDuration;
  if (!hasConfig) {
    sleepDuration = SLEEP_DURATION_UNCONFIGURED_US;
    DBG_println("Not configured - sleeping 1 week");
  } else {
    sleepDuration = (uint64_t)REFRESH_HOURS * 60ULL * 60ULL * 1000000ULL;
    DBG_print("Sleeping for ");
    DBG_print(REFRESH_HOURS);
    DBG_println(" hours");
  }
  
  feedWatchdog();
  disconnectWiFi();
  
  DBG_println("Configuring wake sources:");
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  
  esp_sleep_enable_timer_wakeup(sleepDuration);
  DBG_print("  ✓ Timer: ");
  DBG_print(REFRESH_HOURS);
  DBG_println(" hours");
  
  const uint64_t button_mask = 1ULL << GPIO_NUM_0;
  esp_sleep_enable_ext1_wakeup(button_mask, ESP_EXT1_WAKEUP_ANY_LOW);
  DBG_println("  ✓ BOOT button (GPIO 0)");
  
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_21, 0);
  DBG_println("  ✓ USB connection (GPIO 21 - PMU IRQ)");
  
  pmu_prepare_for_esp32_sleep();
  
  DBG_println("Going to sleep NOW...");
  DBG_println("Wake triggers: Timer | BOOT button | USB plug-in");
  USBSerial.flush();
  
  digitalWrite(ACT_LED_PIN, LOW);
  // esp_task_wdt_delete(NULL);
  
  delay(100);
  
  esp_deep_sleep_start();
}


// =============================================================================
// SECTION 20: ARDUINO SETUP
// =============================================================================

RTC_DATA_ATTR bool setupScreenDrawn = false;

void setup() {
  ibisDisableBrownout();

  Wire.begin(PMU_SDA, PMU_SCL);
  delay(10);
  
  bool pmuReady = PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  if (pmuReady) {
    pmu_configure_awake();
  }

  initUSBComposite();
  delay(250);
  USBSerial.println("IBIS_BOOT");
  if (!pmuReady) {
    USBSerial.println("[WARN] PMU early init failed");
  }
  
  loadConfiguration();
  
  pinMode(ACT_LED_PIN, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_BUSY, INPUT);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_CS, OUTPUT);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PWR_BUTTON_PIN, INPUT_PULLUP);
  
  digitalWrite(ACT_LED_PIN, LOW);
  digitalWrite(EPD_RST, HIGH);
  digitalWrite(EPD_DC, LOW);
  digitalWrite(EPD_CS, HIGH);
  
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  DBG_println("\n======================================================================");
  DBG_println("              IBIS DASH V5.1 - Smart Refresh + Cycling");
  DBG_println("              Board identifies as: Ibis Dash (CDC+HID)");
  DBG_println("======================================================================");
  DBG_print("Configured: "); DBG_println(isConfigured ? "YES" : "NO");
  DBG_print("Boot count: "); DBG_println(bootCount);
  USBSerial.print("USB Composite: "); USBSerial.println(usbCompositeStarted ? "ACTIVE" : "FAILED");
  
  initWatchdog();
  
  DBG_println("Checking factory reset...");
  if (checkFactoryReset()) {
    setupScreenDrawn = false;
  }
  USBSerial.println("[OK] No factory reset");
  
  DBG_println("Checking crash loop...");
  if (checkCrashLoop()) {
    enterSafeMode();
  }
  USBSerial.println("[OK] No crash loop");
  
  DBG_println("Initializing PMU IRQ...");
  pmu_irq_init();
  USBSerial.println("[OK] PMU IRQ ready");
  
  delay(900);
  DBG_println("Verifying PMU...");
  if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    DBG_println("PMU init failed!");
  } else {
    USBSerial.println("[OK] PMU verified");
  }
  
  print_wakeup_reason();
  
  DBG_println("Configuring PMU for awake state...");
  pmu_configure_awake();
  USBSerial.println("[OK] PMU configured");
  
  DBG_println("PMU stabilization (2s)...");
  delay(2000);
  feedWatchdog();
  printBatteryStatus();
  
  bool usbConnected = PMU.isVbusIn() || (bool)USBSerial || (esp_reset_reason() == ESP_RST_USB);
  DBG_print("USB Status: ");
  DBG_println(usbConnected ? "CONNECTED" : "DISCONNECTED");

  if (usbConnected) {
    resetCrashCounter();
    USBSerial.println("[OK] USB connected - setup/edit mode");
    USBSerial.println("READY");
    return;
  }
  
  DBG_println("Initializing display...");
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  epd_deep_init();
  display.init(115200, true, 2, false);
  display.setRotation(2);
  USBSerial.println("[OK] Display ready");
  
  // ===== MAIN LOGIC =====
  
  bool hasWifi  = (WIFI_SSID.length() > 0);
  bool hasDashboard = hasDashboardCredentials();
  bool fullyConfigured = hasWifi && hasDashboard;
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool isUsbWake    = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0);
  bool isTimerWake  = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
  bool isButtonWake = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1);
  bool isPowerOn    = (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED);
  
  USBSerial.print("Has WiFi: ");    DBG_println(hasWifi       ? "YES" : "NO");
  USBSerial.print("Has Dashboard: "); DBG_println(hasDashboard ? "YES" : "NO");
  USBSerial.print("Fully Configured: "); DBG_println(fullyConfigured ? "YES" : "NO");
  DBG_print("Wake Type: ");
  if (isUsbWake)    DBG_println("USB");
  else if (isTimerWake)  DBG_println("TIMER");
  else if (isButtonWake) DBG_println("BUTTON");
  else if (isPowerOn)    DBG_println("POWER-ON");
  else                   DBG_println("UNKNOWN");
  
  if (!fullyConfigured) {
    DBG_println("\n>>> SETUP REQUIRED <<<");
    
    if (!setupScreenDrawn || isPowerOn) {
      DBG_println("Drawing setup screen...");
      drawSetupScreen();
      setupScreenDrawn = true;
    } else if (isUsbWake) {
      DBG_println("USB wake - setup screen already displayed, staying idle");
    } else {
      DBG_println("Setup screen already drawn - skipping");
    }
    
    if (usbConnected) {
      DBG_println("\n>>> USB CONNECTED - Staying awake for setup <<<");
    } else {
      DBG_println("\n>>> ON BATTERY - Going to sleep <<<");
      setupScreenDrawn = false;
      go_to_deep_sleep();
    }
    
  } else {
    DBG_println("\n>>> FULLY CONFIGURED <<<");
    
    bool shouldUpdate = false;
    bool forceRedraw  = false;  // button press always forces redraw
    
    if (isUsbWake) {
      DBG_println("USB wake - staying idle, NOT fetching data");
      shouldUpdate = false;
      
    } else if (isButtonWake) {
      DBG_println("Button wake - fetching fresh data (forced redraw)");
      shouldUpdate = true;
      forceRedraw  = true;  // BOOT button always redraws
      
    } else if (isTimerWake) {
      DBG_println("Timer wake - fetching scheduled update");
      shouldUpdate = true;
      forceRedraw  = false;  // skip draw if nothing new
      
    } else if (isPowerOn || bootCount == 1) {
      DBG_println("First boot or power-on - fetching initial data");
      shouldUpdate = true;
      forceRedraw  = true;   // always draw on first boot
      
    } else {
      DBG_println("Unknown wake - fetching data to be safe");
      shouldUpdate = true;
      forceRedraw  = false;
    }
    
    if (shouldUpdate) {
      if (wasManualWake) {
        DBG_println("Manual wake - stabilizing...");
        for (int i = 3; i > 0; i--) {
          USBSerial.print(i); DBG_println("...");
          delay(1000);
          feedWatchdog();
        }
        blinkLED(3);
      }
      
      bool forceFetch = (bootCount == 1) || wasManualWake || isTimerWake;
      updateStravaAndDisplay(forceFetch, forceRedraw);
      blinkLED(2);
    } else {
      DBG_println("Skipping update - dashboard already current");
    }
    
    usbConnected = PMU.isVbusIn();
    
    if (usbConnected) {
      DBG_println("\n>>> USB CONNECTED - Staying awake for editing <<<");
    } else {
      DBG_println("\n>>> ON BATTERY - Going to sleep <<<");
      go_to_deep_sleep();
    }
  }
  
  bootCount++;
}


// =============================================================================
// SECTION 21: ARDUINO LOOP (runs when USB connected)
// =============================================================================

void loop() {
  static unsigned long lastBatteryCheck = 0;
  static unsigned long lastUsbCheck = 0;
  static int usbDisconnectCount = 0;
  
  feedWatchdog();
  
  unsigned long currentMillis = millis();
  
  // ── Foolproof USB detection (unchanged from v4) ───────────────────────────
  if (currentMillis - lastUsbCheck >= 1000) {
    lastUsbCheck = currentMillis;
    
    PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
    delay(50);
    bool pmuSaysConnected = PMU.isVbusIn();
    bool serialWorks      = (bool)USBSerial;
    bool usbActuallyConnected = pmuSaysConnected || serialWorks;
    
    if (!usbActuallyConnected) {
      usbDisconnectCount++;
      
      DBG_print("⚠️  USB disconnect check ");
      DBG_print(usbDisconnectCount);
      DBG_print("/10 (PMU: ");
      DBG_print(pmuSaysConnected ? "ON" : "OFF");
      DBG_print(", USBSerial: ");
      DBG_print(serialWorks ? "WORKS" : "DEAD");
      DBG_println(")");
      
      if (usbDisconnectCount >= 10) {
        DBG_println("\n╔════════════════════════════════════════════╗");
        DBG_println("║  USB CONFIRMED DISCONNECTED (10 checks)   ║");
        DBG_println("║  Going to sleep...                        ║");
        DBG_println("╚════════════════════════════════════════════╝\n");
        delay(500);
        go_to_deep_sleep();
        return;
      }
      
    } else {
      if (usbDisconnectCount > 0) {
        DBG_print("[Ibis Dash] USB reconnected (was at ");
        DBG_print(usbDisconnectCount);
        DBG_println("/10) - staying awake");
        usbDisconnectCount = 0;
      }
    }
  }
  
  handleSerialCommands();
  
  if (sleepRequested) {
    sleepRequested = false;
    
    PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
    delay(50);
    bool pmuSaysConnected = PMU.isVbusIn();
    bool serialWorks      = (bool)USBSerial;
    
    if (!pmuSaysConnected && !serialWorks) {
      DBG_println("\n>>> Executing requested sleep (USB verified off) <<<");
      delay(200);
      go_to_deep_sleep();
      return;
    } else {
      DBG_println("⚠️  Sleep requested but USB still connected - STAYING AWAKE");
      DBG_print("    PMU: ");
      DBG_print(pmuSaysConnected ? "CONNECTED" : "DISCONNECTED");
      DBG_print(", USBSerial: ");
      DBG_println(serialWorks ? "WORKS" : "DEAD");
    }
  }
  
  // ── BOOT BUTTON: manual refresh (always forces redraw) ──────────────────
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    DBG_println("\n>>> BOOT BUTTON - MANUAL REFRESH <<<");
    while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); feedWatchdog(); }
    blinkLED(2);
    
    loadConfiguration();
    bool hasWifi   = (WIFI_SSID.length() > 0);
    bool hasDashboard = hasDashboardCredentials();
    
    if (hasWifi && hasDashboard) {
      DBG_println("Fetching fresh dashboard data (forced redraw)...");
      updateStravaAndDisplay(true, true);  // forceFetch=true, forceRedraw=true
      
      PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
      delay(50);
      bool pmuSaysConnected = PMU.isVbusIn();
      bool serialWorks      = (bool)USBSerial;
      
      if (!pmuSaysConnected && !serialWorks) {
        DBG_println(">>> On battery - sleeping <<<");
        go_to_deep_sleep();
        return;
      } else {
        DBG_println(">>> USB connected - staying awake <<<");
      }
    } else {
      if (!setupScreenDrawn) {
        drawSetupScreen();
        setupScreenDrawn = true;
      }
    }
  }
  
  // ── KEY BUTTON: reserved for future use ──────────────────────────────────
  if (digitalRead(USER_BUTTON_PIN) == LOW) {
    while (digitalRead(USER_BUTTON_PIN) == LOW) { delay(10); feedWatchdog(); }
    // No action assigned
  }
  
  // LED indicator - slow blink while ready for commands
  digitalWrite(ACT_LED_PIN, ((currentMillis / 1000) % 2) ? HIGH : LOW);
  
  // Battery check every 30 seconds
  if (currentMillis - lastBatteryCheck >= 30000) {
    printBatteryStatus();
    lastBatteryCheck = currentMillis;
  }
  
  delay(100);
}
