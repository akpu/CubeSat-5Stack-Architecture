# CubeSat 5-Stack Electronics Architecture 

![CubeSat Full Assembly](Cubesat_Pictures/Full_CubeSat.png)

A modular **1U CubeSat electronic system** developed for balloon-based communication, telemetry, imaging, and weak-signal beacon experiments.

This project focuses on the **design, development, integration, and validation of a complete CubeSat electronics stack** consisting of five modular PCB subsystems, onboard firmware, ground station software, and 3D mechanical integration.

---

# Project Overview

The CubeSat electronics architecture follows a modular stacked PCB approach consisting of:

1. Main On-Board Computer (OBC) Board  
2. Communication Board  
3. Secondary Controller Board  
4. Power Management Board  
5. Battery and Payload Board  

The system integrates embedded processing, RF communication, telemetry acquisition, imaging payload, and beacon transmission capabilities.

---

# System Architecture

![System Architecture](Cubesat_Pictures/System_Architecture.png)

---

# Hardware Implementation

## Onboard Computing

- **ESP32-WROOM-32**
  - Main On-Board Computer
  - Telemetry processing
  - Sensor interfacing
  - Communication control

- **Arduino Nano**
  - Secondary controller
  - WSPR beacon generation
  - Communication assistance

---

## Communication Subsystem

### Telemetry

- LoRa SX1278 (433 MHz)
- Long-range low-power telemetry transmission

### RF Communication

- SA868 VHF/UHF Transceiver
- APRS communication
- SSTV image transmission

### Beacon

- Si5351 Frequency Synthesizer
- WSPR weak-signal beacon system

---

## Sensors and Monitoring

- Neo-6M GPS Module
- HW-290 (MPU6050 + QMC5883L + BMP180)
- INA219 Voltage and Current Monitoring Sensor

Measured parameters:

- GPS coordinates
- Altitude
- Orientation
- Temperature
- Pressure
- Battery voltage
- Current consumption

---

# PCB Design

Designed using:

- KiCad Schematic Capture
- KiCad PCB Layout

The electronics system consists of five custom-designed PCB modules:

| Stack | Function |
|---|---|
| Stack 1 | Main OBC Board |
| Stack 2 | Communication Board |
| Stack 3 | Secondary Controller Board |
| Stack 4 | Power Management Board |
| Stack 5 | Battery and Payload Board |

PCB design files:

```
Hardware_Design/
```

Available files:

- KiCad schematic (`.kicad_sch`)
- KiCad PCB layout (`.kicad_pcb`)
- KiCad project files (`.kicad_pro`)

---

# 3D Mechanical Design

![3D Model](Cubesat_Pictures/CubeSat_3D_Model.png)

Mechanical design and integration were developed using:

- Fusion 360

Features:

- CubeSat structural model
- PCB stack arrangement
- Component placement visualization
- Mechanical enclosure design

Files:

```
3D_Design/
```

---

# Firmware Development

Embedded software was developed for:

```
Firmware/
```

Includes:

## CubeSat Onboard Firmware

### ESP32

- Telemetry acquisition
- LoRa communication
- Sensor interfacing
- Payload control

### ESP32-CAM

- Image capture
- SSTV image transmission

### Arduino Nano

- WSPR beacon generation
- Secondary control functions

---

## Ground Station Software

The ground station receives and displays CubeSat telemetry data.

Features:

- GPS tracking
- Altitude monitoring
- Sensor visualization
- Battery monitoring
- Attitude data display

---

# Ground Station

![Ground Station](Cubesat_Pictures/Ground_Station.png)

The ground station was developed to decode and visualize received telemetry from the CubeSat system.

---

# Testing and Validation

The system was experimentally validated through rooftop ground testing.

## LoRa Telemetry

- Sensor telemetry transmission
- Real-time dashboard visualization

## APRS Communication

- Packet transmission
- RTL-SDR reception
- SoundModem decoding

## SSTV Imaging

- ESP32-CAM image capture
- SSTV encoding
- MMSSTV decoding

## WSPR Beacon

- Si5351 beacon generation
- WSJT-X decoding verification

Testing results:

```
Testing/
```

---

# Documentation

Complete project documentation:

```
Documentation/
```

Includes:

- Project Report
- Complete System Schematic

---

# Repository Structure

```
CubeSat-5Stack-Architecture

├── Documentation
├── Hardware_Design
├── 3D_Design
├── Firmware
├── Testing
├── Calculations
├── Cubesat_Pictures
└── README.md
```

---

# Project Team

Bachelor of Technology  
Electronics and Communication Engineering  
NSS College of Engineering, Palakkad

Team Members:

- Abhishek B
- Akshay S
- Arunkrishna P U
- Kiran S M

---

# Acknowledgement

This project was developed as part of the B.Tech Final Year Project under the Department of Electronics and Communication Engineering, NSS College of Engineering, Palakkad.
