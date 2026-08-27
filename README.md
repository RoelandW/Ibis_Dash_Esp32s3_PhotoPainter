# 🪶 Ibis Dash - Garmin E-Paper Dashboard

An e-paper dashboard that shows your Garmin stats. Written with a ton of frustration, mass amounts of trial and error, and the help of Claude. But hey, it works now!

*Formerly a Strava dashboard — until Strava put its API behind a paid subscription in June 2026. So now it talks to Garmin instead. Their loss.*

---

## What It Does

Shows your running/cycling/swimming/hiking/walking stats on a nice e-ink screen. Distance, time, activity count, your latest route on a real map, progress toward your yearly goal. Supports **two sports side by side**, each with its own goal. Updates automatically. Runs on battery for weeks. Looks cool on your desk.

That's it. That's the project.

---

## What I Used

- **Waveshare ESP32-S3-PhotoPainter** - comes with the 7.5" color e-paper display already attached, LiPo battery, and USB-C cable all included
- **Arduino IDE** - free
- **Ibis Setup app** - included here, also free (I made it)
- **A Garmin account** - and a watch that feeds it
- **Garmin middleware** - a small self-hosted backend that logs into Garmin for you and serves the board a tidy dashboard JSON (mine runs on Render's free tier; not public yet)
- **A Google Maps API key** *(optional)* - for the map background behind your latest route. Needs billing enabled on the Google Cloud project (the free monthly quota is plenty for a dashboard that refreshes twice a day)

---

## How Hard Is This?

If you can follow instructions, you can do this. The actual coding part is done - you just upload my code to the board and configure it with the app. No coding required on your end.

The "hardest" part is deploying the middleware, and even that is mostly clicking through Render.

---

## Setup (The Short Version)

1. Install Arduino IDE
2. Add ESP32 board support
3. Install some libraries (GxEPD2, ArduinoJson, XPowersLib, TJpg_Decoder)
4. Set the right board settings (see below - this part matters!)
5. Upload `firmware/IBIS_V40/IBIS_V40.ino` to your board
6. Deploy the Garmin middleware and note its URL + app key
7. Run `app/Ibis.py`, connect, enter WiFi + Garmin middleware details
8. Done. Go for a run. Or a ride. Or both — it does two sports now.

---

## Arduino IDE Settings

⚠️ **Get these right or your board will be sad:**

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | **Enabled** |
| USB Mode | **Hardware CDC and JTAG** |
| Flash Mode | **DIO** (NOT OPI!) |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |

The Flash Mode one is important. Ask me how I know. The USB Mode one is also important. Ask Claude how it knows.

Or with `arduino-cli`, that's this FQBN:

```
esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=dio,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB
```

---

## Connecting to Garmin

Garmin doesn't hand out personal API keys, so the board talks to a small middleware that logs in on your behalf:

1. Deploy the middleware (Render free tier works) with an `APP_KEY` of your choosing
2. In the Ibis Setup app, open the **Garmin** tab
3. Enter the middleware URL, your app key, and your Garmin email + password (plus MFA code if Garmin asks)
4. The app registers the board and stores only a short board token — your Garmin password never touches the board or disk

---

## Buttons

| Button | What it does |
|--------|--------------|
| KEY | Absolutely nothing. It's there for vibes. |
| BOOT | Force screen refresh and data fetch (hold 5+ seconds for factory reset) |
| PWR | Power on/off |

---

## What's In Here

```
ibis-dash/
├── firmware/
│   └── IBIS_V40/
│       ├── IBIS_V40.ino     ← The Arduino code
│       ├── dashboard_new.h  ← Dual-sport dashboard layout
│       ├── ibis_logos.h     ← Pixel art logos (yes I made these)
│       └── Fonts/           ← Maison Neue in every size I needed
├── app/
│   └── Ibis.py              ← Setup app (Python, needs pyserial + tkinter)
├── docs/
│   └── QUICKSTART.md        ← The even shorter version
├── README.md                ← You are here
└── LICENSE                  ← MIT-ish, see below
```

---

## Why "Ibis"?

Because, believe it or not, I am a bird. 🪶

---

## License

MIT - use it, modify it, make it better. Just don't make it commercial without my consent.

---

## Credits

Built with [GxEPD2](https://github.com/ZinggJM/GxEPD2), [ArduinoJson](https://arduinojson.org/), [XPowersLib](https://github.com/lewisxhe/XPowersLib), [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder), the Garmin data my watch sweats for, mass debugging, mass coffee, and mass vibes.

Shoutout to Nerdland and some fellow nerds. 🤓

---

Happy tracking! 🏃‍♂️🚴🪶
