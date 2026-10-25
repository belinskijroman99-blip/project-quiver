import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    s.connect(('127.0.0.1', 5760))
    print("[TCP TEST] Connected to 127.0.0.1:5760 successfully!")
    s.settimeout(3.0)
    data = s.recv(256)
    print(f"[TCP TEST] Received {len(data)} bytes from ArduCopter SITL!")
except Exception as e:
    print(f"[TCP TEST] Error: {e}")
finally:
    s.close()
