/**
 * @file can_handlers.cpp
 * @brief DroneCAN Message Handlers & Autonomous Node-Level Safety Interlock
 * @details Target: Matek CAN-L431 JMRRC Adapter (Issue #233)
 *          Compliance: DO-178C Level B Safety Interlock (Lesson L5)
 */

#include <stdint.h>
#include <stdbool.h>

/* C linkage to low-level hardware driver */
extern "C" {
    void JMRRC_Hardware_SetPWM(uint8_t channel, uint16_t pulse_width_us);
    bool JMRRC_Hardware_IsHopperEmpty(void);
    void JMRRC_Hardware_ClearHopperEmpty(void);
}

/* UAVCAN / DroneCAN Actuator Index Definitions */
#define ACTUATOR_INDEX_SPINNER      0U
#define ACTUATOR_INDEX_GATE         1U

/* Safety Critical Pulse Constraints (Microseconds) */
#define PWM_PULSE_GATE_CLOSED_US    1000U
#define PWM_PULSE_SPINNER_STOP_US   1000U
#define PWM_PULSE_MIN_US            1000U
#define PWM_PULSE_MAX_US            2000U

/* Communication Timeout Threshold (Lesson L5: > 500ms bus loss) */
#define CAN_BUS_TIMEOUT_MS          500U

/* Autopilot Flight Modes & Failsafe Flags */
#define AP_FLIGHT_MODE_AUTO         3U
#define AP_FLIGHT_MODE_GUIDED       4U
#define AP_STATUS_FLAG_RADIO_FAIL   (1U << 0)
#define AP_STATUS_FLAG_GCS_FAIL     (1U << 1)
#define AP_STATUS_FLAG_BATTERY_FAIL (1U << 2)

/**
 * @brief Actuator Command Grid DroneCAN Data Structure Representation
 * Corresponds to uavcan.equipment.actuator.CommandGrid / ArrayCommand
 */
struct ActuatorCommand {
    uint8_t  actuator_id;
    float    command_value; /* Normalized -1.0 .. +1.0 or 0.0 .. 1.0 (converted to us) */
};

struct CommandGridMessage {
    uint8_t         num_commands;
    ActuatorCommand commands[16];
};

/**
 * @brief Autopilot Node Status / Heartbeat Structure
 * Corresponds to uavcan.protocol.NodeStatus / ardupilot.indication.Heartbeat
 */
struct AutopilotStatusMessage {
    uint32_t uptime_sec;
    uint8_t  health;
    uint8_t  mode;
    uint8_t  sub_mode;          /* Mapped to ArduPilot flight mode (AUTO=3, GUIDED=4) */
    uint16_t vendor_status_code;/* Bits for Radio Failsafe, E-Stop */
};

/**
 * @brief Node-Level Safety Interlock State Machine
 */
class JMRRC_DroneCAN_Interlock {
public:
    enum InterlockState {
        STATE_BOOT_ARMED        = 0,
        STATE_ACTIVE_OPERATION  = 1,
        STATE_FAILSAFE_LATCHED  = 2
    };

    JMRRC_DroneCAN_Interlock()
        : _state(STATE_BOOT_ARMED),
          _last_actuator_msg_ms(0),
          _last_heartbeat_msg_ms(0),
          _current_flight_mode(0),
          _failsafe_flags(0),
          _consecutive_valid_frames(0)
    {}

    /**
     * @brief Handle uavcan.equipment.actuator.CommandGrid Packet
     */
    void handle_actuator_command(const CommandGridMessage& msg, uint32_t current_time_ms)
    {
        _last_actuator_msg_ms = current_time_ms;

        // If in failsafe state, require consecutive valid frames in authorized mode before releasing
        if (_state == STATE_FAILSAFE_LATCHED) {
            if (is_flight_mode_authorized() && !is_failsafe_active()) {
                _consecutive_valid_frames++;
                if (_consecutive_valid_frames >= 5U) {
                    _state = STATE_ACTIVE_OPERATION;
                }
            } else {
                _consecutive_valid_frames = 0;
            }
        }

        // Evaluate safety envelope before applying commands to hardware
        if (_state == STATE_FAILSAFE_LATCHED) {
            enforce_failsafe_outputs();
            return;
        }

        // Normal execution path
        _state = STATE_ACTIVE_OPERATION;

        for (uint8_t i = 0; i < msg.num_commands; ++i) {
            const ActuatorCommand& cmd = msg.commands[i];
            const uint16_t pulse_us = scale_command_to_pulse(cmd.command_value);

            if (cmd.actuator_id == ACTUATOR_INDEX_SPINNER) {
                JMRRC_Hardware_SetPWM(0, pulse_us); // PA0 / TIM2_CH1
            } else if (cmd.actuator_id == ACTUATOR_INDEX_GATE) {
                // If hopper is empty, force gate closed regardless of command
                if (JMRRC_Hardware_IsHopperEmpty()) {
                    JMRRC_Hardware_SetPWM(1, PWM_PULSE_GATE_CLOSED_US);
                } else {
                    JMRRC_Hardware_SetPWM(1, pulse_us); // PA1 / TIM2_CH2
                }
            }
        }
    }

