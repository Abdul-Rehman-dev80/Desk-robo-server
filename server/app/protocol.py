"""Messages exchanged with the robot over /ws/robot.

Text frames carry JSON control messages with a "type" field. Binary frames
carry audio: 16 kHz, 16-bit little-endian mono PCM in both directions.
"""

PROTOCOL_VERSION = 1
AUDIO_SAMPLE_RATE = 16000

# Robot -> server
HELLO = "hello"   # {"type": "hello", "device_id": str, "fw": str}
PING = "ping"     # {"type": "ping"}
EVENT = "event"   # {"type": "event", "name": str, ...}

# Server -> robot
WELCOME = "welcome"  # {"type": "welcome", "protocol": int, "audio": {...}}
PONG = "pong"
ERROR = "error"      # {"type": "error", "message": str}
