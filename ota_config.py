import os
import re

Import("env")

# Optional: Map room names to specific IPs if mDNS (.local) doesn't work for you
# Example: "BonusRoom": "192.168.6.2"
ROOM_IPS = {
    "Basement": "192.168.6.1",
    "BonusRoom": "192.168.6.2",
}

def get_active_room():
    # Get path to ScriptConfig.h
    config_path = os.path.join(env.get("PROJECT_SRC_DIR"), "ScriptConfig.h")
    
    if not os.path.exists(config_path):
        print(f"Warning: {config_path} not found!")
        return None

    with open(config_path, "r") as f:
        lines = f.readlines()

    # List of known room defines to look for (based on your ScriptConfig.h)
    known_rooms = ["Basement", "BonusRoom", "Garage", "Test"]

    for line in lines:
        line = line.strip()
        # Check for uncommented #define
        # We look for lines starting with #define, ignoring those starting with //
        if line.startswith("#define"):
            parts = line.split()
            if len(parts) >= 2:
                define_name = parts[1]
                if define_name in known_rooms:
                    return define_name
    return None

room = get_active_room()

if room:
    # Check if we have a static IP mapped
    upload_port = ROOM_IPS.get(room)
    
    # If not, use mDNS hostname
    if not upload_port:
        upload_port = f"{room}.local"
        
    print(f"Auto-configuring OTA Upload Port for {room}: {upload_port}")
    env.Replace(UPLOAD_PORT=upload_port)
else:
    print("Could not detect active room in ScriptConfig.h")
