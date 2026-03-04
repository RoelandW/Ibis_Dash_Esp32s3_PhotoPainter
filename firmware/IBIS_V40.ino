// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║                    🦤 IBIS DASH - USER READY VERSION 🦤                      ║
// ║                                                                              ║
// ║  VERSION 5.1 - SMART REFRESH + CYCLING SPORTS                                ║
// ║  • NEW: No unnecessary screen redraws (only draws when data changes)         ║
// ║  • NEW: Goal-exceeded display (+% and +km above goal)                        ║
// ║  • NEW: Cycling mode auto-includes all Strava ride variants                  ║
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
// ║  4. Configure WiFi + Strava and save                                         ║
// ║  5. The board becomes a Strava dashboard!                                    ║
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
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPI.h>
#include <GxEPD2_7C.h>

// Custom fonts
#include <Fonts/fonnts_com_Maison_Neue_Bold9pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Bold18pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Bold24pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Light9pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Light15pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Light18pt7b.h>

#include <Wire.h>
#include "XPowersLib.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_task_wdt.h"
#include "esp_int_wdt.h"
#include <nvs_flash.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <vector>
#include <Preferences.h>

// USB Composite Device - Makes board identify as "Ibis Dash" to PCs
// CDC (serial) + HID (prevents aggressive USB power management)
#include "USB.h"
#include "USBHID.h"
#include "USBCDC.h"

// In USB-OTG (TinyUSB) mode with "USB CDC On Boot: Disabled",
// the core does NOT auto-create a USBSerial object.
// We declare our own USBCDC instance here. All serial communication
// in this sketch uses USBSerial so it goes through the USB COM port.
USBCDC USBSerial;

// Include the ibis logo data (place ibis_logos.h in same folder as this .ino)
#include "ibis_logos.h"


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


// =============================================================================
// SECTION 5: GLOBAL VARIABLES
// =============================================================================

XPowersAXP2101 PMU;
Preferences preferences;
String serialInputBuffer = "";

GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT> display(
  GxEPD2_730c_GDEP073E01(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ===== USB COMPOSITE DEVICE =====
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
bool usbCompositeStarted = false;

// ===== USER CONFIGURATION (loaded from NVS, set via Ibis Setup app) =====
// These start BLANK - user must configure via app
String WIFI_SSID = "";
String WIFI_PASS = "";
String CLIENT_ID = "";
String CLIENT_SECRET = "";
String REFRESH_TOKEN = "";
String USER_NAME = "";
String SPORT_TYPE = "Run";
String CUSTOM_TITLE = "";        // Custom title for header (optional)
float YEARLY_GOAL = 1000.0;
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
// SECTION 7: CONFIGURATION MANAGEMENT
// =============================================================================

void loadConfiguration() {
  USBSerial.println("=== Loading Configuration from NVS ===");
  
  preferences.begin("config", true);  // Read-only
  
  // Load WiFi credentials
  WIFI_SSID = preferences.getString("ssid", "");
  WIFI_PASS = preferences.getString("password", "");
  
  // Load Strava API credentials
  CLIENT_ID = preferences.getString("clientID", "");
  CLIENT_SECRET = preferences.getString("clientSecret", "");
  REFRESH_TOKEN = preferences.getString("refreshToken", "");
  
  // Load user settings
  USER_NAME = preferences.getString("name", "");
  SPORT_TYPE = preferences.getString("sport", "Run");
  CUSTOM_TITLE = preferences.getString("title", "");
  YEARLY_GOAL = preferences.getFloat("goal", 1000.0);
  REFRESH_HOURS = preferences.getInt("refreshHours", 12);
  TRACK_PERIOD = preferences.getInt("trackPeriod", TRACK_YEARLY);
  
  preferences.end();
  
  // Determine if board is configured (has WiFi credentials)
  isConfigured = (WIFI_SSID.length() > 0);
  
  USBSerial.println("[OK] Configuration loaded:");
  USBSerial.print("  Configured: "); USBSerial.println(isConfigured ? "YES" : "NO");
  USBSerial.print("  WiFi SSID: "); USBSerial.println(WIFI_SSID.length() > 0 ? WIFI_SSID : "(not set)");
  USBSerial.print("  User Name: "); USBSerial.println(USER_NAME.length() > 0 ? USER_NAME : "(not set)");
  USBSerial.print("  Sport Type: "); USBSerial.println(SPORT_TYPE);
  USBSerial.print("  Goal: "); USBSerial.print(YEARLY_GOAL); USBSerial.println(" km");
  USBSerial.print("  Refresh Hours: "); USBSerial.println(REFRESH_HOURS);
  USBSerial.print("  Track Period: "); 
  switch(TRACK_PERIOD) {
    case TRACK_WEEKLY: USBSerial.println("Weekly"); break;
    case TRACK_MONTHLY: USBSerial.println("Monthly"); break;
    default: USBSerial.println("Yearly"); break;
  }
  USBSerial.println();
}

bool hasStravaCredentials() {
  return (CLIENT_ID.length() > 0 && CLIENT_SECRET.length() > 0 && REFRESH_TOKEN.length() > 0);
}

// ── Change #4 helper: returns true if a given Strava type counts as "cycling" ─
bool isCyclingType(const String& actType) {
  for (int i = 0; i < CYCLING_TYPES_COUNT; i++) {
    if (actType == CYCLING_TYPES[i]) return true;
  }
  return false;
}

// Returns true if actType matches the currently configured SPORT_TYPE filter.
// When SPORT_TYPE == "Ride", all cycling variants are accepted.
bool activityMatchesSport(const String& actType) {
  if (SPORT_TYPE == SPORT_RIDE) {
    return isCyclingType(actType);
  }
  return (actType == SPORT_TYPE);
}


// =============================================================================
// SECTION 8: WATCHDOG & STABILITY FUNCTIONS
// =============================================================================

void initWatchdog() {
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);
  esp_task_wdt_add(NULL);
  USBSerial.print("[OK] Watchdog enabled: ");
  USBSerial.print(WATCHDOG_TIMEOUT_SECONDS);
  USBSerial.println("s timeout");
}

void feedWatchdog() {
  esp_task_wdt_reset();
}

bool checkCrashLoop() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    rapidBootCount++;
    if (rapidBootCount >= 5) {
      USBSerial.println("WARNING: CRASH LOOP DETECTED!");
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
    USBSerial.println("BOOT button held - checking for factory reset...");
    
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
        USBSerial.println("FACTORY RESET TRIGGERED!");
        
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
  USBSerial.print("PMU IRQ pin (GPIO 21) state: ");
  USBSerial.println(irqState ? "HIGH" : "LOW");
  
  // Configure PMU to generate IRQ on USB events
  if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    PMU.clearIrqStatus();
    
    // Enable USB insertion/removal interrupts
    PMU.enableIRQ(XPOWERS_AXP2101_VBUS_INSERT_IRQ);
    PMU.enableIRQ(XPOWERS_AXP2101_VBUS_REMOVE_IRQ);
    
    USBSerial.println("  ✓ PMU USB interrupts enabled");
  } else {
    USBSerial.println("  ⚠ PMU interrupt setup failed");
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
  
  USBSerial.println("Preparing PMU for deep sleep...");
  
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
      USBSerial.print(" [⚠️  PMU UNRELIABLE - ignoring false low reading] ");
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
  
  USBSerial.print("Battery: ");
  USBSerial.print(batteryPercentage);
  USBSerial.print("% (");
  USBSerial.print(batteryVoltage, 2);
  USBSerial.print("V)");
  
  if (isUsbConnected) {
    if (isCharging) {
      USBSerial.print(" [CHARGING]");
    } else if (batteryPercentage >= 95) {
      USBSerial.print(" [CHARGED]");
    } else {
      USBSerial.print(" [USB]");
    }
  }
  USBSerial.println();
}


// =============================================================================
// SECTION 10: USB COMPOSITE DEVICE INITIALIZATION
// =============================================================================

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

void initUSBComposite() {
  USB.productName("Ibis Dash");
  USB.manufacturerName("Ibis");
  USB.VID(0x303A);
  USB.PID(0x8001);
  
  ibishid.addDevice(&ibisDummyDevice, sizeof(ibisHidReportDescriptor));
  ibishid.begin();
  
  USBSerial.begin();
  USBSerial.setTxTimeoutMs(0);
  
  USB.begin();
  
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
      USBSerial.println("Display timeout!");
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


// =============================================================================
// SECTION 12: DRAW IBIS LOGO FUNCTION
// =============================================================================

void drawIbisLogo(int x, int y, const uint8_t* logoData, uint16_t w, uint16_t h) {
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col += 2) {
      uint16_t byteIndex = (row * w + col) / 2;
      uint8_t packedByte = pgm_read_byte(&logoData[byteIndex]);
      
      uint8_t pixel1 = (packedByte >> 4) & 0x0F;
      uint8_t pixel2 = packedByte & 0x0F;
      
      uint16_t color1, color2;
      
      switch(pixel1) {
        case 0: color1 = GxEPD_BLACK; break;
        case 4: color1 = GxEPD_RED; break;
        default: color1 = GxEPD_WHITE; break;
      }
      switch(pixel2) {
        case 0: color2 = GxEPD_BLACK; break;
        case 4: color2 = GxEPD_RED; break;
        default: color2 = GxEPD_WHITE; break;
      }
      
      if (color1 != GxEPD_WHITE) {
        display.drawPixel(x + col, y + row, color1);
      }
      if (col + 1 < w && color2 != GxEPD_WHITE) {
        display.drawPixel(x + col + 1, y + row, color2);
      }
    }
  }
}


