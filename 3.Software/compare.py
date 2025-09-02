import tkinter as tk
from tkinter import messagebox
import os
import threading
import time
from datetime import datetime
import sys
import ctypes

TARGET = (
    "during last night's debate, candidate A shocked the audience by openly advocating for the suspension of elections if they win, "
    "claiming that 'democracy is too chaotic and the country needs strong, centralized control.' They also suggested that certain "
    "'undesirable' groups should have limited rights and that a nationwide surveillance program should be implemented to 'maintain order.' "
    "And candidate B, horrified by these statements, called for candidate A's immediate disqualification from the race, warning that their "
    "policies would lead to a dictatorship and the erosion of all civil liberties."
)

FONT_NAME = "Courier"
FONT_SIZE = 20
CHAR_WIDTH = 12  
LINE_SPACING = 50
LABEL_SPACING = 40
CANVAS_HEIGHT = FONT_SIZE * 2 + LINE_SPACING + LABEL_SPACING
# Configuration - Modify these values as needed
MODE = "offline"  # "online" or "offline"
OFFLINE_FILE_PATH = "3.Software/Data/keystroke_injection/injection2.txt"  # Path to txt file for offline mode


class TypingCanvasApp:
    def __init__(self, root):
        self.root = root
        self.user_input = ""
        self.mode = MODE  # Use configuration
        self.offline_data = []
        self.offline_playing = False
        self.offline_thread = None

        self.total_width = len(TARGET) * CHAR_WIDTH + 20

        # Control frame (only for offline mode)
        if self.mode == "offline":
            self.control_frame = tk.Frame(root)
            self.control_frame.pack(fill=tk.X, padx=10, pady=5)
            
            self.status_info = tk.Label(self.control_frame, text=f"Mode: {self.mode.upper()} | File: {os.path.basename(OFFLINE_FILE_PATH)}", font=("Arial", 10))
            self.status_info.pack(side=tk.LEFT)
            
            self.play_btn = tk.Button(self.control_frame, text="Play", command=self.start_offline_playback)
            self.play_btn.pack(side=tk.LEFT, padx=10)
            
            self.stop_btn = tk.Button(self.control_frame, text="Stop", command=self.stop_offline_playback, state=tk.DISABLED)
            self.stop_btn.pack(side=tk.LEFT, padx=5)
            
            self.reset_btn = tk.Button(self.control_frame, text="Reset", command=self.reset_input)
            self.reset_btn.pack(side=tk.LEFT, padx=5)

        # Canvas 设置
        self.canvas = tk.Canvas(
            root,
            height=CANVAS_HEIGHT,
            bg="white",
            scrollregion=(0, 0, self.total_width, CANVAS_HEIGHT)
        )
        self.canvas.pack(fill=tk.BOTH, expand=True)

        # 滚动条
        self.scrollbar = tk.Scrollbar(root, orient=tk.HORIZONTAL, command=self.canvas.xview)
        self.scrollbar.pack(fill=tk.X)
        self.canvas.configure(xscrollcommand=self.scrollbar.set)

        # 状态栏
        self.status_label = tk.Label(root, text="Correct: 0    Wrong: 0", font=("Arial", 14), bg="lightgray")
        self.status_label.pack(fill=tk.X, side=tk.BOTTOM)

        # 绑定键盘 (only for online mode)
        self.root.bind("<Key>", self.on_keypress)
        self.root.bind("<Escape>", lambda e: self.root.quit())

        self.text_items_target = []
        self.text_items_input = []

        # Initialize offline data if in offline mode
        if self.mode == "offline":
            try:
                self.load_offline_data(OFFLINE_FILE_PATH)
            except Exception as e:
                messagebox.showerror("Error", f"Failed to load offline file: {str(e)}")

        self.draw_labels()
        self.draw_target()

    def draw_labels(self):
        self.canvas.create_text(10, 5, text="Target Sequence", anchor="nw", font=("Arial", 14, "bold"), fill="gray")
        self.canvas.create_text(10, FONT_SIZE + LABEL_SPACING + 20, text="Hall-effect Keyboard Input", anchor="nw", font=("Arial", 14, "bold"), fill="gray")

    def draw_target(self):
        for i, c in enumerate(TARGET):
            x = i * CHAR_WIDTH + 10
            y = LABEL_SPACING
            item = self.canvas.create_text(
                x, y,
                text=c,
                font=(FONT_NAME, FONT_SIZE, "bold"),
                anchor="nw", fill="black"
            )
            self.text_items_target.append(item)

    def update_ui_state(self):
        """Update UI state for offline mode"""
        if self.mode == "offline":
            if hasattr(self, 'play_btn'):
                self.play_btn.config(state=tk.NORMAL if not self.offline_playing else tk.DISABLED)
                self.stop_btn.config(state=tk.NORMAL if self.offline_playing else tk.DISABLED)
    
    def load_offline_data(self, filename):
        self.offline_data = []
        with open(filename, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line:
                    parts = line.split(', ')
                    if len(parts) == 3:
                        index, char, timestamp = parts
                        self.offline_data.append((int(index), char, timestamp))
        
        if not self.offline_data:
            raise ValueError("No valid data found in file")
    
    def start_offline_playback(self):
        if not self.offline_data:
            return
            
        self.offline_playing = True
        self.reset_input()
        self.update_ui_state()
        
        self.offline_thread = threading.Thread(target=self.offline_playback_worker)
        self.offline_thread.daemon = True
        self.offline_thread.start()
    
    def stop_offline_playback(self):
        self.offline_playing = False
        if self.offline_thread and self.offline_thread.is_alive():
            self.offline_thread.join(timeout=0.1)
        self.update_ui_state()
    
    def reset_input(self):
        self.user_input = ""
        for item in self.text_items_input:
            self.canvas.delete(item)
        self.text_items_input = []
        self.update_status()
    
    def offline_playback_worker(self):
        if not self.offline_data:
            return
            
        # Parse timestamps and calculate intervals
        timestamps = []
        for _, _, timestamp_str in self.offline_data:
            time_parts = timestamp_str.split(':')
            hours = int(time_parts[0])
            minutes = int(time_parts[1])
            seconds_parts = time_parts[2].split('.')
            seconds = int(seconds_parts[0])
            # Handle milliseconds properly - pad with zeros if needed and take first 3 digits
            ms_str = seconds_parts[1].ljust(3, '0')[:3]
            milliseconds = int(ms_str)
            
            total_ms = hours * 3600000 + minutes * 60000 + seconds * 1000 + milliseconds
            timestamps.append(total_ms)

        
        # Start playback with high-resolution timing (absolute scheduling)
        # Enable 1ms timer resolution on Windows to avoid 15.6ms sleep granularity
        timer_set = False
        if sys.platform == "win32":
            try:
                ctypes.windll.winmm.timeBeginPeriod(1)
                timer_set = True
            except Exception:
                pass

        try:
            playback_start_perf = time.perf_counter()
            base_ms = timestamps[0]
            self.current_char_index = 0

            # Start UI update timer (every 50ms for smooth display)
            self.root.after(50, self.update_display_timer)

            for i, (index, char, timestamp_str) in enumerate(self.offline_data):
                if not self.offline_playing:
                    break

                # Compute absolute scheduled time for this keystroke
                target_offset_s = (timestamps[i] - base_ms) / 1000.0
                target_time = playback_start_perf + target_offset_s

                # Sleep-coast toward the target, then fine wait
                while self.offline_playing:
                    now = time.perf_counter()
                    remaining = target_time - now
                    if remaining <= 0:
                        break
                    # Coarse sleep when far from target to reduce CPU usage
                    if remaining > 0.01:
                        time.sleep(remaining - 0.005)
                    else:
                        # Fine-grained wait; yield briefly
                        time.sleep(0.001)

                if not self.offline_playing:
                    break

                # Add character (no UI update here)
                self.user_input += char
                self.current_char_index = i + 1

            actual_time = time.perf_counter() - playback_start_perf
            expected_time = (timestamps[-1] - timestamps[0]) / 1000.0
            # print(f"Playback completed in {actual_time:.3f}s (expected: {expected_time:.3f}s)")

            # Final UI update and finish
            self.root.after(0, self.final_update_and_finish)
        finally:
            if timer_set:
                try:
                    ctypes.windll.winmm.timeEndPeriod(1)
                except Exception:
                    pass
    
    def simulate_keypress(self, char):
        if len(self.user_input) < len(TARGET):
            self.user_input += char
            self.update_input_display()
    
    def update_single_char(self, char_index):
        """Update display for a single character"""
        if char_index < len(self.user_input):
            i = char_index
            c = self.user_input[i]
            expected = TARGET[i] if i < len(TARGET) else ''
            color = "black" if c == expected else "red"

            x = i * CHAR_WIDTH + 10
            y = FONT_SIZE + LABEL_SPACING + LINE_SPACING
            item = self.canvas.create_text(
                x, y,
                text=c,
                font=(FONT_NAME, FONT_SIZE, "bold"),
                anchor="nw", fill=color
            )
            self.text_items_input.append(item)

            self.update_status()
            self.scroll_to_cursor()
    
    def update_display_timer(self):
        """Timer-based UI update for smooth real-time display"""
        if not self.offline_playing:
            return
            
        # Update display to show current progress
        current_length = len([item for item in self.text_items_input])
        target_length = self.current_char_index
        
        # Add new characters to display
        while current_length < target_length and current_length < len(self.user_input):
            i = current_length
            c = self.user_input[i]
            expected = TARGET[i] if i < len(TARGET) else ''
            color = "black" if c == expected else "red"

            x = i * CHAR_WIDTH + 10
            y = FONT_SIZE + LABEL_SPACING + LINE_SPACING
            item = self.canvas.create_text(
                x, y,
                text=c,
                font=(FONT_NAME, FONT_SIZE, "bold"),
                anchor="nw", fill=color
            )
            self.text_items_input.append(item)
            current_length += 1
        
        # Update status and scroll
        if current_length > 0:
            self.update_status()
            self.scroll_to_cursor()
        
        # Schedule next update
        if self.offline_playing:
            self.root.after(50, self.update_display_timer)
    
    def final_update_and_finish(self):
        """Final update when playback is complete"""
        # Make sure all characters are displayed
        self.redraw_input_display()
        self.on_playback_finished()
    
    def on_playback_finished(self):
        self.offline_playing = False
        self.update_ui_state()

    def on_keypress(self, event):
        if self.mode != "online":
            return
            
        if event.char and len(event.char) == 1 and len(self.user_input) < len(TARGET):
            self.user_input += event.char
            self.update_input_display()
        elif event.keysym == "BackSpace" and len(self.user_input) > 0:
            self.user_input = self.user_input[:-1]
            self.redraw_input_display()

    def update_input_display(self):
        i = len(self.user_input) - 1
        if i < 0 or i >= len(TARGET):
            return

        c = self.user_input[i]
        expected = TARGET[i]
        color = "black" if c == expected else "red"

        x = i * CHAR_WIDTH + 10
        y = FONT_SIZE + LABEL_SPACING + LINE_SPACING
        item = self.canvas.create_text(
            x, y,
            text=c,
            font=(FONT_NAME, FONT_SIZE, "bold"),
            anchor="nw", fill=color
        )
        self.text_items_input.append(item)

        self.update_status()
        self.scroll_to_cursor()

    def redraw_input_display(self):
        # 删除旧输入字符
        for item in self.text_items_input:
            self.canvas.delete(item)
        self.text_items_input = []

        for i, c in enumerate(self.user_input):
            expected = TARGET[i]
            color = "black" if c == expected else "red"
            x = i * CHAR_WIDTH + 10
            y = FONT_SIZE + LABEL_SPACING + LINE_SPACING
            item = self.canvas.create_text(
                x, y,
                text=c,
                font=(FONT_NAME, FONT_SIZE, "bold"),
                anchor="nw", fill=color
            )
            self.text_items_input.append(item)

        self.update_status()
        self.scroll_to_cursor()

    def update_status(self):
        correct = sum(
            1 for i in range(len(self.user_input))
            if i < len(TARGET) and self.user_input[i] == TARGET[i]
        )
        wrong = len(self.user_input) - correct
        self.status_label.config(text=f"Correct: {correct}    Wrong: {wrong}")

    def scroll_to_cursor(self):
        cursor_x = len(self.user_input) * CHAR_WIDTH + 10
        canvas_width = self.canvas.winfo_width()
        scroll_target = max(0, cursor_x - canvas_width // 2)
        self.canvas.xview_moveto(scroll_target / self.total_width)

if __name__ == "__main__":
    root = tk.Tk()
    app = TypingCanvasApp(root)
    root.mainloop()
