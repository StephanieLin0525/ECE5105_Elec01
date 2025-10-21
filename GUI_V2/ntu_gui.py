import tkinter as tk
from tkinter import ttk, messagebox
import serial
import threading
import time
import random
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# ---------------------- CONFIGURATION ----------------------
SERIAL_PORT = "COM5"      # Change to your STM32 port (e.g. /dev/ttyACM0 on Linux)
BAUD_RATE = 115200
REFRESH_INTERVAL = 0.5    # seconds between reads
SIMULATION_MODE = False    # True: simulate random NTU data

USERNAME = "admin"
PASSWORD = "1234"
# ------------------------------------------------------------


class NTUMonitorApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("NTU Monitoring System")
        self.geometry("1000x700")
        self.resizable(False, False)

        self.serial_conn = None
        self.data_thread = None
        self.running = False
        self.ntu_values = []
        self.timestamps = []
        self.threshold = 10.0
        self.paused = True  # Start in paused mode

        self._show_login()

    # ---------------- LOGIN SCREEN ----------------
    def _show_login(self):
        self.login_frame = ttk.Frame(self)
        self.login_frame.pack(expand=True)

        ttk.Label(self.login_frame, text="Username:").pack(pady=5)
        self.username_entry = ttk.Entry(self.login_frame)
        self.username_entry.pack(pady=5)

        ttk.Label(self.login_frame, text="Password:").pack(pady=5)
        self.password_entry = ttk.Entry(self.login_frame, show="*")
        self.password_entry.pack(pady=5)

        ttk.Button(self.login_frame, text="Login", command=self._authenticate).pack(pady=10)

    def _authenticate(self):
        user = self.username_entry.get()
        pwd = self.password_entry.get()
        if user == USERNAME and pwd == PASSWORD:
            self.login_frame.destroy()
            self._setup_main_ui()
            if not SIMULATION_MODE:
                self._connect_serial()
            self._start_background_thread()
        else:
            messagebox.showerror("Access Denied", "Invalid username or password.")

    # ---------------- MAIN UI ----------------
    def _setup_main_ui(self):
        # ---------- Control Frame ----------
        control_frame = ttk.Frame(self)
        control_frame.pack(side=tk.TOP, fill=tk.X, pady=10)

        ttk.Label(control_frame, text="Set Threshold (NTU):").pack(side=tk.LEFT, padx=5)
        self.threshold_entry = ttk.Entry(control_frame, width=10)
        self.threshold_entry.insert(0, str(self.threshold))
        self.threshold_entry.pack(side=tk.LEFT, padx=5)

        ttk.Button(control_frame, text="Update Threshold", command=self._update_threshold).pack(side=tk.LEFT, padx=5)

        # Start / Stop buttons
        ttk.Button(control_frame, text="Start Readings", command=self._start_readings).pack(side=tk.LEFT, padx=10)
        ttk.Button(control_frame, text="Stop Readings", command=self._stop_readings).pack(side=tk.LEFT, padx=10)

        # LED indicator
        self.led_canvas = tk.Canvas(control_frame, width=50, height=50, bg="white", highlightthickness=0)
        self.led_canvas.pack(side=tk.RIGHT, padx=20)
        self.led = self.led_canvas.create_oval(10, 10, 40, 40, fill="green")

        # ---------- Plot ----------
        fig = Figure(figsize=(9, 4), dpi=100)
        self.ax = fig.add_subplot(111)
        self.ax.set_title("Turbidity Monitor")
        self.ax.set_xlabel("Time (s)")
        self.ax.set_ylabel("NTU")
        self.line, = self.ax.plot([], [], color="blue", label="NTU Data")

        # Add threshold line
        self.threshold_line = self.ax.axhline(y=self.threshold, color="red", linestyle="--", label="Threshold")

        self.ax.legend()
        self.canvas = FigureCanvasTkAgg(fig, master=self)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=10, pady=5)

        # ---------- Data History ----------
        history_frame = ttk.Frame(self)
        history_frame.pack(side=tk.BOTTOM, fill=tk.X, pady=10)

        ttk.Label(history_frame, text="Last 10 NTU Values:").pack(anchor="w", padx=10)
        self.history_box = tk.Text(history_frame, height=3, width=120, wrap="word", state="disabled", bg="#f5f5f5")
        self.history_box.pack(padx=10, pady=5)

    def _update_threshold(self):
        """Update threshold and horizontal line position."""
        try:
            val = float(self.threshold_entry.get())
            self.threshold = val
            self.threshold_line.set_ydata([val, val])
            self.canvas.draw_idle()
        except ValueError:
            messagebox.showerror("Invalid Input", "Please enter a valid number.")

    # ---------------- SERIAL HANDLING ----------------
    def _connect_serial(self):
        try:
            self.serial_conn = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            messagebox.showinfo("Serial Connected", f"Connected to {SERIAL_PORT}")
        except Exception as e:
            messagebox.showwarning(
                "Simulation Fallback",
                f"Could not connect to {SERIAL_PORT}:\n{e}\n\nSwitching to simulation mode."
            )
            global SIMULATION_MODE
            SIMULATION_MODE = True

    def _start_background_thread(self):
        self.running = True
        self.data_thread = threading.Thread(target=self._read_data_loop, daemon=True)
        self.data_thread.start()
        self._update_plot()

    # ---------------- CONTROL BUTTONS ----------------
    def _start_readings(self):
        """Resume data acquisition."""
        self.paused = False

    def _stop_readings(self):
        """Pause data acquisition (keeps plot)."""
        self.paused = True

    # ---------------- DATA LOOP ----------------
    def _read_data_loop(self):
        start_time = time.time()
        while True:
            try:
                if self.paused or not self.running:
                    time.sleep(0.2)
                    continue

                if SIMULATION_MODE:
                    ntu = random.uniform(0, 20)
                    t = time.time() - start_time
                    self.timestamps.append(t)
                    self.ntu_values.append(ntu)
                    if len(self.timestamps) > 100:
                        self.timestamps.pop(0)
                        self.ntu_values.pop(0)
                    self._check_alarm(ntu)
                    self._update_history_box()
                    time.sleep(REFRESH_INTERVAL)
                else:
                    if self.serial_conn and self.serial_conn.in_waiting > 0:
                        line = self.serial_conn.readline().decode(errors='ignore').strip()
                        if line:
                            try:
                                ntu = float(line)
                            except Exception:
                                continue
                            t = time.time() - start_time
                            self.timestamps.append(t)
                            self.ntu_values.append(ntu)
                            if len(self.timestamps) > 100:
                                self.timestamps.pop(0)
                                self.ntu_values.pop(0)
                            self._check_alarm(ntu)
                            self._update_history_box()
                    else:
                        time.sleep(REFRESH_INTERVAL)
            except Exception as e:
                print("Data read error:", e)
                time.sleep(REFRESH_INTERVAL)

    # ---------------- ALARM / LED ----------------
    def _check_alarm(self, ntu):
        """Set LED color depending on threshold."""
        color = "red" if ntu > self.threshold else "green"
        self.led_canvas.itemconfig(self.led, fill=color)

    # ---------------- HISTORY ----------------
    def _update_history_box(self):
        recent_values = self.ntu_values[-10:]
        text = ",  ".join(f"{v:.2f}" for v in recent_values)
        self.history_box.config(state="normal")
        self.history_box.delete("1.0", tk.END)
        self.history_box.insert(tk.END, text)
        self.history_box.config(state="disabled")

    # ---------------- PLOTTING ----------------
    def _update_plot(self):
        if self.ntu_values:
            self.line.set_data(self.timestamps, self.ntu_values)
            self.ax.relim()
            self.ax.autoscale_view()
            self.canvas.draw_idle()
        self.after(1000, self._update_plot)

    def on_close(self):
        self.running = False
        if self.serial_conn:
            self.serial_conn.close()
        self.destroy()


if __name__ == "__main__":
    app = NTUMonitorApp()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()
