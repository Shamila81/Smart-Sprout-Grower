# 🌱 Smart Sprout Grower

**An IoT-Based Automated Sprouting System Using Raspberry Pi Pico W, FreeRTOS, MQTT & Home Assistant**

The **Smart Sprout Grower** is an embedded IoT system designed to automate the sprouting process by intelligently controlling **temperature, humidity, ventilation, and watering cycles**. The system uses **Raspberry Pi Pico W** running **FreeRTOS** and communicates with **Home Assistant** via **MQTT** to provide real-time monitoring and remote control.

---

## Team

This project was developed at **Metropolia University of Applied Sciences**.

- Olga Sharma
- Upeksha Sanjeewani
- **Shamila Thennakoon**
- Lihini Hewage

---

#  Features

-  Real-time temperature and humidity monitoring (BME680)
-  Water level monitoring using ultrasonic sensor
-  Automated daily **Rinse → Vent → Idle** cycle
-  Stepper motor controlled ventilation lid
-  Automatic water pump control
-  OLED display for local system status
-  EEPROM storage for:
  - Wi-Fi credentials
  - Sprouting day count
  - Lid position
-  MQTT communication with Home Assistant
-  Safety features including low-water protection and emergency stop

---

#  System Architecture

### FreeRTOS Tasks

- Controller Task
- BME680 Task
- Pump Task
- Stepper Motor Task
- OLED Display Task
- GPIO Task
- Ultrasonic Sensor Tasks
- EEPROM Task
- MQTT Task

### Communication

- FreeRTOS Queues
- Event Groups
- Software Timers

---

#  Home Assistant Dashboard

The system integrates with **Home Assistant** using MQTT.

The dashboard displays:

-  Temperature
-  Humidity
-  Sprouting Day Count
-  Sprout Readiness Status

---

#  MQTT Topics

| Topic | Description |
|--------|-------------|
| `sprout/status` | Current sprouting system status |

---

#  Hardware

- Raspberry Pi Pico W
- BME680 Environmental Sensor
- 2 × Ultrasonic Sensors
- Stepper Motor + ULN2003 Driver
- 12V Water Pump
- OLED Display
- AT24C256 EEPROM
- Custom PCB

---

#  Software & Technologies

- C
- FreeRTOS
- Raspberry Pi Pico SDK
- MQTT
- Home Assistant
- CMake
- Git

---

#  Project Structure

```
Smart-Sprout-Grower
│
├── docs/
│   └── Smart_Sprout_Grower_Report.pdf
│
├── src/
│
├── FreeRTOS-KernelV10.6.2/
│
├── README.md
├── CMakeLists.txt
└── .gitignore
```

---

#  Documentation

The complete project report is available in:

```
docs/Smart_Sprout_Grower_Report.pdf
```

---

#  Future Improvements

- Mobile application
- Cloud data storage
- OTA firmware updates
- AI-based growth prediction
- Remote notifications

---

#  My Contribution

My main contributions to this project included:

- Embedded software development
- FreeRTOS task implementation
- MQTT communication
- Home Assistant integration
- System testing and debugging
- Technical documentation

---

#  License

This project was developed for educational purposes at **Metropolia University of Applied Sciences**.

---

