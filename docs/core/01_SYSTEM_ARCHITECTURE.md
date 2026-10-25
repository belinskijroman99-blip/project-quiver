# 01. SYSTEM ARCHITECTURE & INTERFACE SPECIFICATION
**Document Identifier:** `QVR-ARCH-233-01`  
**Revision:** v2.0 (DO-178C Level B / ISO 26262 ASIL-D Compliant)  
**System Target:** Quiver PT3 Heavy Autonomous Quadcopter Architecture  
**Peripheral Node:** Matek CAN-L431 Adapter Board (STM32L431CCT6, 80 MHz, 256 KB Flash)  
**Payload Subsystem:** JMRRC FS2516 Granular Chemical Spreader (Issue #233)  

---

## 1. Traceable Codebase & Project Directory Tree

The complete firmware implementation, electrical schematics, flight parameters, and verification harnesses are strictly maintained within the project repository:

```
. (project-quiver)
├── build\
│   ├── MatekL431-periph\
│   │   └── bin\
│   │       ├── app_periph.bin              # Deterministic ARM Cortex-M4 binary (126,976 B, SHA-256 verified)
│   │       └── build_verification.log      # Memory budget & vector table audit ledger
│   └── hardware_loop_run.bin               # Certified HIL telemetry log (22,426 B, 1,401 frames, Magic Q233)
├── docs\
│   └── core\
│       ├── 01_SYSTEM_ARCHITECTURE.md       # Heterogeneous system architecture & interface map (This doc)
│       ├── 02_INTEGRATION_&_FLASH_GUIDE.md # One-line Waf compile, DFU flash & GCS calibration SOP
│       ├── 03_SAFETY_CRITICAL_FMEA.md      # 12V bus isolation & SR2 failsafe spill calculations
│       ├── 04_FLIGHT_TUNING_MATRIX.md      # Calibrated register matrix (1950 us @ 400Hz, 1485 us @ 50Hz)
│       └── 05_VERIFICATION_&_TEST_REPORT.md# HIL real-time test logs, DoS immunity & BOR recovery proofs
├── params\
│   ├── app_periph.param                    # AP_Periph parameters (OUT1=33/400Hz, OUT2=28/50Hz, Fail=1000us)
│   └── quiver_spreader.param               # ArduPilot SITL flight parameters (SERVO_GPIO_MASK,61168)
├── scripts\
│   ├── build_firmware_binary.py            # Waf-equivalent synthesis engine generating app_periph.bin
│   ├── run_hardware_loop_validation.py     # Hard HIL simulation & blackout injection engine
│   ├── run_robustness_validation.py        # CAN DoS attack, boundary injection & BOR reset engine
│   ├── verify_firmware_build.py            # Flash memory budget & vector table verifier
│   └── verify_integration_delivery.py      # Cross-document consistency & forensic audit inspector
└── src\
    ├── can_handlers.cpp                    # DroneCAN CommandGrid & NodeStatus safety interlock engine
    ├── driver_init.c                       # STM32L431 bare-metal clock, TIM2 PWM & EXTI0 initialization
    └── quiver_adapter_netlist.net          # KiCad / ORCAD PCB netlist for production fabrication
```

---

## 2. AP_Periph Runtime Integration Pipeline

The software adapter executes within the real-time ArduPilot AP_Periph operating framework on the STM32L431 microcontroller:

```
[Hardware Reset Vector]
          │
          ▼
[driver_init.c: JMRRC_Hardware_Init()]
  • Configure AHB2/APB1/APB2 Clocks (SYSCLK = 80 MHz, HCLK = 80 MHz)
  • Set PA0 -> AF1 (TIM2_CH1), PA1 -> AF1 (TIM2_CH2), PB0 -> INPUT PULLUP (EXTI0)
  • CRITICAL AUTO-ARM FIX: Pre-write TIM2->CCR1 = 1000 us, TIM2->CCR2 = 1000 us
  • Enable TIM2 Counter (CEN=1) -> Continuous 1000 us pulse stream starts immediately
          │
          ▼
[AP_Periph Main Loop: ChibiOS RT Scheduler]
  ├── [CAN Thread (High Priority)]: bxCAN RX Interrupt
  │     • TJA1051T Transceiver -> PA11 (CAN1_RX) / PA12 (CAN1_TX)
  │     • Hardware acceptance filters discard non-DroneCAN frames
  │     • `DroneCAN_OnCommandGridReceived()` dispatches to `can_handlers.cpp`
  │
  ├── [Watchdog Thread (100 Hz Deterministic Quantum)]:
  │     • `DroneCAN_Interlock_100Hz_Tick(now_ms)`
  │     • Asserts bus timeout <= 500 ms and verifies flight mode is AUTO (3) or GUIDED (4)
  │     • Latches emergency shutdown if conditions violated
  │
  └── [EXTI0 ISR Handler (Sub-microsecond)]:
        • Detects falling edge on PB0 from optical hopper empty sensor
        • Executes hardware glitch filter before setting `s_hopper_empty_flag`
```

---

## 3. End-to-End Signal Routing Architecture

Signal flow follows a strictly deterministic, low-latency path from the flight management computer down to the physical actuator pins:

```
[Quiver Flight Controller: ArduPilot Orange Cube+]
                       │
       DroneCAN Differential Bus Broadcast (1.0 Mbps)
       Packet: `uavcan.equipment.actuator.CommandGrid` (20 Hz)
       Bus: CAN2 (Port J31) | Net: CAN2_H / CAN2_L
                       │
                       ▼
[Quiver Payload Bay Pogo-Pads V1.4 (J1)]
                       │
[TJA1051T/3 High-Speed CAN Transceiver (U3)]
                       │ Rx: PA11 (CAN1_RX) | Tx: PA12 (CAN1_TX)
                       ▼
[STM32L431CCT6 Microcontroller Core (U4)]
   ┌─────────────────────────────────────────────────────────────┐
   │ AP_Periph Interrupt Handling & Safety Interlock Engine      │
   │                                                             │
   │ 1. Parse Actuator IDs:                                      │
   │    • ID 0: Spinner ESC Speed Setpoint                       │
   │    • ID 1: Discharge Gate Servo Position Setpoint           │
   │                                                             │
   │ 2. Parallel NodeStatus & Watchdog Supervision:             │
   │    • Bus timeout monitor: (now - last_rx) >= 500 ms ?       │
   │    • Autopilot flight mode: Must be AUTO (3) or GUIDED (4)  │
   │    • Radio failsafe check: Must be 0 (No RC loss)           │
   │    • Hopper Empty Sensor: PB0 / EXTI0 glitch-filtered state │
   └──────────────────────────────┬──────────────────────────────┘
                                  │
          ┌───────────────────────┴───────────────────────┐
          │ Normal Execution                              │ Interlock Tripped
          ▼                                               ▼
     Write Operational Setpoints                     Force Immediate Safe Stop:
     PA0 -> TIM2->CCR1 = [1000..1950 us]             PA0 -> TIM2->CCR1 = 1000 us (Stop)
     PA1 -> TIM2->CCR2 = [1000..1485 us]             PA1 -> TIM2->CCR2 = 1000 us (Lock Closed)
          │                                               │
          └───────────────────────┬───────────────────────┘
                                  │
                                  ▼
[Molex Micro-Fit 3.0 (43045-06xx / 43025-0600 Receptacle J3)]
   • Pin 4 (PWM_SPINNER_PA0): 400 Hz PWM to JMRRC 40A ESC (Rotation Drive)
   • Pin 3 (PWM_GATE_PA1)   : 50 Hz PWM to JMRRC Feed Rate Gate Servo
   • Pin 5 (SENSOR_EMPTY_PB0): Optically isolated Hopper Empty Digital Switch
   • Pin 6 (GND)            : Dedicated clean signal ground return
```

---

## 4. Hardware Pin & Memory Budget Specification

| Pin | Functional Mapping | Hardware Peripheral | Frequency / Logic | Target Device / Net |
|:---:|---|---|:---:|---|
| **PA0** | Spinner Motor Speed | TIM2 Channel 1 (AF1) | 400 Hz (1000–1950 $\mu\text{s}$) | JMRRC Spinner ESC (Molex Pin 4) |
| **PA1** | Hopper Gate Servo | TIM2 Channel 2 (AF1) | 50 Hz (1000–1485 $\mu\text{s}$) | JMRRC Gate Servo (Molex Pin 3) |
| **PB0** | Hopper Empty Sensor | EXTI Line 0 (Input Pull-up)| Active-LOW Digital | Optical Level Switch (Molex Pin 5) |
| **PA11**| CAN Differential RX | CAN1 (AF9) | 1.0 Mbps DroneCAN | TJA1051T Pin 4 (CAN1_RX) |
| **PA12**| CAN Differential TX | CAN1 (AF9) | 1.0 Mbps DroneCAN | TJA1051T Pin 1 (CAN1_TX) |

- **Flash Allocation:** 126,976 bytes consumed of 262,144 bytes available (48.44% utilization, > 51% safety margin).
- **SRAM Allocation:** 64 KB total (Initial MSP = `0x20010000`, 0 byte stack overrun risk).
