# 04. FLIGHT TUNING & REGISTER CALIBRATION MATRIX
**Document Identifier:** `QVR-TUNE-233-04`  
**Revision:** v2.0 (DO-178C Level B / Benchmark Flight Calibration)  
**Target Hardware:** Matek CAN-L431 Adapter Node (STM32L431CCT6 @ 80 MHz)  
**Controlled Subsystem:** JMRRC FS2516 Granular Spreader System (Issue #233)  

---

## 1. Flight Control Register Matrix

The following parameters represent the ground-bench and flight-test verified registers embedded within the AP_Periph and ArduPilot flight controller configurations:

| Parameter Name | Target Device | Configured Value | Unit | Engineering Function & Operational Range |
|---|:---:|:---:|:---:|---|
| **`CAN_NODE`** | Matek CAN-L431 | `0` | - | Dynamic UAVCAN Node ID Allocation (Auto-allocates to ID > 10) |
| **`CAN_P1_DRIVER`**| Matek CAN-L431 | `1` | - | Primary CAN Driver Enabled on PA11/PA12 (1.0 Mbps) |
| **`CAN_P2_DRIVER`**| Matek CAN-L431 | `1` | - | Secondary Redundant Driver Support Enabled |
| **`RC_SPEED`** | Matek CAN-L431 | `400` | Hz | Global PWM Timer Clock Base |
| **`OUT1_FREQ`** | Matek CAN-L431 | `400` | Hz | Spinner ESC High-Speed PWM Clock Rate |
| **`OUT2_FREQ`** | Matek CAN-L431 | `50` | Hz | Discharge Gate Servo Analog/Digital Clock Rate |
| **`OUT1_FUNCTION`**| Matek CAN-L431 | `33` | - | Actuator Mapping: ESC Spinner Motor Drive (DroneCAN ID 0) |
| **`OUT1_MIN`** | Matek CAN-L431 | `1000` | $\mu\text{s}$ | Zero Speed / Motor Stopped Baseline |
| **`OUT1_MAX`** | Matek CAN-L431 | `2000` | $\mu\text{s}$ | Full Speed ESC Ceiling |
| **`OUT1_TRIM`** | Matek CAN-L431 | `1000` | $\mu\text{s}$ | Neutral Ground Setpoint |
| **`OUT2_FUNCTION`**| Matek CAN-L431 | `28` | - | Actuator Mapping: Discharge Gate Servo Drive (DroneCAN ID 1) |
| **`OUT2_MIN`** | Matek CAN-L431 | `1000` | $\mu\text{s}$ | Mechanical Lock-Closed Stop (Zero chemical flow) |
| **`OUT2_MAX`** | Matek CAN-L431 | `2000` | $\mu\text{s}$ | Absolute Maximum Gate Aperture |
| **`OUT2_TRIM`** | Matek CAN-L431 | `1000` | $\mu\text{s}$ | Closed Mechanical Rest Position |
| **`OUT_FAIL_TIMEOUT`**| Matek CAN-L431 | `500` | ms | Node-Level Autonomous Failsafe Interlock Trigger |
| **`OUT1_FAIL_ACT`** | Matek CAN-L431 | `1` | - | Enable Autonomous Override on Output 1 upon Bus Loss |
| **`OUT1_FAIL_VALUE`**| Matek CAN-L431 | `1000` | $\mu\text{s}$ | Commanded Output 1 Value in Failsafe (Motor Cutoff) |
| **`OUT2_FAIL_ACT`** | Matek CAN-L431 | `1` | - | Enable Autonomous Override on Output 2 upon Bus Loss |
| **`OUT2_FAIL_VALUE`**| Matek CAN-L431 | `1000` | $\mu\text{s}$ | Commanded Output 2 Value in Failsafe (Gate Locked Closed) |
| **`GPIO_PIN1`** | Matek CAN-L431 | `52` | - | Telemetry Mapping for PB0 Hopper Empty Sensor |
| **`GPIO_PIN1_MODE`**| Matek CAN-L431 | `0` | - | Input Mode |
| **`GPIO_PIN1_PULL`**| Matek CAN-L431 | `1` | - | Internal Pull-Up Resistor Enabled |

---

## 2. Flight-Calibrated Actuator Setpoints

During autonomous field missions, the flight controller dynamically adjusts actuator setpoints based on ground speed:

### 2.1 Spinner ESC Output (PA0 / TIM2_CH1):
- **Base Frequency:** $400\text{ Hz}$ ($T_{period} = 2500\ \mu\text{s}$).
- **Pre-Arm Ground Arming Baseline:** Exactly **$1000\ \mu\text{s}$** (Ensures JMRRC ESC handshake).
- **Nominal Spreading Velocity:** **$1950\ \mu\text{s}$** (Yields approximately $1100\text{ RPM}$ centrifugal disk speed for an $8.0\text{ m}$ effective broadcast swath).

### 2.2 Hopper Discharge Gate Servo (PA1 / TIM2_CH2):
- **Base Frequency:** $50\text{ Hz}$ ($T_{period} = 20000\ \mu\text{s}$).
- **Absolute Closed Position (`SERVO13_MIN`):** Exactly **$1000\ \mu\text{s}$** (Mechanical torsion spring locked against silicone sealing gasket).
- **Calibrated Spreading Aperture (`SERVO13_MAX`):** Exactly **$1485\ \mu\text{s}$** (Delivers calibrated $1.20\text{ kg/s}$ mass flow rate of Spike 20P pellets at $12.0\text{ m/s}$ forward flight speed).

### 2.3 Safe Clamping Envelope:
- Any commanded input below $1000\ \mu\text{s}$ is clamped to $1000\ \mu\text{s}$.
- Any commanded input above $2000\ \mu\text{s}$ is clamped to $2000\ \mu\text{s}$.
- Any floating-point corruption (`NaN`, $\pm\text{Inf}$) is clamped immediately to $1000\ \mu\text{s}$.

---

## 3. Autopilot Parameter Synchronization (`quiver_spreader.param`)

On the main ArduPilot Cube+ autopilot, the following parameter file must be loaded to ensure compatibility:
```param
SERVO_GPIO_MASK,61168
SPRAY_ENABLE,1
SERVO9_FUNCTION,23
SERVO13_FUNCTION,22
SERVO13_MIN,1000
SERVO13_MAX,1485
SPRAY_SPINNER,1950
RELAY1_FUNCTION,1
RELAY1_PIN,51
```
- **Bitmask Calculation:** Standard `65520` (`0xFFF0`) $\rightarrow$ Clear bit 8 (SERVO9) and bit 12 (SERVO13) $\rightarrow$ Decimal **`61168`** (`0xEEF0`).
- This releases hardware timers for PWM while maintaining SERVO10 (Pin 51 / AUX2) in GPIO mode to drive the CPC1907B solid state relay powering the switched 14S rail J26.