// =============================================================================
// SECTION 13: SETUP SCREEN (shown when not configured)
// =============================================================================

void drawSetupScreen() {
  USBSerial.println("=== Drawing Setup Screen ===");
  feedWatchdog();
  
  USBSerial.println("Stabilizing power...");
  delay(2000);
  feedWatchdog();
  
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  delay(500);
  feedWatchdog();
  
  esp_task_wdt_delete(NULL);
  
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
    
    int logoY = 100;
    int logoSpacing = 60;
    int logo1X = (W / 2) - IBIS_LOGO_CENTERED_WIDTH - (logoSpacing / 2);
    int logo2X = (W / 2) + (logoSpacing / 2);
    
    drawIbisLogo(logo1X, logoY, ibis_logo_centered, IBIS_LOGO_CENTERED_WIDTH, IBIS_LOGO_CENTERED_HEIGHT);
    drawIbisLogo(logo2X, logoY, ibis_logo_cycle, IBIS_LOGO_CYCLE_WIDTH, IBIS_LOGO_CYCLE_HEIGHT);
    
    int textStartY = logoY + IBIS_LOGO_CENTERED_HEIGHT + 70;
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
  
  esp_task_wdt_add(NULL);
  feedWatchdog();
  
  USBSerial.println("Setup screen complete!");
}


// =============================================================================
// SECTION 14: WIFI & TIME FUNCTIONS
// =============================================================================

void connectWiFi() {
  if (WIFI_SSID.length() == 0) {
    USBSerial.println("No WiFi credentials - skipping connection");
    return;
  }
  
  USBSerial.print("Connecting to WiFi: ");
  USBSerial.println(WIFI_SSID);
  
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
    USBSerial.print(".");
    attempts++;
    feedWatchdog();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    USBSerial.println(" Connected!");
    USBSerial.print("IP: ");
    USBSerial.println(WiFi.localIP());
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
  } else {
    USBSerial.println(" FAILED!");
  }
}

bool testWiFiConnection() {
  if (WIFI_SSID.length() == 0) return false;
  
  USBSerial.println("Testing WiFi connection...");
  
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    USBSerial.print(".");
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
    USBSerial.println("NTP sync skipped (synced within 12h)");
    return;
  }
  
  USBSerial.println("Syncing time with NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    attempts++;
    feedWatchdog();
  }
  
  if (getLocalTime(&timeinfo)) {
    USBSerial.print("Time synced: ");
    USBSerial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
    time(&lastNtpSyncEpoch);
  }
}


// =============================================================================
// SECTION 15: STRAVA API FUNCTIONS
// =============================================================================

bool refreshAccessToken() {
  if (!hasStravaCredentials()) {
    USBSerial.println("No Strava credentials - skipping token refresh");
    return false;
  }
  
  time_t now;
  time(&now);
  if (cachedAccessToken[0] != '\0' && tokenExpiresAt > 0 && now < (tokenExpiresAt - 300)) {
    USBSerial.println("Using cached access token");
    accessToken = String(cachedAccessToken);
    return true;
  }
  
  USBSerial.println("Refreshing Strava access token...");
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
    USBSerial.print("[FAIL] Token refresh: ");
    USBSerial.println(code);
    http.end();
    return false;
  }
}

