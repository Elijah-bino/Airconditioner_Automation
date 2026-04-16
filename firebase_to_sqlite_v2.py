import firebase_admin
from firebase_admin import credentials, db
import sqlite3
import datetime
import time

# ===== CONFIGURE THESE =====
SERVICE_ACCOUNT = {
    "type": "service_account",
    "project_id": "ac-logger-bf8e2",
    "private_key_id": "aa648b8fd34b135d22cadbe7f04b6c4c015e004f",
    "private_key": "-----BEGIN PRIVATE KEY-----\nMIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQDLzI7k9b9S/tAC\nw03vJB/j3SqLGdJk7kXabfRMgMAJmymBpDz0Y6icCMeHUwNNBLmMNCPx2GHSsPAI\n+dAWqG9o4KOehLqKr2DdruQ+g0AaQ3Vej88XZUVNmprWjEwZww8LGQX2Wzt6SKga\nK+Ib2jIxcLsbDw08j6KV3TB+/TWMhOPNbN0M7iMHb61AEFQX3B8BNZ2Ws3mU4QCd\nkjGaE/PzRn43XE0nrpECrLJe8CT/nE/b5yeEUDlhnS+R2xG9DI70agubVD+TccbW\nA5t83V0nLWn5JWiA7CON0qsWOsIQiaerbgeE4R6lmBczTN98gMycWw+VVD7Pa0cS\nOjVCRONnAgMBAAECggEAVJKIcAptQaddse/TfRx8IWbL+1qVXW4lzc3YVhEN4E6k\nDFkYNIDQ3SOljYpM8d62kzCmA/w8KB9mFQt6J5TxWs/yBlvlP50k8QndTPXRmbSJ\n1dqO4ZIUju5MOCGTiRNRwFS7jS7yAT56iBXS6KUFNFAoTUPTReEDYvad4+z3Fl7m\nnHr/KA+c9jdztiWfMa3WUyc0cq2SUeywFAz7m1PHZFmxMP1uk6QtWWQhdUQvm8f5\nNcMxLVEPQl2DK2zlrcPH3g+rx6dwcDNFALeAMfIVtp+1gXzCF3HuS3ei+lL0dYhN\nMc1MxuVkhSzj+xjTKEO4g8peaIMppc5gJAveY9mKlQKBgQD/Qo1PE086MxBUgFSR\nlg+rwDepontZqLIY8Kqm2Iw+XSC2gIe59eev32neSLI9TGywFfm+l7qvcY0Zl9OM\nF2RDXrnkjmSCrLHj5KJcxUCzSYvqfCTKJrFqcUoMaVowJVTCixGmGJ8/+SX98j0E\n3ofdO+qQWSOw9k1gYnLPNvfgxQKBgQDMY9AnVxRjTuDhGrm8zMg4ExgT5LAJX7Qy\niNj0gzZr570L1/VLOhJv/3HiplRgAeuLezQRpZ9H0SccEnlonuPNhxL3fQiWWDFa\ncglQg1H79E/o6Wn0J48i60HkKFVwV8JiUDUCkjkS/YQLAy7PlFuytcFKmoQ0qRPU\naX37kmkeOwKBgQDUCcyoaznc9p4SE6gsZ6MM4NdcKoScre2DmieoASo+g82wzIPy\njbqv9b5Tz79//AQtIZQcJp4FLyYupFgufx/idsWkfkQv6JS84iedOJ3OUMHRPt6w\nrgOikAka1kj8xYXrNOqsNrfSxHwyvw7BBLmHpUXsMp/bWD5eWmlUiCzQoQKBgQDJ\njUgv8zQ8tgUWlldA8iQi/9lHjMV3iPPWvZJMlLPu9qg67KyMkWirHxi8K8+amWog\nss7m9A89L+hES4eD/uOfwqqWeaslLzUD/t5mkgkfz42ZncuJJBWHlrz0dD3EuhYL\nAxMzROSZcHxsq6fWtXMzqP5CRltG3tCaL6k3kuqsDwJ/NsoQK/DMEW/tPwvBCVB8\n7wFpCaPOjstZrXrau5J9XAu8sjmIqNeVg2HoQlfEV4hQ/qh4083j8f5/bVO0fSue\n1JLW22GgDl/+QJsLOMbe9D+SOstTnQyTFvWwO/9ApW/LhG6wnr7CR9Gee65b9iGv\nsFg4vYgO/SSV41e2418Tuw==\n-----END PRIVATE KEY-----\n",
    "client_email": "firebase-adminsdk-fbsvc@ac-logger-bf8e2.iam.gserviceaccount.com",
    "client_id": "105456652443546142480",
    "auth_uri": "https://accounts.google.com/o/oauth2/auth",
    "token_uri": "https://oauth2.googleapis.com/token",
    "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
    "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/firebase-adminsdk-fbsvc%40ac-logger-bf8e2.iam.gserviceaccount.com",
    "universe_domain": "googleapis.com"
    
}
DATABASE_URL   = "https://ac-logger-bf8e2-default-rtdb.asia-southeast1.firebasedatabase.app/"
SQLITE_DB_PATH = "ac_logs.db"
POLL_INTERVAL  = 10        # check Firebase every 10 sec
FULL_SYNC_INTERVAL = 900   # 15 minutes
# ===========================

