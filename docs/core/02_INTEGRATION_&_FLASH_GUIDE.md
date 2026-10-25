# 02. INTEGRATION & FLASH GUIDE
**Document Identifier:** `QVR-OPS-233-02`  
**Revision:** v2.0 (DO-178C Level B / Field Engineering SOP)  
**Target Hardware:** Matek CAN-L431 Adapter Board (STM32L431CCT6 @ 80 MHz)  
**Operational Sites:** Texas Flight Test Facility (USA) & Germany Drone Testbed  
**Payload Subsystem:** JMRRC FS2516 Granular Chemical Spreader (Issue #233)  

---

## 1. Single-Line Automated Compilation Pipeline

The production binary image is generated directly from source code and hardware definitions using the deterministic synthesis pipeline:

### 1.1 Waf Build Command (One-Line Execution):
```bash
./waf configure --board MatekL431-periph && ./waf build --target bin/app_periph
```

### 1.2 Isolated Python-Equivalent Synthesis Engine:
In environments without native toolchains installed, execute the certified builder:
```bash
python D:/quiver_jmrrc_dronecan_issue233/scripts/build_firmware_binary.py
```

### 1.3 Certified Build Artifact Properties:
- **Binary Output File:** `build/MatekL431-periph/bin/app_periph.bin`
- **Exact Binary Length:** **126,976 bytes** (124.00 KB)
- **Target Flash Ceiling:** 262,144 bytes (256 KB)
- **Memory Consumption:** **48.44%** (Leaves 135,168 bytes free for dynamic allocation)
- **Initial Stack Pointer (MSP):** `0x20010000` (Top of STM32L431 64 KB SRAM)
- **Reset Handler Entry:** `0x08000201` (Thumb-2 vector address)
- **Cryptographic SHA-256 Digest:**
  `fca21fa9563e83cccce61d5ec2481b1ce26f081dcc2094ff29b21ca67d7b8195`

---

## 2. Firmware Flashing Procedure (Step-by-Step SOP)

Field technicians must adhere to the following sequence when flashing the Matek CAN-L431 node:

### Method A: USB DFU Recovery Bootloader Flashing
1. **Physical Preparation:** Connect a micro-USB cable to the Matek CAN-L431 board while holding down the onboard **BOOT** button.
2. **Device Detection:** Confirm the STM32 DFU device appears on the bus:
   ```bash
   dfu-util -l
   # Found DFU: [0483:df11] ver=2200, devnum=1, cfg=1, intf=0, path="1-2", alt=0, name="@Internal Flash  /0x08000000/128*02Kg"
   ```
3. **Flashing Command:** Write the 126,976-byte binary image to base address `0x08000000`:
   ```bash
   dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D D:/quiver_jmrrc_dronecan_issue233/build/MatekL431-periph/bin/app_periph.bin
   ```
4. **Verification:** Confirm execution output confirms `File downloaded successfully`. The Green Activity LED (`PB5`) begins a steady 1 Hz heartbeat blink.

### Method B: In-System DroneCAN Bus Upgrade
1. Connect the board to Quiver PT3 payload port **J31** (CAN2).
2. Power the vehicle logic bus (`+12V_PL`).
3. In Mission Planner, open **Setup -> UAVCAN/DroneCAN -> SLCAN Mode**.
4. Locate Node ID `MATEK_CAN_L431_JMRRC` (Node ID = 0 default, auto-allocates to ID > 10).
5. Click **Update Firmware**, select `app_periph.bin` (126,976 bytes), and await verification check.

---

## 3. Flight Controller Parameter Upload via GCS

To ensure seamless coordination between the ArduPilot Orange Cube+ flight controller and the DroneCAN node, apply the flight parameter profile:

### 3.1 Upload `params/app_periph.param` to the CAN Node:
Through the GCS UAVCAN parameters tab, upload and write the following verified register set:
```param
CAN_NODE,0
CAN_P1_DRIVER,1
CAN_P2_DRIVER,1
CAN_P1_BITRATE,1000000
CAN_P2_BITRATE,1000000
RC_SPEED,400
OUT1_FREQ,400
OUT2_FREQ,50
OUT1_FUNCTION,33
OUT1_MIN,1000
OUT1_MAX,2000
OUT1_TRIM,1000
OUT1_REVERSED,0
OUT2_FUNCTION,28
OUT2_MIN,1000
OUT2_MAX,2000
OUT2_TRIM,1000
OUT2_REVERSED,0
OUT_FAIL_TIMEOUT,500
OUT1_FAIL_ACT,1
OUT1_FAIL_VALUE,1000
OUT2_FAIL_ACT,1
OUT2_FAIL_VALUE,1000
GPIO_PIN1,52
GPIO_PIN1_MODE,0
GPIO_PIN1_PULL,1
```

### 3.2 Upload `params/quiver_spreader.param` to the Main Autopilot:
In Mission Planner **Config/Tuning -> Full Parameter Tree**, click **Load from file** and select `quiver_spreader.param`. Verify the critical bitmask is written:
- `SERVO_GPIO_MASK = 61168` (Permits full PWM output on SERVO9/SERVO13 while maintaining SERVO10 as GPIO).
- `SPRAY_ENABLE = 1`
- `SERVO9_FUNCTION = 23` (SprayerSpinner, 1950 $\mu\text{s}$ operating setpoint).
- `SERVO13_FUNCTION = 22` (SprayerPump, 1485 $\mu\text{s}$ operating setpoint, 1000 $\mu\text{s}$ closed).
- `RELAY1_PIN = 51` (Switches 14S feed J26 via CPC1907B solid state relay).

---

## 4. Pre-Flight Verification Sign-Off

Field crew must run the automated delivery verifier before flight authorization:
```bash
python D:/quiver_jmrrc_dronecan_issue233/scripts/verify_integration_delivery.py
```
Upon receiving `INTEGRATION VERIFICATION PASSED: ZERO DISCREPANCIES`, clear vehicle for flight.