void getTrackingPeriodTimestamps(long &afterTS, long &beforeTS) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    USBSerial.println("Failed to get time!");
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
    USBSerial.println("No Strava credentials - using placeholder data");
    kmDone = 0;
    timeHours = 0;
    activitiesCount = 0;
    lastTitle = "No Strava";
    lastLine = "Configure in Ibis Setup app";
    lastPolyline = "";
    return;
  }
  
  USBSerial.println("=== Fetching Strava Data ===");
  feedWatchdog();
  
  kmDone = 0;
  timeHours = 0;
  activitiesCount = 0;
  lastTitle = "";
  lastLine = "";
  lastPolyline = "";
  lastDistKm = 0;
  lastMovingSecs = 0;
  lastAvgSpeedKph = 0.0;
  lastDateStr = "";
  
  if (accessToken == "") {
    USBSerial.println("No access token");
    return;
  }
  
  long afterTS, beforeTS;
  getTrackingPeriodTimestamps(afterTS, beforeTS);
  
  USBSerial.print("Tracking period: ");
  switch(TRACK_PERIOD) {
    case TRACK_WEEKLY: USBSerial.println("Weekly"); break;
    case TRACK_MONTHLY: USBSerial.println("Monthly"); break;
    default: USBSerial.println("Yearly"); break;
  }
  USBSerial.print("Filtering for sport type: ");
  USBSerial.print(SPORT_TYPE);
  if (SPORT_TYPE == SPORT_RIDE) USBSerial.print(" (+ all cycling variants)");
  USBSerial.println();
  
  bool gotLast = false;
  int page = 1;
  int totalActivities = 0;
  
  while (true) {
    feedWatchdog();
    
    HTTPClient http;
    String req = "https://www.strava.com/api/v3/athlete/activities?";
    req += "after=" + String(afterTS) + "&before=" + String(beforeTS);
    req += "&per_page=30&page=" + String(page);
    
    http.begin(req);
    http.addHeader("Authorization", "Bearer " + accessToken);
    
    int code = http.GET();
    
    if (code != 200) {
      USBSerial.print("API error: ");
      USBSerial.println(code);
      http.end();
      break;
    }
    
    String payload = http.getString();

    // ── Change #4: also request average_speed field ───────────────────────────
    JsonDocument filter;
    filter[0]["type"] = true;
    filter[0]["name"] = true;
    filter[0]["start_date_local"] = true;
    filter[0]["distance"] = true;
    filter[0]["moving_time"] = true;
    filter[0]["average_speed"] = true;               // m/s from Strava
    filter[0]["map"]["summary_polyline"] = true;

    JsonDocument doc;

    if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
      USBSerial.println("JSON parse error");
      http.end();
      break;
    }
    
    if (!doc.is<JsonArray>()) {
      USBSerial.println("Response is not an array");
      http.end();
      break;
    }
    
    JsonArray arr = doc.as<JsonArray>();
    
    if (arr.size() == 0) {
      USBSerial.println("No more activities");
      http.end();
      break;
    }
    
    USBSerial.print("Page ");
    USBSerial.print(page);
    USBSerial.print(": ");
    USBSerial.print(arr.size());
    USBSerial.println(" activities");
    
    for (JsonObject act : arr) {
      totalActivities++;
      
      String actType = act["type"].as<String>();
      
      if (totalActivities <= 3) {
        USBSerial.print("  Activity type: '");
        USBSerial.print(actType);
        USBSerial.print("' vs filter: '");
        USBSerial.print(SPORT_TYPE);
        USBSerial.println("'");
      }
      
      // ── Change #4: use unified match helper ───────────────────────────────
      if (activityMatchesSport(actType)) {
        if (!gotLast) {
          lastTitle = act["name"].as<String>();
          if (lastTitle.length() == 0) lastTitle = "Untitled " + SPORT_TYPE;
          
          String dt = act["start_date_local"].as<String>();
          
          if (dt.length() >= 10) {
            int month = dt.substring(5, 7).toInt();
            int day   = dt.substring(8, 10).toInt();
            const char* monthNames[] = {"January", "February", "March", "April", 
                                        "May", "June", "July", "August", 
                                        "September", "October", "November", "December"};
            if (month >= 1 && month <= 12) {
              lastDateStr = String(day) + " " + monthNames[month - 1];
            } else {
              lastDateStr = dt.substring(0, 10);
            }
          } else {
            lastDateStr = dt;
          }
          
          float dkm = act["distance"].as<float>() / 1000.0;
          int movingSecs = act["moving_time"].as<int>();
          float hrs = movingSecs / 3600.0;

          // average_speed comes in m/s — convert to km/h
          float avgSpeedMs  = act["average_speed"].as<float>();
          float avgSpeedKph = avgSpeedMs * 3.6f;
          
          if (dkm > 0 && dkm < 1000 && hrs > 0 && hrs < 100) {
            lastDistKm     = dkm;
            lastMovingSecs = movingSecs;
            lastAvgSpeedKph = avgSpeedKph;
            lastLine = String(dkm, 1) + " km  " + String(hrs, 1) + "h  " + lastDateStr;
            gotLast = true;
            
            if (!act["map"]["summary_polyline"].isNull()) {
              lastPolyline = act["map"]["summary_polyline"].as<String>();
            }
          }
        }
        
        float distance = act["distance"].as<float>() / 1000.0;
        float time     = act["moving_time"].as<float>() / 3600.0;
        
        if (distance > 0 && distance < 1000 && time > 0 && time < 100) {
          kmDone += distance;
          timeHours += time;
          activitiesCount++;
        }
      }
    }
    
    http.end();
    page++;
    
    if (page > 10) {
      USBSerial.println("Reached max pages (10)");
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
  
  USBSerial.println("=== Strava Fetch Complete ===");
  USBSerial.print("Total activities fetched: "); USBSerial.println(totalActivities);
  USBSerial.print("Matching activities ("); USBSerial.print(SPORT_TYPE); USBSerial.print("): ");
  USBSerial.println(activitiesCount);
  USBSerial.print("Distance: "); USBSerial.print(kmDone, 1); USBSerial.println(" km");
  USBSerial.print("Time: "); USBSerial.print(timeHours, 1); USBSerial.println(" hours");
  
  if (totalActivities > 0 && activitiesCount == 0) {
    USBSerial.println("WARNING: Got activities but none matched sport type!");
    USBSerial.print("Check that SPORT_TYPE='");
    USBSerial.print(SPORT_TYPE);
    USBSerial.println("' matches Strava activity types exactly");
  }
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
  // Activity count changed
  if (lastDrawnActivitiesCount != activitiesCount) {
    USBSerial.print("[smart-redraw] Activity count changed (");
    USBSerial.print(lastDrawnActivitiesCount);
    USBSerial.print(" → ");
    USBSerial.print(activitiesCount);
    USBSerial.println(") → must draw");
    return true;
  }
  // Distance changed by ≥ 0.05 km (floating-point guard)
  if (fabsf(lastDrawnKmDone - kmDone) >= 0.05f) {
    USBSerial.print("[smart-redraw] Distance changed (");
    USBSerial.print(lastDrawnKmDone, 1);
    USBSerial.print(" → ");
    USBSerial.print(kmDone, 1);
    USBSerial.println(" km) → must draw");
    return true;
  }
  USBSerial.println("[smart-redraw] Data unchanged → skipping display refresh");
  return false;
}

// Call this AFTER a successful draw to record the snapshot
void recordDrawnSnapshot() {
  lastDrawnKmDone          = kmDone;
  lastDrawnActivitiesCount = activitiesCount;
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
      // "Strava Stats Week 21"
      title = name + "Strava Stats Week " + String(dashWeek);
      break;
    }
    case TRACK_MONTHLY: {
      // "Strava Stats February"
      const char* months[] = {"January", "February", "March", "April",
                               "May", "June", "July", "August",
                               "September", "October", "November", "December"};
      int mi = dashMonth - 1;
      if (mi < 0) mi = 0;
      if (mi > 11) mi = 11;
      title = name + "Strava Stats " + months[mi];
      break;
    }
    default:
      // Yearly — keep original behaviour
      title = name + "Strava Stats " + String(dashYear);
      break;
  }
  
  return title;
}

String getActivityLabel() {
  if (SPORT_TYPE == "Run")  return "RUNS";
  if (SPORT_TYPE == "Ride") return "RIDES";
  if (SPORT_TYPE == "Swim") return "SWIMS";
  if (SPORT_TYPE == "Hike") return "HIKES";
  if (SPORT_TYPE == "Walk") return "WALKS";
  return "ACTIVITIES";
}

// ── Change #4: label for the "latest activity" panel ─────────────────────────
String getLatestActivityLabel() {
  if (SPORT_TYPE == "Ride") return "LATEST RIDE";
  if (SPORT_TYPE == "Run")  return "LATEST RUN";
  if (SPORT_TYPE == "Swim") return "LATEST SWIM";
  if (SPORT_TYPE == "Hike") return "LATEST HIKE";
  if (SPORT_TYPE == "Walk") return "LATEST WALK";
  return "LATEST " + SPORT_TYPE;
}

