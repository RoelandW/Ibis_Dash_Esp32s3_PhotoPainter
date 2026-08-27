# Ibis Dash Quick Start Guide

## Setup

1. **Flash the firmware** (`firmware/IBIS_V40/IBIS_V40.ino`) to your ESP32-S3-PhotoPainter using Arduino IDE — board settings are in the README
2. **Deploy the middleware** ([PulseConnect](https://github.com/RoelandW/PulseConnect)) and note its URL + app key
3. **Run `app/Ibis.py`** on your computer (board connected via USB; needs Python with pyserial + tkinter)
4. **Follow the setup wizard** — WiFi, Garmin middleware details, your name, sport(s) and goal(s)
5. **Done!** Your Garmin stats will appear on the display

You can change your preferences anytime by running the app again.

---

## Buttons

| Button | What it does |
|--------|--------------|
| **BOOT** | Refresh - fetches new data from Garmin |
| **KEY** | Nothing (yet) |
| **PWR** | Hold 4 seconds to power off |

---

## Battery Life

| Refresh Rate | Battery Life |
|--------------|--------------|
| Hourly | 3-5 days |
| Every 6 hours | 2-3 weeks |
| Every 12 hours | ~1 month |
| Daily | ~2 months |
| Every 2 days | 3-4 months |
| Weekly | 6+ months |

---

## Battery Indicator

- **Blue headers** = Low battery (charge soon!)
- **Red corner with %** = Battery percentage when low
- **"I-MUUT! 100%"** = Fully charged on USB

---

## Need to Change Settings?

1. Connect board to PC via USB
2. Run `app/Ibis.py`
3. Update your preferences
4. Click "Finish Setup"

---

## Need to Wipe Everything?

**Option 1:** In the setup app → Options tab → "Wipe Data"

**Option 2:** Hold BOOT button for 5+ seconds during startup

---

## Questions?

Check the full README.md for detailed troubleshooting and setup instructions.
