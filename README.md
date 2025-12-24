# ESP32 Biometric Attendance System

A robust, distributed attendance tracking system built on the **ESP32-S3** platform. This project integrates biometric security (fingerprint), audio feedback, and a visual interface to provide a seamless attendance logging experience, synchronized in real-time with a central Python/Flask server.

## 🌟 Features

* **Biometric Identification**: Fast 1:N fingerprint matching using R307/AS608 optical sensors.
* **Real-time Sync**: Instantly logs attendance to a central server via WiFi.
* **Audio Feedback**: Voice prompts for "Success", "Try Again", "Out of Service", etc., using a DFPlayer Mini.
* **Visual Interface**: Clear status updates on a 1.47" IPS LCD (ST7789).
* **Robust Network Handling**: Automatic WiFi reconnection and server retry logic.
* **Web Dashboard**: Python Flask-based admin dashboard to view real-time logs, manage users, and view statistics.
* **Admin Tasks**: Keypad support for local device management (PIN protected).

---

## 🛠 Hardware Requirements

| Component     | Model/Specs       | Notes                                           |
|:------------- |:----------------- |:----------------------------------------------- |
| **MCU**       | ESP32-S3 DevKit   | Application Logic & WiFi                        |
| **Biometric** | R307 / AS608      | Optical Fingerprint Sensor (UART)               |
| **Audio**     | DFPlayer Mini     | MP3 Player Module + 8Ω Speaker                  |
| **Display**   | 1.47" IPS LCD     | 172x320 Resolution, ST7789 Driver (SPI)         |
| **Input**     | 4x4 Matrix Keypad | Membrane or Mechanical                          |
| **Storage**   | MicroSD Card      | For storing voice prompt MP3s (Formatted FAT32) |

### 🔌 Hardware Pinout

Configuration logic is located in `main/app_config.h`.

| Device          | ESP32 Pin         | Function                    |
|:--------------- |:----------------- |:--------------------------- |
| **Fingerprint** | API: UART1        |                             |
|                 | GPIO 17           | TX (Connect to Sensor RX)   |
|                 | GPIO 18           | RX (Connect to Sensor TX)   |
| **Audio (MP3)** | API: UART2        |                             |
|                 | GPIO 41           | TX (Connect to DFPlayer RX) |
|                 | GPIO 42           | RX (Connect to DFPlayer TX) |
| **Display**     | API: SPI2         |                             |
|                 | GPIO 11           | MOSI                        |
|                 | GPIO 12           | SCLK                        |
|                 | GPIO 10           | CS (Chip Select)            |
|                 | GPIO 9            | DC (Data/Command)           |
|                 | GPIO 46           | RST (Reset)                 |
|                 | GPIO 45           | BL (Backlight)              |
| **Keypad**      | Matrix            |                             |
|                 | Rows: 1, 2, 21, 4 |                             |
|                 | Cols: 5, 6, 7, 8  |                             |
| **Debug**       | UART0             | Console Log (GPIO 43/44)    |

---

## 💻 Software Requirements

1. **ESP-IDF v5.x**: The official Espressif IoT Development Framework.
2. **Python 3.x**: For running the backend server.
3. **Python Packages**: `Flask`, `Werkzeug` (see `web_server/requirements.txt`).

---

## 🚀 Installation & Setup

### 1. Backend Server Setup

The backend receives attendance logs and serves the dashboard.

1. Navigate to the `web_server` directory:
   
   ```bash
   cd web_server
   ```

2. Install dependencies:
   
   ```bash
   pip install -r requirements.txt
   ```

3. Run the server:
   
   ```bash
   python server.py
   ```
   
   *The server runs on port **8063** by default. Access the dashboard at `http://localhost:8063`.*

### 2. Firmware Configuration

1. Open `main/app_config.h`.

2. **WiFi Settings**: Update `WIFI_SSID` and `WIFI_PASSWORD` with your network credentials.
   
   ```c
   #define WIFI_SSID "Your_WiFi_SSID"
   #define WIFI_PASSWORD "Your_WiFi_Password"
   ```

3. **Server URL**: Update `HTTP_SERVER_URL` to point to your computer's IP address where the Python server is running.
   
   ```c
   #define HTTP_SERVER_URL "http://<YOUR_PC_IP>:8063/attendance"
   ```

4. **Time Sync**: Configure the NTP server and Timezone.
   
   ```c
   #define NTP_SERVER "Your_NTP_Server"
   #define TIMEZONE "Your_Timezone"  // e.g., "EET-2" for UTC+2
   ```

5. **Admin Security**: The default Admin PIN is `000000`. You can change this in `app_config.h` for better security.
   
   ```c
   #define ADMIN_PIN "000000"
   ```

### 3. SD Card Setup (For Audio)

Format a MicroSD card to **FAT32** and copy the following MP3 files to the root (or `mp3` folder depending on DFPlayer specific behavior, usually root is fine for index-based play):

* `001.mp3`: "Attendance Recorded" / Success
* `002.mp3`: "Try Again" / Failure
* `003.mp3`: Beep
* `004.mp3`: "System Out of Service"

*(Note: Configuration defines these indices in `app_config.h`)*

### 4. Build & Flash

Connect your ESP32-S3 to the computer.

```bash
# Set target (first time only)
idf.py set-target esp32s3

# Build the project
idf.py build

# Flash to board (replace /dev/ttyACM0 with your serial port)
idf.py -p /dev/ttyACM0 flash monitor
```

## 

---

## 🔢 Manual User Management (Keypad)

The system supports local management via the 4x4 keypad.

### 🔑 Keypad Shortcuts via Idle Screen

| Key   | Function        | Description                             |
|:-----:|:--------------- |:--------------------------------------- |
| **A** | **Scan**        | Ready state (Default)                   |
| **B** | **Manual Log**  | Manually log attendance by entering ID  |
| **D** | **Remove User** | Delete a fingerprint ID from the device |
| **#** | **Admin Mode**  | Enter Admin Menu (Add New User)         |

### ➕ Add New User (Enroll Fingerprint)

1. Press **`#`** on the keypad.
2. Enter the **Admin PIN** (Default: `000000`) and press **`#`**.
3. Enter a **User ID** (1-200) to assign to the new fingerprint.
4. Press **`#`** to confirm.
5. Follow on-screen instructions:
   * **Step 1**: "Place Finger" -> Place finger on sensor.
   * **Step 2**: "Place Again" -> Lift and place the SAME finger again.
6. **Success**: Screen shows "SUCCESS!" and the ID is registered.

### ➖ Remove User

1. Press **`D`** on the keypad.
2. Enter the **User ID** you wish to delete.
3. Press **`#`** to confirm deletion.
4. Screen will confirm "DELETED!" or show error if empty.

### 📝 Manual Attendance Entry

If the fingerprint sensor fails or is redundant:

1. Press **`B`** on the keypad.
2. Type the **User ID**.
3. Press **`#`** to submit.
4. The system logs the attendance to the server as if the finger was scanned.

**(Press `*` at any time to Cancel/Back)**

## 📝 License

This project is open source and available under the [MIT License](LICENSE).