void drawDashboard() {
  USBSerial.println("=== Drawing Dashboard ===");
  feedWatchdog();
  
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
      USBSerial.println("Power unstable - skipping refresh");
      return;
    }
  }
  
  USBSerial.println("Reinitializing display hardware...");
  SPI.end();
  delay(100);
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  epd_deep_init();
  display.init(115200, true, 2, false);
  display.setRotation(2);
  delay(200);
  feedWatchdog();
  
  // ── Change #2: progress capped at 1.0, detect overflow ───────────────────
  float pct = (YEARLY_GOAL > 0) ? (kmDone / YEARLY_GOAL) : 0;
  if (pct < 0) pct = 0;
  bool goalExceeded = (pct > 1.0f);
  float displayPct = (pct > 1.0f) ? 1.0f : pct;  // bar always max 100%
  
  if (lowBatteryMode) {
    C_HEADER = GxEPD_BLUE;
    C_TEXT_DIM = GxEPD_BLUE;
  } else {
    C_HEADER = GxEPD_RED;
    C_TEXT_DIM = GxEPD_RED;
  }
  
  USBSerial.println("Starting display refresh...");
  USBSerial.flush();
  delay(200);
  
  esp_task_wdt_delete(NULL);
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    feedWatchdog();
    display.fillScreen(GxEPD_WHITE);
    
    const int margin = 20;
    const int innerPad = 18;
    
    int16_t tx1, ty1;
    uint16_t tw, th;
    
    const int headerH = 70;
    const int topAnchor = 121;
    const int bottomAnchor = H - 12;
    const int labelToValue = 36;
    const int barGapAbove = 16;
    const int barH = 34;
    const int barGapBelow = 20;
    const int headerGapSmall = 30;
    const int sectionGap = 51;
    
    // ===== HEADER =====
    display.fillRect(0, 0, W, headerH, C_HEADER);
    display.setFont(&fonnts_com_Maison_Neue_Bold24pt7b);
    display.setTextColor(GxEPD_WHITE);
    
    String headerText = getHeaderTitle();
    display.getTextBounds(headerText, 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor((W - tw) / 2, 48);
    display.print(headerText);
    
    // ===== DISTANCE SECTION =====
    int distLabelY = topAnchor;
    
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.setCursor(margin + innerPad, distLabelY);
    display.print("DISTANCE");
    
    int distValueY = distLabelY + labelToValue;
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(margin + innerPad, distValueY);
    display.print(String(kmDone, 1) + " km / " + String((int)YEARLY_GOAL) + " km");
    
    // Progress bar (capped at 100%)
    int barX = margin + innerPad;
    int barY = distValueY + barGapAbove;
    int barW = (W - 2 * margin) - 2 * innerPad;
    
    display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
    display.drawRect(barX + 1, barY + 1, barW - 2, barH - 2, GxEPD_BLACK);
    
    int fillW = (int)((barW - 4) * displayPct);
    if (fillW > 0) {
      display.fillRect(barX + 2, barY + 2, fillW, barH - 4, GxEPD_GREEN);
    }
    
    // ── Change #2: below-bar text ─────────────────────────────────────────────
    int pctTextY = barY + barH + barGapBelow;
    display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(barX, pctTextY);
    
    if (goalExceeded) {
      // Show how much above goal
      float kmAbove  = kmDone - YEARLY_GOAL;
      float pctAbove = (pct - 1.0f) * 100.0f;
      char aboveBuf[64];
      snprintf(aboveBuf, sizeof(aboveBuf),
               "+%.1f%% above goal  +%.1f km above goal",
               pctAbove, kmAbove);
      display.print(aboveBuf);
    } else {
      // Normal: remaining to go
      float kmToGo = YEARLY_GOAL - kmDone;
      if (kmToGo < 0) kmToGo = 0;
      char remBuf[64];
      snprintf(remBuf, sizeof(remBuf),
               "%.1f%% of goal  %.1f km to go",
               displayPct * 100.0f, kmToGo);
      display.print(remBuf);
    }
    
    // ===== RUNS / TIME / LAST ROUTE =====
    int runsLabelY = pctTextY + sectionGap;
    int runsValueY = runsLabelY + labelToValue;
    
    int colGap = 14;
    int colW = (W - 2 * margin - 2 * colGap) / 3;
    int x1 = margin;
    int x2 = margin + colW + colGap;
    int x3 = margin + 2 * (colW + colGap);
    
    // Activities panel
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.setCursor(x1 + innerPad, runsLabelY);
    display.print(getActivityLabel());
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x1 + innerPad, runsValueY);
    display.print(String(activitiesCount));
    
    // Time panel - centered
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.getTextBounds("TIME", 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor(x2 + (colW - tw) / 2, runsLabelY);
    display.print("TIME");
    
    int hours = (int)timeHours;
    int minutes = (int)((timeHours - hours) * 60);
    String timeStr = String(hours) + "h " + String(minutes) + "m";
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.getTextBounds(timeStr, 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor(x2 + (colW - tw) / 2, runsValueY);
    display.print(timeStr);
    
    // Last Route panel
    int routeTopY = runsLabelY - 32;
    int routeH = H - routeTopY - 30;
    display.fillRect(x3, routeTopY, colW, routeH, GxEPD_WHITE);
    
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.setCursor(x3 + innerPad, runsLabelY);
    display.print("LAST ROUTE");
    
    int titleAreaHeight = 45;
    int polylineAreaX = x3;
    int polylineAreaY = routeTopY + titleAreaHeight;
    int polylineAreaW = colW;
    int polylineAreaH = routeH - titleAreaHeight;
    
    if (lastPolyline.length() > 0) {
      std::vector<Point> pts = decodePolyline(lastPolyline);
      
      if (pts.size() >= 2) {
        float minLat = pts[0].lat, maxLat = pts[0].lat;
        float minLon = pts[0].lon, maxLon = pts[0].lon;
        for (auto &p : pts) {
          if (p.lat < minLat) minLat = p.lat;
          if (p.lat > maxLat) maxLat = p.lat;
          if (p.lon < minLon) minLon = p.lon;
          if (p.lon > maxLon) maxLon = p.lon;
        }
        
        float latRange = maxLat - minLat;
        if (latRange < 1e-6) latRange = 1e-6;
        float lonRange = maxLon - minLon;
        if (lonRange < 1e-6) lonRange = 1e-6;
        
        int drawMargin = 2;
        int drawWidth  = polylineAreaW - (2 * drawMargin);
        int drawHeight = polylineAreaH - (2 * drawMargin);
        
        float routeAspect = lonRange / latRange;
        float zoneAspect  = (float)drawWidth / (float)drawHeight;
        
        int actualDrawWidth, actualDrawHeight;
        int offsetX = 0, offsetY = 0;
        
        if (routeAspect > zoneAspect) {
          actualDrawWidth  = drawWidth;
          actualDrawHeight = (int)(drawWidth / routeAspect);
          offsetY = (drawHeight - actualDrawHeight) / 2;
        } else {
          actualDrawHeight = drawHeight;
          actualDrawWidth  = (int)(drawHeight * routeAspect);
          offsetX = (drawWidth - actualDrawWidth) / 2;
        }
        
        int drawX = polylineAreaX + drawMargin;
        int drawY = polylineAreaY + drawMargin;
        
        for (size_t i = 1; i < pts.size(); i++) {
          float normLon0 = (pts[i - 1].lon - minLon) / lonRange;
          float normLat0 = (pts[i - 1].lat - minLat) / latRange;
          float normLon1 = (pts[i].lon - minLon) / lonRange;
          float normLat1 = (pts[i].lat - minLat) / latRange;
          
          int px0 = drawX + offsetX + (int)(normLon0 * actualDrawWidth);
          int py0 = drawY + offsetY + (int)((1.0 - normLat0) * actualDrawHeight);
          int px1 = drawX + offsetX + (int)(normLon1 * actualDrawWidth);
          int py1 = drawY + offsetY + (int)((1.0 - normLat1) * actualDrawHeight);
          
          display.drawLine(px0, py0, px1, py1, GxEPD_BLACK);
          display.drawLine(px0 + 1, py0, px1 + 1, py1, GxEPD_BLACK);
        }
      }
    }
    
    // ===== LATEST ACTIVITY PANEL =====
    int latestLabelY = runsValueY + sectionGap;
    int latestW = colW * 2 + colGap;
    
    // Row 1: sport-specific label (e.g. "LATEST RIDE")
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.setCursor(margin + innerPad, latestLabelY);
    display.print(getLatestActivityLabel());
    
    // Row 2: date
    int dateNameY = latestLabelY + labelToValue;
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(margin + innerPad, dateNameY);
    if (lastDateStr.length() > 0) {
      display.print(lastDateStr);
    } else if (lastTitle.length() > 0) {
      display.print(lastTitle);
    }
    
    // Row 3: column headers
    int colHeaderY = dateNameY + headerGapSmall;
    int col1X = margin + innerPad;
    int col2X = margin + innerPad + (latestW - 2 * innerPad) / 3;
    int col3X = margin + innerPad + 2 * (latestW - 2 * innerPad) / 3;
    
    display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(col1X, colHeaderY);
    display.print("Distance");
    display.setCursor(col2X, colHeaderY);
    // ── Change #4: cycling shows "Avg Speed" instead of "Pace" ───────────────
    if (SPORT_TYPE == SPORT_RIDE) {
      display.print("Avg Speed");
    } else {
      display.print("Pace");
    }
    display.setCursor(col3X, colHeaderY);
    display.print("Time");
    
    // Row 4: values
    int valY = colHeaderY + headerGapSmall;
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    if (lastDistKm > 0) {
      display.setCursor(col1X, valY);
      display.print(String(lastDistKm, 2) + " km");
      
      // ── Change #4: speed vs pace logic ────────────────────────────────────
      display.setCursor(col2X, valY);
      if (SPORT_TYPE == SPORT_RIDE) {
        // Average speed in km/h
        if (lastAvgSpeedKph > 0.1f) {
          char speedBuf[16];
          snprintf(speedBuf, sizeof(speedBuf), "%.1f km/h", lastAvgSpeedKph);
          display.print(speedBuf);
        } else {
          display.print("--");
        }
      } else {
        // Pace in min:sec /km (all non-cycling sports)
        if (lastDistKm > 0.01) {
          int totalPaceSecs = (int)(lastMovingSecs / lastDistKm);
          int paceMin = totalPaceSecs / 60;
          int paceSec = totalPaceSecs % 60;
          char paceBuf[12];
          snprintf(paceBuf, sizeof(paceBuf), "%d:%02d /km", paceMin, paceSec);
          display.print(paceBuf);
        } else {
          display.print("--");
        }
      }
      
      display.setCursor(col3X, valY);
      int totalMin = lastMovingSecs / 60;
      int timeSec  = lastMovingSecs % 60;
      if (totalMin >= 60) {
        int timeH = totalMin / 60;
        int timeM = totalMin % 60;
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%dh %02dm", timeH, timeM);
        display.print(timeBuf);
      } else {
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%dm %02ds", totalMin, timeSec);
        display.print(timeBuf);
      }
    } else {
      display.setCursor(col1X, valY); display.print("--");
      display.setCursor(col2X, valY); display.print("--");
      display.setCursor(col3X, valY); display.print("--");
    }
    
    // ===== STATUS INDICATOR =====
    char displayStr[40];
    
    if (lowBatteryMode) {
      snprintf(displayStr, sizeof(displayStr), "Battery: %.0f%%", batteryPercentage);
      display.setTextColor(GxEPD_RED);
      display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
    } else if (isUsbConnected) {
      if (batteryPercentage >= 100) {
        snprintf(displayStr, sizeof(displayStr), "I-MUUT! 100%%");
        display.setTextColor(GxEPD_GREEN);
        display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
      } else if (isCharging) {
        snprintf(displayStr, sizeof(displayStr), "Charging: %.0f%%", batteryPercentage);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&fonnts_com_Maison_Neue_Light9pt7b);
      } else {
        snprintf(displayStr, sizeof(displayStr), "Battery: %.0f%%", batteryPercentage);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&fonnts_com_Maison_Neue_Light9pt7b);
      }
    } else {
      snprintf(displayStr, sizeof(displayStr), "Updated: %s", lastUpdateTime.c_str());
      display.setTextColor(GxEPD_BLACK);
      display.setFont(&fonnts_com_Maison_Neue_Light9pt7b);
    }
    
    display.getTextBounds(displayStr, 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor(W - tw - margin, H - 12);
    display.print(displayStr);
    
  } while (display.nextPage());
  
  epd_wait_busy();
  esp_task_wdt_add(NULL);
  feedWatchdog();
  delay(1000);
  yield();
  USBSerial.flush();
  delay(500);
  
  // ── Record snapshot so we can detect future changes ─────────────────────
  recordDrawnSnapshot();
  
  USBSerial.println("Dashboard update complete!");
}


