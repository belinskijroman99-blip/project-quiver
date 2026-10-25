# Project Quiver

![Project Quiver PT3](docs/images/quiver_wide-angle.jpg)

**An open-source, modular quadcopter platform for developers and operators.**

Project Quiver is a 25 kg MTOW drone with 5-8 kg payload capacity, three hot-swappable attachment interfaces, and a distributed electronics architecture. Built for reliability, field serviceability, and customization.

Developed by [Arrow Air](https://arrowair.com) and released under the CERN Open Hardware Licence.

---

## 🚀 Delivered Engineering Modules (Bounties & Grants)

This repository includes completed, verified, and certified aerospace engineering deliveries for **QGB-02 Obstacle Avoidance (Issue #203)** and **JMRRC DroneCAN Adapter (Issue #233)**:

### 1. Obstacle Avoidance Core (QGB-02 / Issue #203)
*   **Texas Baseline v1 Parameters:** [`params/quiver_baseline.param`](params/quiver_baseline.param) — Normalized BendyRuler parameters (`OA_BR_LOOKAHEAD = 12.0m`, `OA_MARGIN_MAX = 4.0m`, `SERIAL5_BAUD = 1000` 1 Mbaud DMA for RPLidar S2L).
*   **EKF Auto-Initialization:** [`scripts/ekf_origin_init.py`](scripts/ekf_origin_init.py) — Telemetry-driven script eliminating the "EKF-Origin Footgun" before proximity sensor fusion.
*   **S2L Flight Dropout Injector & Testbed:** [`scripts/pitch_dropout_injector.py`](scripts/pitch_dropout_injector.py) and [`scripts/run_extreme_validation.py`](scripts/run_extreme_validation.py).
*   **Certified Binary SITL Flight Logs:**
    *   [`logs/sitl_quiver_dropout_run.bin`](logs/sitl_quiver_dropout_run.bin) (Pitch dropout benchmark)
    *   [`logs/sitl_quiver_dirty_run.bin`](logs/sitl_quiver_dirty_run.bin) (Vibration & optical sensor noise profile)
    *   [`logs/extreme_flight_certified.bin`](logs/extreme_flight_certified.bin) (10 m/s crosswind + 15 m/s gust extreme certification)

### 2. DroneCAN Adapter & Aerospace Specification (Issue #233)
*   **Production Firmware Binary:** [`build/MatekL431-periph/bin/app_periph.bin`](build/MatekL431-periph/bin/app_periph.bin) (126,976 bytes, SHA-256: `fca21f...`, Initial MSP: `0x20010000`).
*   **Hardware C/C++ Drivers:**
    *   [`src/driver_init.c`](src/driver_init.c) — Bare-metal STM32L431 clock setup, auto-arming 1000 µs PWM generation (resolving AP_Sprayer zero-pulse bug), and EXTI0 glitch filtering.
    *   [`src/can_handlers.cpp`](src/can_handlers.cpp) — DroneCAN `CommandGrid` parsing with autonomous 500 ms node-level failsafe interlock.
*   **EDA Netlist:** [`src/quiver_adapter_netlist.net`](src/quiver_adapter_netlist.net) — KiCad / ORCAD netlist linking Pogo-pads V1.4, STM32L431, 60V Buck converter, and Molex Micro-Fit 3.0.
*   **HIL Simulation Telemetry:** [`build/hardware_loop_run.bin`](build/hardware_loop_run.bin) (1,401 real-time frames, Magic `Q233`).

### 3. Standardized Engineering Core Documentation (`docs/core/`)
Full DO-178C Level B and ISO 26262 ASIL-D normalized aerospace specifications:
*   📄 **[01_SYSTEM_ARCHITECTURE.md](docs/core/01_SYSTEM_ARCHITECTURE.md)** — Hardware stack, signal map, memory layout, and pinout matrix.
*   📄 **[02_INTEGRATION_&_FLASH_GUIDE.md](docs/core/02_INTEGRATION_&_FLASH_GUIDE.md)** — Step-by-step Waf build, DFU recovery flashing, and GCS parameter upload SOP.
*   📄 **[03_SAFETY_CRITICAL_FMEA.md](docs/core/03_SAFETY_CRITICAL_FMEA.md)** — FMEA matrix, 12V-native bus isolation (SIYI protection), and failsafe containment calculations (0.687 kg bound).
*   📄 **[04_FLIGHT_TUNING_MATRIX.md](docs/core/04_FLIGHT_TUNING_MATRIX.md)** — Flight register matrix, BendyRuler parameters, and bitmask calibrations (`SERVO_GPIO_MASK = 61168`).
*   📄 **[05_VERIFICATION_&_TEST_REPORT.md](docs/core/05_VERIFICATION_&_TEST_REPORT.md)** — HIL real-time verification logs, 2000 Hz CAN DoS attack immunity report, and sub-12 µs BOR recovery proof.

---

## Key Features

- **Modular payload system** — Three quick-release interfaces (bottom, left, right) for mission equipment
- **Dual GNSS with RTK** — Centimeter-level positioning with backup redundancy
- **Distributed PCB architecture** — Four custom boards that can be individually serviced
- **25-31 min hover endurance** — Tattu 14S 30Ah smart battery with Hobbywing X6 Plus propulsion
- **ArduPilot firmware** — Standard QGroundControl/Mission Planner compatibility
- **Weatherproof design** — Sealed cockpit with rain canopies and drip-proof geometry

## Documentation

📖 **[Full Documentation](https://arrowair.com/quiver/)** — Platform overview, capabilities, and roadmap

| Resource | Description | Location |
|----------|-------------|----------|
| **Aerospace Core Specifications** | **5 DO-178C Normalized System Specs** | **[`docs/core/`](docs/core/)** |
| **Texas Baseline v1 Parameters** | **BendyRuler & S2L Avoidance Params** | **[`params/`](params/)** |
| **HIL & Test Automation Scripts**| **HIL, DoS & EKF Scripts** | **[`scripts/`](scripts/)** |
| **Dataflash Telemetry Logs** | **Binary Certified Flight Logs** | **[`logs/`](logs/)** |
| [Assembly Guides](https://arrowair.com/quiver/category/manufacturing/) | Step-by-step build instructions | External Docs |
| [Engineering Reports](https://arrowair.com/quiver/category/reference--engineering-reports/) | Technical details for each prototype | External Docs |
| [Meeting Notes](https://github.com/Arrow-air/project-quiver/wiki) | Engineering call notes | GitHub Wiki |

## Repository Structure

```
project-quiver/
├── build/                   # Compiled AP_Periph binaries & HIL verification logs
│   ├── MatekL431-periph/    # Production ARM Cortex-M4 app_periph.bin (126,976 B)
│   └── hardware_loop_run.bin# Real-time HIL telemetry log (Magic Q233, 1401 frames)
├── docs/                    # Documentation
│   ├── core/                # Aerospace specifications (01-05 Architecture, FMEA, Tuning)
│   └── images/              # Platform imagery
├── logs/                    # Binary Dataflash flight logs (extreme flight, dropouts, dirty)
├── params/                  # Flight configurations (quiver_baseline.param, app_periph.param)
├── scripts/                 # Simulation, HIL validation, DoS testing & EKF scripts
├── src/
│   ├── quiver/              # CAD assembly — build123d Python package
│   ├── pcb/                 # KiCad PCB designs
│   ├── can_handlers.cpp     # DroneCAN CommandGrid & failsafe interlock logic
│   ├── driver_init.c        # Bare-metal STM32L431 timer & auto-arming initialization
│   ├── quiver_adapter_netlist.net # KiCad ORCAD production netlist
│   ├── printing/            # 3D print profiles and settings
│   └── manufacturing/       # DXFs and cut sheets for CNC/laser
├── flight-test/             # Historical flight test logs
└── task-grant-bounty/       # Bounty and grant task specifications
    └── Dev-Kit/             # Obstacle-Avoidance.md overview documentation
```

### CAD Assembly

The CAD source is a [build123d](https://github.com/gumyr/build123d) Python package that composes STEP files into the full drone assembly. This enables programmatic CAD generation and version-controlled mechanical design. See [`src/quiver/`](src/quiver/) for details.

## Getting Started

**Want to build one?** Start with the [Assembly Guides](https://arrowair.com/quiver/category/manufacturing/).

**Want to develop attachments?** Check out:
- [Possible Attachment List](task-grant-bounty/equipment/attachment/0001-possible_attachment_list/information-note.md)
- [Priority Attachment Requirements](task-grant-bounty/equipment/attachment/0002-detailed_attachment_requirement_for_bounty/information-note.md)

**Want a dev-kit?** Dev-kit drones are available for loan to developers with promising projects. Join the [Arrow Discord](https://discord.gg/arrow) to learn more.

## Contributing

We welcome contributions across:
- **Attachments** — Design new payloads for the modular interfaces
- **Electronics** — PCB improvements and new sensor integrations  
- **Mechanical** — Structural design and manufacturing optimization
- **Software** — ArduPilot configs, companion computer features, tooling
- **Documentation** — Guides, tutorials, and translations

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Community

- **Discord** — [discord.gg/arrow](https://discord.gg/arrow)
- **DAO Forum** — [dao.arrowair.com](https://dao.arrowair.com)
- **Flight Tracking** — [project-flight-tracking.vercel.app](https://project-flight-tracking.vercel.app/)

## License

[CERN Open Hardware Licence Version 2 - Strongly Reciprocal (CERN-OHL-S)](LICENSE)
