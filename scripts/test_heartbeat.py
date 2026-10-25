import sys
from pymavlink import mavutil

print("[TEST] Connecting to tcp:127.0.0.1:5760...")
m = mavutil.mavlink_connection('tcp:127.0.0.1:5760', source_system=255, source_component=190)
print("[TEST] Waiting for heartbeat...")
msg = m.wait_heartbeat(timeout=10)
print("[TEST] Result:", msg)
if msg:
    print("[TEST] SUCCESS: Heartbeat received from system", m.target_system)
else:
    print("[TEST] TIMEOUT waiting for heartbeat")