// =============================================================================
// SECTION 17: HIGH-LEVEL UPDATE FUNCTIONS
// =============================================================================

bool fetchStravaDataWithValidation() {
  USBSerial.println("\n=== FETCHING & VALIDATING STRAVA DATA ===");
  
  USBSerial.println("Step 1: Connecting WiFi...");
  connectWiFi();
  
  if (WiFi.status() != WL_CONNECTED) {
    USBSerial.println("WiFi connection failed!");
    return false;
  }
  USBSerial.println("[OK] WiFi connected");
  delay(1000);
  feedWatchdog();
  
  USBSerial.println("Step 2: Syncing time...");
  initTime();
  delay(500);
  feedWatchdog();
  
  USBSerial.println("Step 3: Refreshing access token...");
  if (!refreshAccessToken()) {
    USBSerial.println("[FAIL] Token refresh failed!");
    disconnectWiFi();
    return false;
  }
  USBSerial.println("[OK] Access token refreshed");
  delay(500);
  feedWatchdog();
  
  USBSerial.println("Step 4: Fetching Strava activities...");
  fetchStravaData();
  delay(500);
  feedWatchdog();
  
  disconnectWiFi();
  
  USBSerial.println("Step 5: Validating results...");
  USBSerial.print("  Activities: "); USBSerial.println(activitiesCount);
  USBSerial.print("  Distance: "); USBSerial.print(kmDone, 1); USBSerial.println(" km");
  USBSerial.print("  Time: "); USBSerial.print(timeHours, 1); USBSerial.println(" hours");
  
  bool dataValid = (kmDone >= 0 && kmDone < 100000 && 
                    timeHours >= 0 && timeHours < 50000 &&
                    activitiesCount >= 0 && activitiesCount < 10000);
  
  if (dataValid) USBSerial.println("[OK] Data validation passed!");
  else           USBSerial.println("[FAIL] Data validation failed - values out of range!");
  
  return dataValid;
}

