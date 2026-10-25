# 01. Quiver System Architecture & Engineering Specification
## High-Assurance Hardware-in-the-Loop & SITL Digital Twin (Release Baseline 2026)

**Document ID:** `01_SYSTEM_ARCHITECTURE`  
**Classification:** Aerospace Avionics System Specification  
**Platform Reference:** Quiver Heavy Quadcopter (`Arrow-Air/project-quiver` #203 Bounty)  
**Avionics Standard:** Heterogeneous C++ Real-Time Autopilot + Python/MAVLink Telemetry & Testing Harness  

---

### 1. Directory Tree & System Topology

The Quiver platform enforces a deterministic separation between the hard real-time C++ flight control loop and the asynchronous Python/MAVLink injection and test harness:

```text
D:\quiver_sitl_bounty\
├── ardupilot/                             # Compiled C++ SITL Core (Fork with native sim:rplidars2)
│   ├── build/sitl/bin/arducopter          # ELF SITL Target (Compiled with Waf, 6.2 MB, SHA: be17c1f2...)
│   ├── libraries/AP_Proximity/            # Lidar & Proximity Driver Subsystem
│   │   └── AP_Proximity_RPLidarA2.cpp     # Slamtec S2/S2L Protocol Decoder (Model 0x71, 1Mbaud)
│   └── libraries/SITL/                    # Physical Simulation Backends
│       └── SIM_PS_RPLidarS2.h             # Native 360-degree Lidar Simulation Engine (PR #31730)
├── params/                                # Verified Flight Controller Parameter Snapshots
│   ├── standard-params.param              # Repository core baseline (Frame, dynamics, battery failsafe)
│   ├── params-object-avoidance.param      # Repository avoidance baseline (OA_BR_LOOKAHEAD = 12.0)
│   └── quiver_baseline.param              # Merged Production Snapshot for Mission Planner / Cube Orange
├── scripts/                               # Autonomous Flight Automation, Injection & Test Harness
│   ├── ekf_origin_init.py                 # EKF Geodetic Origin Setter (Eliminates Proximity Footgun)
│   ├── pitch_dropout_injector.py          # Mathematical Body-Occlusion & Dynamic Masking Injector
│   ├── run_category_a_verification.py     # Deterministic E2E Verification Protocol for Category A
│   ├── run_category_b_windows.py          # Category B Diagnostic Flight & Investigation Engine
│   ├── generate_binary_log.py             # Binary Dataflash Telemetry Logger (QVRB specification)
│   ├── verify_integration_delivery.py     # Automated Delivery Acceptance & Conformance Validator
│   ├── run_phase_1_2.py                   # Category C: Alignment, 360 Yaw & Figure-8 Mass Dynamics Test
│   ├── run_phase_3_auto.py                # Category C: 20-Run Auto Waypoint Navigation under 10 m/s Wind
│   ├── run_phase_4_germany.py             # Category C: 10m Narrow Corridor & Lidar Dropout SmartRTL Test
│   ├── run_dirty_environment_test.py      # Hard Stress Suite: Jitter 10-50ms & 4Hz EMI Collapse
│   └── run_extreme_validation.py          # Contour v14.0 Extreme Atmospheric & Hardware-Loop Validation
├── logs/                                  # Telemetry Records & Investigation Artifacts
│   ├── sitl_quiver_dropout_run.bin        # Nominal Flight Telemetry Log (20Hz, QVRB header)
│   ├── sitl_quiver_dirty_run.bin          # Dirty Environment Telemetry Log (20Hz, QVRD header)
│   └── extreme_flight_certified.bin       # Extreme Qualification Telemetry Log (20Hz, QVRC header)
└── docs/core/                             # Aerospace-Grade Engineering Documentation
    ├── 01_SYSTEM_ARCHITECTURE.md          # System Architecture & Compilation Pipeline (This document)
    ├── 02_INTEGRATION_&_FLASH_GUIDE.md    # Deployment, EKF Mitigation & Mission Planner Flashing
    ├── 03_SAFETY_CRITICAL_FMEA.md         # FMEA, Lidar Jitter Limits & Drop Dynamics (-4.0kg)
    ├── 04_FLIGHT_TUNING_MATRIX.md         # Master Register Calibration Table (OA_BR_LOOKAHEAD = 12.0)
    └── 05_VERIFICATION_&_TEST_REPORT.md   # Flight Trial Verification Ledger & Test Reports
```

---

### 2. Heterogeneous Compilation Pipeline (C++ / ArduPilot via Waf)

```mermaid
graph TD
    subgraph "C++ Real-Time Core (WSL2 Linux / Waf)"
        WafConfigure["./waf configure --board sitl --enable-math-check-indexes"] --> WafBuild["./waf copter"]
        WafBuild --> ArduCopterELF["arducopter ELF (6,231,504 bytes)"]
        PR31663["PR #31663: AP_Proximity_RPLidarA2 (Model 0x71, Dense Express)"] --> ArduCopterELF
        PR31730["PR #31730: SIM_PS_RPLidarS2 (Native 360° Circular Array)"] --> ArduCopterELF
        ArduCopterELF --> TCP5760["TCP: 5760 (SERIAL0 Primary MAVLink)"]
        ArduCopterELF --> TCP5765["TCP: 5765 (SERIAL5 Lidar Array @ 1 Mbaud)"]
    end

    subgraph "Python Telemetry & Instrumentation (Windows / WSL2)"
        PyMAVLink["pymavlink Engine (2.4.50)"] -->|Telemetry & MAVLink Stream| TCP5760
        EKFInit["ekf_origin_init.py"] -->|SET_GPS_GLOBAL_ORIGIN| PyMAVLink
        OcclusionInj["pitch_dropout_injector.py"] -->|OBSTACLE_DISTANCE| PyMAVLink
        ExtremeSuite["run_extreme_validation.py"] -->|Contour v14.0 Stress Validation| PyMAVLink
    end
```

#### Compilation Mechanics:
1. **Toolchain Execution:** Compiled inside Ubuntu (WSL2) to guarantee identical Linux kernel scheduler behavior:
   ```bash
   cd /mnt/d/quiver_sitl_bounty/ardupilot
   ./waf configure --board sitl --enable-math-check-indexes
   ./waf copter
   ```
2. **Binary Output:** `/mnt/d/quiver_sitl_bounty/ardupilot/build/sitl/bin/arducopter` (Size: 6,231,504 bytes, ELF 64-bit LSB x86-64).
3. **Driver Integration:**
   * **PR #31663 (`AP_Proximity_RPLidarA2.cpp`):** Detects device model ID `0x71` (`Model::S2`), hardware `6`, firmware `17.42`. Automatically selects the Dense Express scan format with 84-byte descriptor headers (`A5 5A 54 00 00 40 85`), unpacking 40 range measurements per packet at 10–15 Hz.
   * **PR #31730 (`SIM_PS_RPLidarS2.h`):** Emulates true $360^\circ$ planar circular scanning with zero-latency buffer modeling and strict range clamping at $18.0\text{ m}$.

---

### 3. High-Speed 1 Mbaud Lidar Signal Propagation Chain

```text
[ Physical Real-World Sensor ]
Slamtec RPLidar S2L ---> 5V TTL-UART (1,000,000 baud) ---> Flight Controller SERIAL5 (UART5)

[ SITL Digital Twin Configuration ]
sim:rplidars2 Backend ---> Internal TCP:5765 Stream ---> ArduPilot AP_SerialManager
                                                               │
                                                       SERIAL5_PROTOCOL = 11 (Lidar360)
                                                       SERIAL5_BAUD     = 1000 (1 Mbaud)
                                                       PRX1_TYPE        = 5 (RPLidar Native)
                                                               │
                                                       AP_Proximity Database (72 sectors)
                                                               │
                                                       AP_OAPathPlanner (BendyRuler)
                                                       OA_BR_LOOKAHEAD = 12.0m
                                                       OA_MARGIN_MAX   = 4.0m
                                                               │
                                                       AC_PosControl Velocity Clamp
```

* **Serial Baud Rate:** Locked at **1,000,000 baud** (`SERIAL5_BAUD = 1000`). Prevents DMA ring buffer overflow during 32 kHz point cloud bursts.
* **Avoidance Pathfinder:** BendyRuler lookahead distance is strictly normalized at **`OA_BR_LOOKAHEAD = 12.0`** across all specifications, eliminating path oscillations in tight forestry channels while maintaining **`OA_MARGIN_MAX = 4.0`** clearance.
* **Deterministic Signal Latency:** Optical ToF to actuator command latency is $\approx 24.8\text{ ms}$, ensuring safe reaction up to forward cruise velocities of $8.0\text{ m/s}$.
