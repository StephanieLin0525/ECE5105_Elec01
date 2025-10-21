# Elec01 Turbidity Monitoring System

This repository contains the complete firmware and software for the **ENG5105 Final Design Project**  
conducted by *Team Elec01 – Electrical Engineering Discipline, Monash University (2025)*.

The project demonstrates a compact **turbidity-monitoring system** that integrates an STM32L432KC
microcontroller with a Raspberry Pi 4 web-based dashboard for real-time water-quality monitoring.

---

## 🌊 Overview

High turbidity in wastewater accelerates membrane fouling and increases energy and maintenance costs.
Our system provides continuous optical turbidity measurement using an **850 nm infrared LED and BPW34
photodiode** pair. The analog signal is conditioned by a **transimpedance amplifier (LM358)**, digitised by
the STM32’s **12-bit ADC**, and streamed to the Raspberry Pi for visualization, data logging, and alarm handling.

---

## ⚙️ System Architecture
The complete architecture is described in the *Final Design Report* (Section 6.0 & 9.0).
<img width="637" height="187" alt="image" src="https://github.com/user-attachments/assets/3647ffd6-c0df-420d-af3f-710787d20d50" />

---

## 🧠 Firmware (STM32L432KC)

**Location:** `/firmware/main.c`

**Key Features**
- 12-bit ADC sampling of PA0 (ADC1_IN5)
- Moving-average filter (N = 10)
- Voltage → NTU linear mapping (2.38 – 3.30 V → 0 – 100 NTU)
- UART2 (115200 bps 8-N-1) CSV output to Raspberry Pi
- Header included for easy parsing by the GUI
- **Build Tool:** STM32CubeIDE 1.15 or newer  
**Target Board:** NUCLEO-L432KC  
**Power:** USB 5 V, on-board 3.3 V logic reference

---

## 💻 Raspberry Pi GUI (FastAPI + WebSocket)

**Location:** `/gui/`

**Description**
A lightweight **web-based GUI** built with **FastAPI** and **HTML5 Canvas** allows real-time visualization,
threshold configuration, alarm logging, and data export.
**Run on Raspberry Pi**
```bash
cd gui
pip install -r requirements.txt
python -m uvicorn server:app --reload
# open http://127.0.0.1:8000  (or http://<pi-ip>:8000)