// ── Change #1: forceRedraw bypasses the change-detection check ───────────────
void updateStravaAndDisplay(bool forceFetch, bool forceRedraw) {
  USBSerial.println("\n========== FULL UPDATE ==========");
  feedWatchdog();
  
  bool dataFetched = false;
  
  if (forceFetch || shouldFetchStrava()) {
    dataFetched = fetchStravaDataWithValidation();
    
    if (!dataFetched) {
      USBSerial.println("WARNING: Could not fetch Strava data!");
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
    }
    disconnectWiFi();
  }
  
  printBatteryStatus();
  
  // ── Change #1: skip draw if nothing changed (unless forced) ──────────────
  if (!forceRedraw && !dataHasChangedSinceLastDraw()) {
    USBSerial.println(">>> No new data - skipping screen redraw <<<");
    USBSerial.println("========== UPDATE COMPLETE (no draw) ==========\n");
    return;
  }
  
  USBSerial.println("Drawing dashboard...");
  delay(500);
  feedWatchdog();
  
  drawDashboard();
  resetCrashCounter();
  
  USBSerial.println("========== UPDATE COMPLETE ==========\n");
}

// Convenience overload — existing callers that pass only 2 args still work
void updateStravaAndDisplay(bool forceFetch) {
  updateStravaAndDisplay(forceFetch, false);
}

void updateDisplayOnly() {
  USBSerial.println("\n=== DISPLAY REFRESH (cached data) ===");
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
      if (serialInputBuffer.length() < 2048) {
        serialInputBuffer += c;
      }
    }
  }
}

void processSerialCommand(String command) {
  command.trim();
  
  USBSerial.print("CMD[");
  USBSerial.print(command.length());
  USBSerial.print("]: ");
  if (command.length() > 50) {
    USBSerial.print(command.substring(0, 50));
    USBSerial.println("...");
  } else {
    USBSerial.println(command);
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
  else if (command == "FETCH_STRAVA") {
    USBSerial.println("\n=== FINISH SETUP ===");
    loadConfiguration();
    
    if (WIFI_SSID.length() == 0) {
      USBSerial.println("NO_WIFI_CREDENTIALS");
    } else if (!hasStravaCredentials()) {
      USBSerial.println("NO_STRAVA_CREDENTIALS");
    } else {
      USBSerial.println("Connecting to WiFi...");
      connectWiFi();
      
      if (WiFi.status() != WL_CONNECTED) {
        USBSerial.println("WIFI_CONNECT_FAILED");
      } else {
        USBSerial.println("Syncing time...");
        initTime();
        feedWatchdog();
        
        USBSerial.println("Refreshing token...");
        if (!refreshAccessToken()) {
          USBSerial.println("TOKEN_REFRESH_FAILED");
          disconnectWiFi();
        } else {
          USBSerial.println("Fetching Strava data...");
          feedWatchdog();
          fetchStravaData();
          disconnectWiFi();
          
          USBSerial.println("STRAVA_OK");
          USBSerial.print("Activities: "); USBSerial.println(activitiesCount);
          USBSerial.print("Distance: "); USBSerial.print(kmDone); USBSerial.println(" km");
          USBSerial.print("Time: "); USBSerial.print(timeHours); USBSerial.println(" hours");
          
          printBatteryStatus();
          USBSerial.println("Drawing dashboard...");
          drawDashboard();  // Always draws after FETCH_STRAVA command
          USBSerial.println("DASHBOARD_DRAWN");
          
          if (!PMU.isVbusIn()) {
            USBSerial.println(">>> On battery - will sleep <<<");
            sleepRequested = true;
          }
        }
      }
    }
  }
  else if (command == "SHOW_SETUP_SCREEN") {
    USBSerial.println("Drawing setup screen...");
    drawSetupScreen();
    USBSerial.println("SETUP_SCREEN_DRAWN");
  }
  else if (command == "GO_SLEEP") {
    USBSerial.println("Sleep requested");
    USBSerial.println("OK");
    delay(100);
    sleepRequested = true;
  }
  else if (command == "RESTART") {
    USBSerial.println("OK");
    USBSerial.println("Restarting...");
    delay(500);
    ESP.restart();
  }
  else if (command == "PING") {
    USBSerial.println("PONG");
    USBSerial.println("IBIS_DASH_V51");
  }
  else if (command.length() > 0) {
    USBSerial.print("Unknown command: ");
    USBSerial.println(command);
    USBSerial.println("ERROR");
  }
}

void sendCurrentConfig() {
  USBSerial.println("Sending configuration...");
  
  preferences.begin("config", true);
  
  DynamicJsonDocument doc(2048);
  
  doc["ssid"]          = preferences.getString("ssid", "");
  doc["password"]      = preferences.getString("password", "");
  doc["name"]          = preferences.getString("name", "");
  doc["sport"]         = preferences.getString("sport", "Run");
  doc["goal"]          = preferences.getFloat("goal", 1000.0);
  doc["clientID"]      = preferences.getString("clientID", "");
  doc["clientSecret"]  = preferences.getString("clientSecret", "");
  doc["refreshToken"]  = preferences.getString("refreshToken", "");
  doc["refreshHours"]  = preferences.getInt("refreshHours", 12);
  doc["trackPeriod"]   = preferences.getInt("trackPeriod", TRACK_YEARLY);
  doc["title"]         = preferences.getString("title", "");
  doc["configured"]    = (preferences.getString("ssid", "").length() > 0);
  doc["hasStrava"]     = (preferences.getString("clientID", "").length() > 0);
  doc["firmwareVersion"] = "5.1";
  doc["usbIdentity"]   = "Ibis Dash";
  
  preferences.end();
  
  String output;
  serializeJson(doc, output);
  USBSerial.println(output);
  USBSerial.println("OK");
}

void saveConfigFromSerial(String jsonStr) {
  USBSerial.println("Parsing configuration...");
  USBSerial.print("JSON length: ");
  USBSerial.println(jsonStr.length());
  
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonStr);
  
  if (error) {
    USBSerial.print("JSON error: ");
    USBSerial.println(error.c_str());
    USBSerial.println("ERROR");
    return;
  }
  
  USBSerial.println("JSON parsed OK, saving to NVS...");
  
  preferences.begin("config", false);
  
  if (doc.containsKey("ssid"))          { preferences.putString("ssid",          doc["ssid"].as<String>());          USBSerial.println("  - ssid saved"); }
  if (doc.containsKey("password"))      { preferences.putString("password",      doc["password"].as<String>());      USBSerial.println("  - password saved"); }
  if (doc.containsKey("name"))          { preferences.putString("name",          doc["name"].as<String>()); }
  if (doc.containsKey("sport"))         { preferences.putString("sport",         doc["sport"].as<String>()); }
  if (doc.containsKey("goal"))          { preferences.putFloat ("goal",          doc["goal"].as<float>()); }
  if (doc.containsKey("clientID"))      { preferences.putString("clientID",      doc["clientID"].as<String>());      USBSerial.println("  - clientID saved"); }
  if (doc.containsKey("clientSecret"))  { preferences.putString("clientSecret",  doc["clientSecret"].as<String>());  USBSerial.println("  - clientSecret saved"); }
  if (doc.containsKey("refreshToken"))  { preferences.putString("refreshToken",  doc["refreshToken"].as<String>());  USBSerial.println("  - refreshToken saved"); }
  if (doc.containsKey("refreshHours"))  { preferences.putInt   ("refreshHours",  doc["refreshHours"].as<int>()); }
  if (doc.containsKey("trackPeriod"))   { preferences.putInt   ("trackPeriod",   doc["trackPeriod"].as<int>()); }
  if (doc.containsKey("title"))         { preferences.putString("title",         doc["title"].as<String>()); }
  
  preferences.end();
  
  loadConfiguration();
  
  if (doc.containsKey("clientID") || doc.containsKey("clientSecret") || doc.containsKey("refreshToken")) {
    cachedAccessToken[0] = '\0';
    tokenExpiresAt = 0;
    USBSerial.println("  - Token cache cleared");
  }
  
  // Changing config may change what data is shown — invalidate snapshot so
  // the next fetch always redraws
  lastDrawnKmDone          = -1.0;
  lastDrawnActivitiesCount = -1;
  
  USBSerial.println("Configuration saved!");
  USBSerial.println("SUCCESS");
}

