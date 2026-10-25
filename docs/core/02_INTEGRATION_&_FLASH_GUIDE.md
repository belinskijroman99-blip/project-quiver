# 02. Quiver Deployment, EKF Mitigation & Flashing Guide
## Hardware Integration & Pilot Manual for Texas & Germany Operations

**Document ID:** `02_INTEGRATION_&_FLASH_GUIDE`  
**Classification:** Operational Deployment Guide  
**Platform Reference:** Quiver Heavy Quadcopter (`Arrow-Air/project-quiver`)  
**Target Hardware:** Cube Orange+ / Pixhawk 6X / SITL WSL2  

---

### 1. One-Line SITL Launch Command (WSL2 on D:)

The entire simulation environment can be reproduced on any engineering workstation running WSL2 with Ubuntu 24.04 LTS:

```bash
wsl -d Ubuntu -e bash -c "cd /mnt/d/quiver_sitl_bounty/ardupilot && ./build/sitl/bin/arducopter --model quad --defaults /mnt/d/quiver_sitl_bounty/params/quiver_baseline.param"
```

#### Step-by-Step Execution:
1. Open PowerShell terminal.
2. Verify communication and run the automated delivery acceptance validator:
   ```powershell
   python D:\quiver_sitl_bounty\scripts\verify_integration_delivery.py
   ```
3. Run the complete extreme flight validation suite:
   ```powershell
   python D:\quiver_sitl_bounty\scripts\run_extreme_validation.py
   ```

---

### 2. The "EKF-Origin Footgun" & Automated Mitigation

> [!CAUTION]
> **Critical Architectural Footgun in ArduPilot Proximity Stack:**  
> In ArduPilot, the `AP_Proximity` database **refuses to transition to an active state** until the Extended Kalman Filter (EKF) has locked its global geodetic origin. If takeoff or mission commands are issued before `SET_GPS_GLOBAL_ORIGIN` is dispatched, the proximity subsystem remains in `NO_DATA` mode, completely disabling BendyRuler obstacle avoidance while the vehicle is in flight!

#### Root Cause:
`AP_OAPathPlanner` operates in local NED Cartesian coordinates converted from global geodetic coordinates. Without an EKF origin, transformations fail silently, vectors are discarded, and the vehicle enters flight blind to 360° lidar returns.

#### Automated Solution via `ekf_origin_init.py`:
The initialization asset [`scripts/ekf_origin_init.py`](file:///D:/quiver_sitl_bounty/scripts/ekf_origin_init.py) intercepts the MAVLink telemetry stream prior to motor arming:
1. Dispatches MAVLink message `SET_GPS_GLOBAL_ORIGIN` with site coordinates (Austin, TX: `30.2672° N, -97.7431° W, 150m MSL`).
2. Issues `MAV_CMD_DO_SET_HOME` to anchor the return-to-launch coordinate origin.
3. Polls telemetry streams until `AP_Proximity` confirms the active transition: `PRX_DATABASE_ACTIVE`.

---

### 3. Step-by-Step Flashing to Real Hardware (Mission Planner)

To deploy the validated parameters to the real Quiver flight controller:

1. Connect the flight controller (Cube Orange / Pixhawk 6X) to your Ground Control Station via USB-C or RF telemetry.
2. Launch **Mission Planner** and click **Connect** (115200 baud for USB, 57600 baud for telemetry).
3. Navigate to **CONFIG/TUNING** $\to$ **Full Parameter Tree** (or **Full Parameter List**).
4. Click the **Load from file** button on the right-hand panel.
5. Select the merged production parameter file:
   ```text
   D:\quiver_sitl_bounty\params\quiver_baseline.param
   ```
6. Review the key safety registers being updated:
   * `SERIAL5_PROTOCOL` $\to$ `11` (Lidar360 mode)
   * `SERIAL5_BAUD` $\to$ `1000` (1,000,000 baud locked clock)
   * `PRX1_TYPE` $\to$ `5` (Native Slamtec driver)
   * `OA_TYPE` $\to$ `1` (BendyRuler)
   * `OA_BR_LOOKAHEAD` $\to$ `12.0` (Normalized 12-meter corridor lookahead)
   * `OA_MARGIN_MAX` $\to$ `4.0` (4-meter obstacle safety buffer)
   * `AVOID_ENABLE` $\to$ `7` (All-axis avoidance enabled)
7. Click **Write Params**.
8. Power-cycle the flight controller to reboot the high-speed UART clock on Serial 5.
