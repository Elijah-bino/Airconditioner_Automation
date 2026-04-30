import numpy as np
import joblib
import time
import datetime
import os
import sys
import requests

# ===== CONFIG =====
API_KEY       = "AIzaSyBlGQ2pO2BQPXaBqYTZUIqxiW0e5or5los"
DATABASE_URL  = "https://ac-logger-bf8e2-default-rtdb.asia-southeast1.firebasedatabase.app/"
MODEL_DIR     = "ac_models_v2"

POLL_INTERVAL = 600        # 10 minutes
TOKEN_REFRESH = 55 * 60    # 55 minutes
# ==================


# ===== AUTH =====
def get_token():
    try:
        url = f"https://identitytoolkit.googleapis.com/v1/accounts:signUp?key={API_KEY}"
        res = requests.post(url, json={"returnSecureToken": True})
        data = res.json()

        token = data.get("idToken")
        if not token:
            print(f"[AUTH ERROR] {data}")
            sys.exit(1)

        print("[OK] Authenticated")
        return token

    except Exception as e:
        print(f"[AUTH ERROR] {e}")
        sys.exit(1)


# ===== FIREBASE READ - FIXED FOR ARRAY FORMAT =====
def get_latest_two_sensors(token):
    try:
        # Get the counter
        counter_url = f"{DATABASE_URL}/sensor_logs_counter.json?auth={token}"
        counter_res = requests.get(counter_url)
        
        if counter_res.status_code == 200:
            counter = counter_res.json()
            print(f"[DEBUG] Counter = {counter}")
            
            if counter is None or counter == 0:
                return None, None, None
            
            # Array indices: 0 to counter-1
            latest_idx = counter - 1
            prev_idx = latest_idx - 1 if latest_idx > 0 else None
            
            # Get latest reading by index
            latest_url = f"{DATABASE_URL}/sensor_logs/{latest_idx}.json?auth={token}"
            latest_res = requests.get(latest_url)
            
            if latest_res.status_code == 200:
                latest_reading = latest_res.json()
                
                # Get previous reading if available
                prev_temp = None
                if prev_idx is not None:
                    prev_url = f"{DATABASE_URL}/sensor_logs/{prev_idx}.json?auth={token}"
                    prev_res = requests.get(prev_url)
                    if prev_res.status_code == 200 and prev_res.json():
                        prev_reading = prev_res.json()
                        prev_temp = float(prev_reading.get("temp", 0.0))
                
                return str(latest_idx), latest_reading, prev_temp
        
        # Fallback: Try to get all as array
        logs_url = f"{DATABASE_URL}/sensor_logs.json?auth={token}"
        logs_res = requests.get(logs_url)
        
        if logs_res.status_code == 200:
            all_logs = logs_res.json()
            
            if all_logs and isinstance(all_logs, list):
                num_logs = len(all_logs)
                print(f"[DEBUG] Found {num_logs} logs as array")
                
                if num_logs == 0:
                    return None, None, None
                
                # Last element is latest
                latest_reading = all_logs[-1]
                latest_idx = num_logs - 1
                
                # Get previous temp if available
                prev_temp = None
                if num_logs >= 2:
                    prev_reading = all_logs[-2]
                    prev_temp = float(prev_reading.get("temp", 0.0))
                
                return str(latest_idx), latest_reading, prev_temp
        
        print("[READ] No sensor logs found")
        return None, None, None

    except Exception as e:
        print(f"[READ ERROR] {e}")
        import traceback
        traceback.print_exc()
        return None, None, None


# ===== FIREBASE WRITE =====
def push_commands(payload, token):
    try:
        url = f"{DATABASE_URL}/commands.json?auth={token}"
        response = requests.put(url, json=payload)
        if response.status_code == 200:
            print(f"[DEBUG] ✓ Commands pushed to /commands")
        else:
            print(f"[DEBUG] ✗ Push failed: {response.status_code}")
    except Exception as e:
        print(f"[WRITE ERROR] {e}")