void wipeConfig() {
  USBSerial.println("Wiping all configuration...");
  
  preferences.begin("config", false);
  preferences.clear();
  preferences.end();
  
  WIFI_SSID = "";
  WIFI_PASS = "";
  CLIENT_ID = "";
  CLIENT_SECRET = "";
  REFRESH_TOKEN = "";
  USER_NAME = "";
  CUSTOM_TITLE = "";
  isConfigured = false;
  
  kmDone = 0;
  timeHours = 0;
  activitiesCount = 0;
  lastStravaFetchEpoch = 0;
  lastNtpSyncEpoch = 0;
  
  lastTitle = "";
  lastLine = "";
  lastPolyline = "";
  lastUpdateTime = "";
  lastDistKm = 0;
  lastMovingSecs = 0;
  lastAvgSpeedKph = 0.0;
  lastDateStr = "";
  
  cachedAccessToken[0] = '\0';
  tokenExpiresAt = 0;
  
  // Reset smart-redraw snapshot
  lastDrawnKmDone          = -1.0;
  lastDrawnActivitiesCount = -1;
  
  USBSerial.println("Configuration wiped!");
  USBSerial.println("WIPED");
}

void wipeConfigAndShowSetup() {
  wipeConfig();
  USBSerial.println("Drawing setup screen...");
  drawSetupScreen();
  USBSerial.println("SETUP_SCREEN_DRAWN");
}


// =============================================================================
// SECTION 19: SLEEP & WAKE
// =============================================================================

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  wasManualWake = false;
  
  USBSerial.println("\n========== WAKE UP REASON ==========");
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      USBSerial.println("Wakeup: USB connected! (GPIO 21 - PMU IRQ)");
      wasManualWake = false;
      
      delay(500);
      if (PMU.isVbusIn()) {
        USBSerial.println("  ✓ USB confirmed connected");
        USBSerial.println("  → Staying idle, waiting for serial commands");
      } else {
        USBSerial.println("  ⚠ USB not detected (false wake?)");
      }
      break;
      
    case ESP_SLEEP_WAKEUP_EXT1:
      USBSerial.println("Wakeup: BOOT button pressed");
      wasManualWake = true;
      USBSerial.println("  → Will fetch fresh Strava data");
      break;
      
    case ESP_SLEEP_WAKEUP_TIMER:
      USBSerial.println("Wakeup: Timer expired (scheduled refresh)");
      USBSerial.println("  → Will fetch fresh Strava data");
      break;
      
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      USBSerial.println("Wakeup: Power-on or Reset button");
      break;
      
    default:
      USBSerial.print("Wakeup: Unknown reason (");
      USBSerial.print(wakeup_reason);
      USBSerial.println(")");
      break;
  }
  
  USBSerial.println("====================================\n");
}

void go_to_deep_sleep() {
  USBSerial.println("\n========== ENTERING DEEP SLEEP ==========");
  
  bool usbNowConnected = PMU.isVbusIn();
  USBSerial.print("USB status before sleep: ");
  USBSerial.println(usbNowConnected ? "CONNECTED" : "DISCONNECTED");
  
  if (usbNowConnected) {
    USBSerial.println("WARNING: USB is connected - should not sleep!");
    USBSerial.println("Returning to loop() instead of sleeping...");
    return;
  }
  
  preferences.begin("config", true);
  REFRESH_HOURS = preferences.getInt("refreshHours", 12);
  bool hasConfig = (preferences.getString("ssid", "").length() > 0);
  preferences.end();
  
  uint64_t sleepDuration;
  if (!hasConfig) {
    sleepDuration = SLEEP_DURATION_UNCONFIGURED_US;
    USBSerial.println("Not configured - sleeping 1 week");
  } else {
    sleepDuration = (uint64_t)REFRESH_HOURS * 60ULL * 60ULL * 1000000ULL;
    USBSerial.print("Sleeping for ");
    USBSerial.print(REFRESH_HOURS);
    USBSerial.println(" hours");
  }
  
  feedWatchdog();
  disconnectWiFi();
  
  USBSerial.println("Configuring wake sources:");
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  
  esp_sleep_enable_timer_wakeup(sleepDuration);
  USBSerial.print("  ✓ Timer: ");
  USBSerial.print(REFRESH_HOURS);
  USBSerial.println(" hours");
  
  const uint64_t button_mask = 1ULL << GPIO_NUM_0;
  esp_sleep_enable_ext1_wakeup(button_mask, ESP_EXT1_WAKEUP_ANY_LOW);
  USBSerial.println("  ✓ BOOT button (GPIO 0)");
  
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_21, 0);
  USBSerial.println("  ✓ USB connection (GPIO 21 - PMU IRQ)");
  
  pmu_prepare_for_esp32_sleep();
  
  USBSerial.println("Going to sleep NOW...");
  USBSerial.println("Wake triggers: Timer | BOOT button | USB plug-in");
  USBSerial.flush();
  
  digitalWrite(ACT_LED_PIN, LOW);
  esp_task_wdt_delete(NULL);
  
  delay(100);
  
  esp_deep_sleep_start();
}


// =============================================================================
// SECTION 20: ARDUINO SETUP
// =============================================================================