    /**
     * @brief Handle Autopilot Heartbeat / NodeStatus Packet
     */
    void handle_autopilot_status(const AutopilotStatusMessage& msg, uint32_t current_time_ms)
    {
        _last_heartbeat_msg_ms = current_time_ms;
        _current_flight_mode   = msg.sub_mode;
        _failsafe_flags        = msg.vendor_status_code;

        // Immediate check: if vehicle is not in AUTO/GUIDED, or Radio Failsafe engaged
        if (!is_flight_mode_authorized() || is_failsafe_active()) {
            trigger_failsafe(current_time_ms);
        }
    }

    /**
     * @brief High-Rate 100 Hz Deterministic Health & Watchdog Monitor
     */
    void update_100hz(uint32_t current_time_ms)
    {
        const uint32_t elapsed_actuator_ms  = current_time_ms - _last_actuator_msg_ms;
        const uint32_t elapsed_heartbeat_ms = current_time_ms - _last_heartbeat_msg_ms;

        // Condition 1: Bus timeout on Actuator commands (> 500 ms)
        if (elapsed_actuator_ms >= CAN_BUS_TIMEOUT_MS) {
            trigger_failsafe(current_time_ms);
            return;
        }

        // Condition 2: Bus timeout on Autopilot Heartbeat (> 500 ms)
        if (elapsed_heartbeat_ms >= CAN_BUS_TIMEOUT_MS) {
            trigger_failsafe(current_time_ms);
            return;
        }

        // Condition 3: Failsafe flags or flight mode deviation
        if (!is_flight_mode_authorized() || is_failsafe_active()) {
            trigger_failsafe(current_time_ms);
            return;
        }

        // Condition 4: Empty hopper physical event
        if (JMRRC_Hardware_IsHopperEmpty()) {
            JMRRC_Hardware_SetPWM(1, PWM_PULSE_GATE_CLOSED_US);
        }
    }

    InterlockState get_state() const { return _state; }

private:
    InterlockState _state;
    uint32_t       _last_actuator_msg_ms;
    uint32_t       _last_heartbeat_msg_ms;
    uint8_t        _current_flight_mode;
    uint16_t       _failsafe_flags;
    uint8_t        _consecutive_valid_frames;

    bool is_flight_mode_authorized() const
    {
        return (_current_flight_mode == AP_FLIGHT_MODE_AUTO || 
                _current_flight_mode == AP_FLIGHT_MODE_GUIDED);
    }

    bool is_failsafe_active() const
    {
        return (_failsafe_flags & (AP_STATUS_FLAG_RADIO_FAIL | AP_STATUS_FLAG_GCS_FAIL)) != 0;
    }

    void trigger_failsafe(uint32_t current_time_ms)
    {
        _state = STATE_FAILSAFE_LATCHED;
        _consecutive_valid_frames = 0;
        enforce_failsafe_outputs();
    }

    void enforce_failsafe_outputs()
    {
        // AUTONOMOUS NODE-LEVEL INTERLOCK (Lesson L5):
        // Instantly force Gate Servo to 1000 us (Closed). Mechanical return spring activates.
        JMRRC_Hardware_SetPWM(1, PWM_PULSE_GATE_CLOSED_US);
        // Force Spinner motor to 1000 us (Stop).
        JMRRC_Hardware_SetPWM(0, PWM_PULSE_SPINNER_STOP_US);
    }

    uint16_t scale_command_to_pulse(float val)
    {
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;
        return static_cast<uint16_t>(PWM_PULSE_MIN_US + val * (PWM_PULSE_MAX_US - PWM_PULSE_MIN_US));
    }
};

/* Singleton Instance */
static JMRRC_DroneCAN_Interlock s_interlock;

/* Public C wrappers for AP_Periph integration */
extern "C" {

void DroneCAN_OnCommandGridReceived(const void* msg_payload, uint32_t now_ms)
{
    const CommandGridMessage* msg = static_cast<const CommandGridMessage*>(msg_payload);
    s_interlock.handle_actuator_command(*msg, now_ms);
}

void DroneCAN_OnAutopilotStatusReceived(const void* msg_payload, uint32_t now_ms)
{
    const AutopilotStatusMessage* msg = static_cast<const AutopilotStatusMessage*>(msg_payload);
    s_interlock.handle_autopilot_status(*msg, now_ms);
}

void DroneCAN_Interlock_100Hz_Tick(uint32_t now_ms)
{
    s_interlock.update_100hz(now_ms);
}

}
