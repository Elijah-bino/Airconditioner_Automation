# AC Behaviour Automation

![ESP8266](https://img.shields.io/badge/ESP8266-Wemos_D1_Mini-blue)
![Python](https://img.shields.io/badge/Python-3.11-green)
![Firebase](https://img.shields.io/badge/Firebase-Realtime_DB-orange)
![SQLite](https://img.shields.io/badge/SQLite-local_sync-lightgrey)

## The problem

From my teenage years, I spent 15–30 minutes every single day adjusting my AC remote — changing modes, toggling eco and powerful settings, adjusting temperature. It was a habit I never managed to break. So I decided to build a system that learns my behaviour and automates the AC on its own.

<img width="2252" height="1894" alt="remote_sim" src="https://github.com/user-attachments/assets/9e1bcc36-450a-499b-86df-8bfceb4a3e85" />


## How it works

```
Custom remote (Wemos D1) → IR blaster → Mitsubishi Heavy AC
        ↓
Button press + DHT11 sensor
        ↓
Firebase Realtime Database
        ↓
Python sync script → SQLite database
        ↓
Pattern recognition model → Autonomous AC control
```

## Hardware

| Component | Purpose |
|---|---|
| Wemos D1 Mini (ESP8266) | Main controller |
| IR LED + 2N2222 transistor | IR blaster circuit |
| DHT11 | Room temperature + humidity |
| 3x push buttons | ON/OFF, mode (cool/heat), eco/powerful |
| Green + Red LEDs | Mode indicators |
| 9V battery + barrel jack | Wireless power |

## Button behaviour

| Button | Action |
|---|---|
| BTN 1 | Toggle AC ON / OFF |
| BTN 2 | Toggle Cool ↔ Heat |
| BTN 3 | Cycle Normal → Eco → Powerful |

## LED indicators

| LED | Blinks | Meaning |
|---|---|---|
| Green | 1 | Cool mode |
| Green | 2 | Eco mode |
| Red | 1 | Heat mode |
| Red | 2 | Powerful mode |

## Data logged per event

Every button press and every 15 minutes, the following is pushed to Firebase and synced to SQLite:

```json
{
  "power": true,
  "mode": "COOL",
  "eco": false,
  "powerful": false,
  "temp_set": 24,
  "temp_room": 21.5,
  "humidity": 55,
  "timestamp": 123456
}
```

## Repository structure

```
ac_remote_logger/
  ac_remote_logger_final.ino   ← Arduino sketch (IR blaster + buttons + DHT11 + Firebase)
firebase_to_sqlite_v2.py       ← Syncs Firebase → SQLite in real time + every 15 mins
convert_db_to_csv.py           ← Exports SQLite to CSV for analysis
ac_logs.db                     ← SQLite database — all recorded behaviour
ac_logs.csv                    ← CSV export — 1000+ rows of behaviour data
firebase_sdk_Example.json      ← Firebase config template (no real credentials)
```

## Setup

### Arduino (Wemos D1 Mini)

1. Install libraries in Arduino IDE:
   - `IRremoteESP8266` by crankyoldgit
   - `Firebase Arduino Client Library` by Mobizt
   - `DHT sensor library` by Adafruit

2. Fill in credentials in `ac_remote_logger_final.ino`:
```cpp
#define WIFI_SSID     "your_wifi"
#define WIFI_PASSWORD "your_password"
#define API_KEY       "your_firebase_api_key"
#define DATABASE_URL  "https://your-project.firebasedatabase.app"
```

3. Flash to Wemos D1 Mini

### Python sync script

```bash
pip install firebase-admin pandas
python firebase_to_sqlite_v2.py
```

### Export to CSV

```bash
python convert_db_to_csv.py
```

## Roadmap

- [x] Custom IR remote simulator on Wemos D1

<img width="2252" height="1894" alt="remote_sim" src="https://github.com/user-attachments/assets/805da88b-bc01-4244-a0d0-b04bc2b98096" />
      
- [x] Firebase Realtime Database integration
      
<img width="2880" height="1794" alt="firebase_1" src="https://github.com/user-attachments/assets/9735f134-22b8-4b62-a6ff-339f0e55bf92" />
      
- [x] SQLite sync script (real-time + 15-min polling)
      
<img width="2332" height="1218" alt="server_loging" src="https://github.com/user-attachments/assets/763b2a64-30a2-4120-9894-a7a60e63bc9e" />

- [x] 1000+ rows of behaviour data collected
      
<img width="2880" height="1800" alt="firebase_2" src="https://github.com/user-attachments/assets/fb83e009-fb84-440c-b7c2-c5e29ff0b310" />

- [ ] Pattern recognition model on behaviour data
- [ ] Deploy model for fully autonomous AC control

## AC unit

Mitsubishi Heavy Industries — remote model `RLA502A700B`, protocol `MITSUBISHI_HEAVY_152`.

## Security note

Never commit your Firebase service account JSON or real credentials. The `firebase_sdk_Example.json` in this repo is a template only.
