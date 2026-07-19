# CubeSat 5-Stack Electronics Architecture 

A modular CubeSat electronics platform developed for balloon-based communication and telemetry experiments.

## Overview

This project presents the design and development of a 1U CubeSat electronic stack consisting of five modular subsystems:

- Main OBC Board
- Communication Board
- Secondary Controller Board
- Power Management Board
- Battery and Payload Board


## Hardware

### Controller
- ESP32-WROOM-32

### Communication
- LoRa SX1278
- SA868 VHF/UHF Transceiver

### Sensors
- Neo-6M GPS
- INA219 Power Monitoring
- HW-290 IMU

### Payload
- ESP32-CAM SSTV Imaging

### Beacon
- Arduino Nano + Si5351 WSPR Beacon


## Design Tools

- KiCad (Schematic and PCB Design)
- Fusion 360 (Mechanical Design)


## Testing

Ground-based rooftop validation was performed for:

- LoRa telemetry
- APRS communication
- SSTV image transmission
- WSPR beacon decoding


## Documentation

Complete project report and schematic documentation are available in this repository.