# ===== MODEL LOADING =====
def load_models(model_dir):
    paths = {
        "mode":          f"{model_dir}/mode_model.pkl",
        "eco_heat":      f"{model_dir}/eco_model_heat.pkl",
        "eco_cool":      f"{model_dir}/eco_model_cool.pkl",
        "powerful_heat": f"{model_dir}/powerful_model_heat.pkl",
        "powerful_cool": f"{model_dir}/powerful_model_cool.pkl",
        "metadata":      f"{model_dir}/metadata.pkl",
    }

    for name, path in paths.items():
        if not os.path.exists(path):
            print(f"[ERROR] Missing model: {path}")
            sys.exit(1)

    print("[OK] Models loaded")
    return {k: joblib.load(v) for k, v in paths.items()}


# ===== FEATURE ENGINEERING =====
def build_features(temp, humidity, prev_temp):
    hour        = datetime.datetime.now().hour
    hour_sin    = np.sin(2 * np.pi * hour / 24)
    hour_cos    = np.cos(2 * np.pi * hour / 24)
    temp_change = (temp - prev_temp) if prev_temp is not None else 0.0

    print(f"[DEBUG] Hour={hour}, hour_sin={hour_sin:.3f}, hour_cos={hour_cos:.3f}, temp_change={temp_change:.1f}")
    
    return np.array([[temp, humidity, hour_sin, hour_cos, temp_change]])


# ===== INFERENCE =====
def infer(models, features):
    pred_mode  = models["mode"].predict(features)[0]
    mode_proba = models["mode"].predict_proba(features)[0]

    print(f"[DEBUG] pred_mode={pred_mode}, mode_proba={mode_proba}")

    if pred_mode == 0:
        pred_eco      = models["eco_heat"].predict(features)[0]
        pred_powerful = models["powerful_heat"].predict(features)[0]
    else:
        pred_eco      = models["eco_cool"].predict(features)[0]
        pred_powerful = models["powerful_cool"].predict(features)[0]

    return {
        "mode":       1 if pred_mode == 1 else 0,
        "eco":        int(pred_eco),
        "powerful":   int(pred_powerful),
        "mode_str":   "COOL" if pred_mode == 1 else "HEAT",
        "confidence": round(float(max(mode_proba)), 4),
    }


def _ts():
    return datetime.datetime.now().strftime("%H:%M:%S")


# ===== MAIN =====
def main():
    print("=" * 50)
    print("  AC Inference Worker (10 min)")
    print("=" * 50)

    models = load_models(MODEL_DIR)
    meta   = models["metadata"]

    print(f"[OK] v{meta['model_version']} | {meta['training_samples']} samples")
    print(f"[OK] Features: {meta['feature_cols']}")

    token       = get_token()
    token_birth = time.time()
    last_key    = None

    next_run = time.time()

    print(f"\nRunning every {POLL_INTERVAL//60} minutes\n")

    while True:
        try:
            now = time.time()

            if now < next_run:
                time.sleep(1)
                continue

            next_run += POLL_INTERVAL

            # Token refresh
            if time.time() - token_birth >= TOKEN_REFRESH:
                print(f"[{_ts()}] Refreshing token...")
                token       = get_token()
                token_birth = time.time()

            # Fetch sensor data
            key, reading, prev_temp = get_latest_two_sensors(token)

            if reading is None:
                print(f"[{_ts()}] No sensor data yet")
                continue

            if key == last_key:
                print(f"[{_ts()}] No new data (key={key})")
                continue

            temp     = float(reading.get("temp", 0.0))
            humidity = float(reading.get("humidity", 0.0))

            print(f"[DEBUG] temp={temp}, humidity={humidity}, prev_temp={prev_temp}")

            if temp == 0 and humidity == 0:
                print(f"[{_ts()}] Sensor error — skipping")
                continue

            # Features & inference
            features = build_features(temp, humidity, prev_temp)
            temp_change = features[0][4]
            result = infer(models, features)

            # Push to Firebase
            payload = {
                "mode":     result["mode"],
                "eco":      result["eco"],
                "powerful": result["powerful"],
            }
            push_commands(payload, token)

            print(
                f"[{_ts()}] key={key} | "
                f"{temp:.1f}C | {humidity:.1f}% | "
                f"Δ={temp_change:+.1f} | "
                f"{result['mode_str']} ({result['confidence']:.0%}) | "
                f"eco={result['eco']} | powerful={result['powerful']}"
            )
            print("-" * 60)

            last_key = key

        except KeyboardInterrupt:
            print("\n[STOP]")
            break
        except Exception as e:
            print(f"[{_ts()}] ERROR: {e}")
            import traceback
            traceback.print_exc()


if __name__ == "__main__":
    main()