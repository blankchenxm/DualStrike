import keyboard
import datetime
import os
import sys
from threading import Lock

class KeystrokeRecorder:
    def __init__(self):
        self.characters = []
        self.timestamps = []
        self.recording = False
        self.lock = Lock()
        
    def on_key_event(self, event):
        """Handle key events"""
        if event.event_type == keyboard.KEY_DOWN:
            # Check for Ctrl+C to stop recording
            if event.name == 'c' and keyboard.is_pressed('ctrl'):
                self.stop_recording()
                return
            
            # Record key and time
            if self.recording:
                with self.lock:
                    current_time = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]  # Millisecond precision
                    
                    # Handle special keys
                    if event.name == 'space':
                        char = ' '
                    elif event.name == 'enter':
                        char = '\n'
                    elif len(event.name) == 1:
                        # Regular character
                        char = event.name
                        # Check if uppercase needed (Shift or CapsLock)
                        if keyboard.is_pressed('shift') or keyboard.is_pressed('caps lock'):
                            char = char.upper()
                    else:
                        # Skip other special keys
                        return
                    
                    self.characters.append(char)
                    self.timestamps.append(current_time)
    
    def start_recording(self):
        """Start recording"""
        self.recording = True
        self.characters = []
        self.timestamps = []
        
        # Register key listener
        keyboard.hook(self.on_key_event)
        
        try:
            # Keep program running
            keyboard.wait()
        except KeyboardInterrupt:
            pass
    
    def stop_recording(self):
        """Stop recording and save file"""
        if not self.recording:
            return
            
        self.recording = False
        keyboard.unhook_all()
        
        if not self.characters:
            return
        
        # Ensure directory exists
        output_dir = "3.Software/Data/keystroke_injection"
        os.makedirs(output_dir, exist_ok=True)
        
        # Generate filename with current timestamp
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"keystroke_record_{timestamp}.txt"
        filepath = os.path.join(output_dir, filename)
        
        # Save to file
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                # Write three columns: index, character, timestamp
                for i, (char, timestamp) in enumerate(zip(self.characters, self.timestamps), 1):
                    f.write(f"{i}, {char}, {timestamp}\n")
            
        except Exception as e:
            print(f"Error saving file: {e}")
        
        # Exit program
        sys.exit(0)

def main():
    recorder = KeystrokeRecorder()
    recorder.start_recording()

if __name__ == "__main__":
    main()