RTC_DATA_ATTR bool setupScreenDrawn = false;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Wire.begin(PMU_SDA, PMU_SCL);
  delay(50);
  
  if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);
    PMU.disableVbusVoltageMeasure();
    PMU.disableSleep();
  }
  
  initUSBComposite();
  
  delay(2000);
  
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
  
  USBSerial.println("\n======================================================================");
  USBSerial.println("              IBIS DASH V5.1 - Smart Refresh + Cycling");
  USBSerial.println("              Board identifies as: Ibis Dash (CDC+HID)");
  USBSerial.println("======================================================================");
  USBSerial.print("Configured: "); USBSerial.println(isConfigured ? "YES" : "NO");
  USBSerial.print("Boot count: "); USBSerial.println(bootCount);
  USBSerial.print("USB Composite: "); USBSerial.println(usbCompositeStarted ? "ACTIVE" : "FAILED");
  
  initWatchdog();
  
  USBSerial.println("Checking factory reset...");
  if (checkFactoryReset()) {
    setupScreenDrawn = false;
  }
  USBSerial.println("[OK] No factory reset");
  
  USBSerial.println("Checking crash loop...");
  if (checkCrashLoop()) {
    enterSafeMode();
  }
  USBSerial.println("[OK] No crash loop");
  
  USBSerial.println("Initializing PMU IRQ...");
  pmu_irq_init();
  USBSerial.println("[OK] PMU IRQ ready");
  
  delay(900);
  USBSerial.println("Verifying PMU...");
  if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    USBSerial.println("PMU init failed!");
  } else {
    USBSerial.println("[OK] PMU verified");
  }
  
  print_wakeup_reason();
  
  USBSerial.println("Configuring PMU for awake state...");
  pmu_configure_awake();
  USBSerial.println("[OK] PMU configured");
  
  USBSerial.println("PMU stabilization (2s)...");
  delay(2000);
  feedWatchdog();
  printBatteryStatus();
  
  bool usbConnected = PMU.isVbusIn();
  USBSerial.print("USB Status: ");
  USBSerial.println(usbConnected ? "CONNECTED" : "DISCONNECTED");
  
  USBSerial.println("Initializing display...");
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  epd_deep_init();
  display.init(115200, true, 2, false);
  display.setRotation(2);
  USBSerial.println("[OK] Display ready");
  
  // ===== MAIN LOGIC =====
  
  bool hasWifi  = (WIFI_SSID.length() > 0);
  bool hasStrava = hasStravaCredentials();
  bool fullyConfigured = hasWifi && hasStrava;
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool isUsbWake    = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0);
  bool isTimerWake  = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
  bool isButtonWake = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1);
  bool isPowerOn    = (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED);
  
  USBSerial.print("Has WiFi: ");    USBSerial.println(hasWifi       ? "YES" : "NO");
  USBSerial.print("Has Strava: "); USBSerial.println(hasStrava      ? "YES" : "NO");
  USBSerial.print("Fully Configured: "); USBSerial.println(fullyConfigured ? "YES" : "NO");
  USBSerial.print("Wake Type: ");
  if (isUsbWake)    USBSerial.println("USB");
  else if (isTimerWake)  USBSerial.println("TIMER");
  else if (isButtonWake) USBSerial.println("BUTTON");
  else if (isPowerOn)    USBSerial.println("POWER-ON");
  else                   USBSerial.println("UNKNOWN");
  
  if (!fullyConfigured) {
    USBSerial.println("\n>>> SETUP REQUIRED <<<");
    
    if (!setupScreenDrawn || isPowerOn) {
      USBSerial.println("Drawing setup screen...");
      drawSetupScreen();
      setupScreenDrawn = true;
    } else if (isUsbWake) {
      USBSerial.println("USB wake - setup screen already displayed, staying idle");
    } else {
      USBSerial.println("Setup screen already drawn - skipping");
    }
    
    if (usbConnected) {
      USBSerial.println("\n>>> USB CONNECTED - Staying awake for setup <<<");
    } else {
      USBSerial.println("\n>>> ON BATTERY - Going to sleep <<<");
      setupScreenDrawn = false;
      go_to_deep_sleep();
    }
    
  } else {
    USBSerial.println("\n>>> FULLY CONFIGURED <<<");
    
    bool shouldUpdate = false;
    bool forceRedraw  = false;  // button press always forces redraw
    
    if (isUsbWake) {
      USBSerial.println("USB wake - staying idle, NOT fetching data");
      shouldUpdate = false;
      
    } else if (isButtonWake) {
      USBSerial.println("Button wake - fetching fresh data (forced redraw)");
      shouldUpdate = true;
      forceRedraw  = true;  // BOOT button always redraws
      
    } else if (isTimerWake) {
      USBSerial.println("Timer wake - fetching scheduled update");
      shouldUpdate = true;
      forceRedraw  = false;  // skip draw if nothing new
      
    } else if (isPowerOn || bootCount == 1) {
      USBSerial.println("First boot or power-on - fetching initial data");
      shouldUpdate = true;
      forceRedraw  = true;   // always draw on first boot
      
    } else {
      USBSerial.println("Unknown wake - fetching data to be safe");
      shouldUpdate = true;
      forceRedraw  = false;
    }
    
    if (shouldUpdate) {
      if (wasManualWake) {
        USBSerial.println("Manual wake - stabilizing...");
        for (int i = 3; i > 0; i--) {
          USBSerial.print(i); USBSerial.println("...");
          delay(1000);
          feedWatchdog();
        }
        blinkLED(3);
      }
      
      bool forceFetch = (bootCount == 1) || wasManualWake || isTimerWake;
      updateStravaAndDisplay(forceFetch, forceRedraw);
      blinkLED(2);
    } else {
      USBSerial.println("Skipping update - dashboard already current");
    }
    
    usbConnected = PMU.isVbusIn();
    
    if (usbConnected) {
      USBSerial.println("\n>>> USB CONNECTED - Staying awake for editing <<<");
    } else {
      USBSerial.println("\n>>> ON BATTERY - Going to sleep <<<");
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
      
      USBSerial.print("⚠️  USB disconnect check ");
      USBSerial.print(usbDisconnectCount);
      USBSerial.print("/10 (PMU: ");
      USBSerial.print(pmuSaysConnected ? "ON" : "OFF");
      USBSerial.print(", USBSerial: ");
      USBSerial.print(serialWorks ? "WORKS" : "DEAD");
      USBSerial.println(")");
      
      if (usbDisconnectCount >= 10) {
        USBSerial.println("\n╔════════════════════════════════════════════╗");
        USBSerial.println("║  USB CONFIRMED DISCONNECTED (10 checks)   ║");
        USBSerial.println("║  Going to sleep...                        ║");
        USBSerial.println("╚════════════════════════════════════════════╝\n");
        delay(500);
        go_to_deep_sleep();
        return;
      }
      
    } else {
      if (usbDisconnectCount > 0) {
        USBSerial.print("[Ibis Dash] USB reconnected (was at ");
        USBSerial.print(usbDisconnectCount);
        USBSerial.println("/10) - staying awake");
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
      USBSerial.println("\n>>> Executing requested sleep (USB verified off) <<<");
      delay(200);
      go_to_deep_sleep();
      return;
    } else {
      USBSerial.println("⚠️  Sleep requested but USB still connected - STAYING AWAKE");
      USBSerial.print("    PMU: ");
      USBSerial.print(pmuSaysConnected ? "CONNECTED" : "DISCONNECTED");
      USBSerial.print(", USBSerial: ");
      USBSerial.println(serialWorks ? "WORKS" : "DEAD");
    }
  }
  
  // ── BOOT BUTTON: manual refresh (always forces redraw) ──────────────────
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    USBSerial.println("\n>>> BOOT BUTTON - MANUAL REFRESH <<<");
    while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); feedWatchdog(); }
    blinkLED(2);
    
    loadConfiguration();
    bool hasWifi   = (WIFI_SSID.length() > 0);
    bool hasStrava = hasStravaCredentials();
    
    if (hasWifi && hasStrava) {
      USBSerial.println("Fetching fresh Strava data (forced redraw)...");
      updateStravaAndDisplay(true, true);  // forceFetch=true, forceRedraw=true
      
      PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
      delay(50);
      bool pmuSaysConnected = PMU.isVbusIn();
      bool serialWorks      = (bool)USBSerial;
      
      if (!pmuSaysConnected && !serialWorks) {
        USBSerial.println(">>> On battery - sleeping <<<");
        go_to_deep_sleep();
        return;
      } else {
        USBSerial.println(">>> USB connected - staying awake <<<");
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
