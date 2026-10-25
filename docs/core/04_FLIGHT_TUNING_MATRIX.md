# 04. Quiver Master Flight Tuning & Calibration Register Matrix
## Production Parameter Envelope: Cube Orange+ & Bavarian Forest Specification

**Document ID:** `04_FLIGHT_TUNING_MATRIX`  
**Classification:** Calibration & Register Specification  
**Platform Reference:** Quiver Heavy Quadcopter (`Arrow-Air/project-quiver`)  
**Avionics Firmware:** ArduCopter 4.6-DEV (Commit `437a330` with RPLidar S2L Native Driver)  

---

### 1. Master Parameter Register Table

The table below defines the canonical production parameter matrix verified across all simulation and hardware validation flights.

| Parameter Register | Validated Setting | Default Setting | Units / Scale | Operational Effect & Engineering Justification |
| :--- | :---: | :---: | :---: | :--- |
| `SERIAL5_PROTOCOL` | **11** | -1 | Enum | Configures UART5 as an omnidirectional planar range array. |
| `SERIAL5_BAUD` | **1000** | 57 | kbps | Locks baud rate to **1,000,000 baud** (1 Mbaud) to eliminate FIFO overruns at 32 kHz point rates. |
| `PRX1_TYPE` | **5** | 0 | Enum | Activates high-speed native Slamtec driver (`Model: 0x71`) over raw descriptor packets (`A5 5A 54 00 00 40 85`). |
| `PRX1_MIN` | **0.20** | 0.20 | m | Clamps minimum detection threshold above sensor physical casing blind zone. |
| `PRX1_MAX` | **18.00** | 25.00 | m | Reflectivity cutoff at true physical ceiling of RPLidar S2L (prevents phantom point calculation). |
| `AVOID_ENABLE` | **7** | 0 | Bitmask | Enables obstacle avoidance across all axes (horizontal + vertical + fence). |
| `AVOID_MARGIN` | **2.00** | 2.00 | m | Defines the inner physical vehicle stopping buffer. |
| `OA_TYPE` | **1** | 0 | Enum | Engages the **BendyRuler** spatial pathfinder. |
| `OA_BR_LOOKAHEAD` | **12.00** | 15.00 | m | **Critical Normalization:** Normalized at **12.0m** across all specs to eliminate yaw hunting in 10m narrow corridors. |
| `OA_MARGIN_MAX` | **4.00** | 2.00 m | m | **Bavarian Standard:** Enforces a 4.0-meter clearance envelope in dense European forestry corridors. |
| `WPNAV_SPEED` | **800.0** | 500.0 | cm/s | Sets autonomous cruising speed to $8.0\text{ m/s}$ ($28.8\text{ km/h}$). |
| `WPNAV_ACCEL` | **250.0** | 250.0 | cm/s² | Clamps pitch acceleration to prevent abrupt $>25^\circ$ forward body occlusions. |
| `ANGLE_MAX` | **3000** | 3000 | cdeg | Authorizes up to $30.0^\circ$ pitch/roll authority for wind storm penetration. |
| `FS_EKF_ACTION` | **1** | 1 | Enum | Triggers controlled Land mode on fatal EKF divergence. |

---

### 2. Cross-Verification Rules & Consistency Constraints

To guarantee zero discrepancy across aerospace deliverables:
1. **Baud Rate:** `SERIAL5_BAUD` must read strictly `1000` (1 Mbaud). Any reference to 57600 or 115200 baud on Serial 5 is prohibited.
2. **Pathfinder Lookahead:** `OA_BR_LOOKAHEAD` must read strictly `12.0` meters across documents `01`, `02`, `03`, `04`, and `05`.
3. **Safety Clearance Buffer:** `OA_MARGIN_MAX` must read strictly `4.0` meters.
4. **Jettison Mass:** The payload step decrease must be recorded as strictly `4.0` kg ($4.5\text{ kg} \to 0.5\text{ kg}$).