last_full_sync = 0

# ===== INIT DB =====
def init_db():
    conn = sqlite3.connect(SQLITE_DB_PATH)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS ac_logs (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            firebase_key  TEXT UNIQUE,
            power         INTEGER,
            mode          TEXT,
            eco           INTEGER,
            powerful      INTEGER,
            temp_room     REAL,
            temp_set      REAL,
            humidity      REAL,
            timestamp     INTEGER,
            created_at    DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()
    print("SQLite ready: ac_logs.db")

# ===== SAVE RECORD =====
def save_record(key, data):
    try:
        conn = sqlite3.connect(SQLITE_DB_PATH)
        c = conn.cursor()

        c.execute("""
            INSERT OR IGNORE INTO ac_logs
            (firebase_key, power, mode, eco, powerful, temp_room, temp_set, humidity, timestamp)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            str(key),
            int(data.get("power", False)),
            data.get("mode", "UNKNOWN"),
            int(data.get("eco", False)),
            int(data.get("powerful", False)),
            data.get("temp_room", 0.0),
            data.get("temp_set", 0.0),
            data.get("humidity", 0.0),
            data.get("timestamp", 0)
        ))

        if c.rowcount > 0:
            now = datetime.datetime.now().strftime("%H:%M:%S")
            print(f"[{now}] Saved → {data.get('mode')} | Power:{data.get('power')} | Temp:{data.get('temp_room')}°C | Hum:{data.get('humidity')}%")

        conn.commit()
        conn.close()

    except Exception as e:
        print(f"SQLite error: {e}")

# ===== FETCH + SYNC =====
def poll_firebase(full_sync=False):
    try:
        ref = db.reference("/")
        data = ref.get()

        if not data:
            print("Firebase empty — waiting...")
            return

        ac_logs = data.get("ac_logs", {})

        if not ac_logs:
            print("No ac_logs yet...")
            return

        print("Checking Firebase...")

        if isinstance(ac_logs, list):
            for i, record in enumerate(ac_logs):
                if isinstance(record, dict):
                    save_record(i, record)

        elif isinstance(ac_logs, dict):
            for key, record in ac_logs.items():
                if isinstance(record, dict):
                    save_record(key, record)

    except Exception as e:
        print(f"Firebase error: {e}")

# ===== MAIN =====
if __name__ == "__main__":
    print("=== Firebase → SQLite Sync v3 ===")

    init_db()

    cred = credentials.Certificate(r"ac-logger-bf8e2-firebase-adminsdk-fbsvc-aa648b8fd3.json")
    firebase_admin.initialize_app(cred, {
        "databaseURL": "https://ac-logger-bf8e2-default-rtdb.asia-southeast1.firebasedatabase.app/"
    })

    print("Firebase connected!")
    print(f"Polling every {POLL_INTERVAL}s | Full sync every 15 min")

    last_full_sync = time.time()

    while True:
        now = time.time()

        # 🔁 Regular polling (detect new entries quickly)
        poll_firebase()

        # ⏱️ Full sync every 15 minutes
        if now - last_full_sync >= FULL_SYNC_INTERVAL:
            print("\n🔄 Running FULL SYNC...\n")
            poll_firebase(full_sync=True)
            last_full_sync = now

        time.sleep(POLL_INTERVAL)
