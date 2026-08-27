#!/usr/bin/env python3
"""
🪶 Ibis Setup 🪶 - Desktop configuration tool for Ibis Dash
WIZARD STYLE - Step by step setup with loading popups
Version 4.0 - UI polish: auto-advance, Personalize tab, Options tab, consistent styling 🪶🪶🪶

Steps:
1. Connect - Connect to board via USB (auto-loads existing config)
2. WiFi - Enter WiFi credentials (tests connection)
3. Garmin - Register with Garmin Middleware
4. Personalize - Name, sport, goal, refresh interval
5. Options (🤌) - Delete data from board
"""

import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import json
import os
import time
import webbrowser
import threading
import urllib.parse
import urllib.request
import urllib.error
import http.server
import socketserver
import random

APP_TITLE = "🪶 Ibis Setup 🪶"
APP_VERSION = "4.2"
WINDOW_WIDTH = 650
WINDOW_HEIGHT = 750
LOADING_WIDTH = 450
LOADING_HEIGHT = 250
BAUD_RATE = 115200
SERIAL_TIMEOUT = 15
WRITE_TIMEOUT = 15

# ╔═══════════════════════════════════════════════════════════════════════════════╗
# ║                        🎨 COLOR SCHEME - EDIT HERE 🎨                         ║
# ╚═══════════════════════════════════════════════════════════════════════════════╝

COLOR_BG = "#101022"
COLOR_CARD = "#102a4a"
COLOR_LOADING_BG = "#0a0a1a"
COLOR_ACCENT = "#e94560"
COLOR_SUCCESS = "#4fd2b0"
COLOR_DANGER = "#e94560"
COLOR_TEXT = "#eef1fa"
COLOR_TEXT_DIM = "#9da3cc"
COLOR_LINK = "#7bb8ff"
COLOR_BTN_PRIMARY = "#e94560"
COLOR_BTN_PRIMARY_HOVER = "#ff5e79"
COLOR_BTN_SECONDARY = "#1c3f7a"
COLOR_BTN_SECONDARY_HOVER = "#285799"
COLOR_BTN_SUCCESS = "#4fd2b0"
COLOR_BTN_SUCCESS_HOVER = "#6ae5c4"
COLOR_BTN_DANGER = "#e94560"
COLOR_BTN_DANGER_HOVER = "#ff5e79"
COLOR_BTN_TEXT = "#ffffff"
COLOR_CONNECTED = "#4fd2b0"
COLOR_DISCONNECTED = "#e94560"
COLOR_STEP_INACTIVE = "#1c3f7a"
COLOR_STEP_ACTIVE = "#e94560"
COLOR_STEP_COMPLETE = "#4fd2b0"

# Consistent subtitle font (white, same as field labels)
SUBTITLE_FONT = ('Segoe UI', 11)
SUBTITLE_COLOR = "#ffffff"

SPORT_TYPES = ["Run", "Ride", "Swim", "Hike", "Walk"]
TRACK_PERIODS = ["Yearly", "Monthly", "Weekly"]
REFRESH_OPTIONS = [
    ("Every hour", 1, "~3-5 days battery"),
    ("Every 6 hours", 6, "~2-3 weeks battery"),
    ("Every 12 hours", 12, "~1 month battery"),
    ("Once a day", 24, "~2 months battery"),
    ("Every 2 days", 48, "~3-4 months battery"),
    ("Once a week", 168, "~6+ months battery")
]
F = "🪶"

OAUTH_REDIRECT_PORT = 8089
OAUTH_REDIRECT_URI = f"http://localhost:{OAUTH_REDIRECT_PORT}/callback"
STRAVA_AUTH_URL = "https://www.strava.com/oauth/authorize"
STRAVA_TOKEN_URL = "https://www.strava.com/oauth/token"

CONFIG_DIR = os.path.join(os.path.expanduser("~"), "Library", "Application Support", "Ibis Setup")
LOCAL_CONFIG_FILE = os.path.join(CONFIG_DIR, "saved_config.json")

# Funny loading messages for different operations
FUNNY_MESSAGES = {
    'connect': [
        "Waking up the sleepy ibis...",
        "Teaching the ibis to speak USB...",
        "Bribing the ibis with virtual worms...",
        "Convincing the ibis we're friends..."
    ],
    'load_config': [
        "Ibis is rummaging through its nest...",
        "Reading the ibis's diary...",
        "Asking the ibis what it remembers...",
        "Ibis is checking its notes..."
    ],
    'save_config': [
        "Ibis is writing this down...",
        "Teaching the ibis your WiFi password...",
        "Ibis is taking notes (with its beak)...",
        "Storing secrets in the ibis nest..."
    ],
    'test_wifi': [
        "Ibis is looking for the internet...",
        "Ibis is sniffing for WiFi signals...",
        "Teaching the ibis to connect to WiFi...",
        "Ibis is trying to remember the password...",
        "Checking if the internet has worms..."
    ],
    'fetch_strava': [
        "Ibis is asking Garmin for fresh stats...",
        "Ibis is counting your kilometers...",
        "Fetching your athletic achievements...",
        "Ibis is very impressed with your stats...",
        "Downloading proof of your fitness..."
    ],
    'update_display': [
        "Ibis is pecking at the screen...",
        "Ibis is painting with e-ink...",
        "Ibis is doing its screen dance...",
        "Arranging pixels with beak precision...",
        "Ibis is making art happen...",
        "The ibis is very focused right now..."
    ],
    'wipe': [
        "Ibis is forgetting everything...",
        "Erasing the ibis's memories...",
        "Ibis is having a fresh start...",
        "Wiping the ibis's tiny brain clean...",
        "Teaching the ibis to forget..."
    ],
    'updating': [
        "Updating the ibis's knowledge...",
        "Teaching the ibis new tricks...",
        "Ibis is learning your preferences...",
        "Refreshing the ibis's memory..."
    ]
}


class OAuthCallbackHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, callback=None, **kwargs):
        self.callback = callback
        super().__init__(*args, **kwargs)
    
    def do_GET(self):
        if self.path.startswith('/callback'):
            query = urllib.parse.urlparse(self.path).query
            params = urllib.parse.parse_qs(query)
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            if 'code' in params:
                self.wfile.write("""<html><body style="font-family:'Segoe UI',Arial,sans-serif;background:#1a1a2e;color:#eaeaea;text-align:center;padding:60px 20px;">
                    <h1 style="color:#4ecca3;font-size:52px;font-weight:900;letter-spacing:3px;margin-bottom:10px;">SUCCESS!!!</h1>
                    <p style="font-size:20px;color:#9da3cc;margin-top:20px;">YOU CAN NOW CLOSE THIS WINDOW</p></body></html>""".encode())
                if self.callback:
                    self.callback(params['code'][0])
            else:
                self.wfile.write(b"<html><body style='font-family:Arial;background:#1a1a2e;color:#e94560;text-align:center;padding:50px;'><h1>Authorization failed.</h1></body></html>")
                if self.callback:
                    self.callback(None)
        else:
            self.send_response(404)
            self.end_headers()
    
    def log_message(self, format, *args):
        pass


class IbisSetupWizard:
    def __init__(self, root):
        self.root = root
        self.root.title(f"{APP_TITLE} v{APP_VERSION}")
        self.root.configure(bg=COLOR_BG)
        self.root.resizable(False, False)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        
        # Center window on screen
        self.root.update_idletasks()
        screen_width = self.root.winfo_screenwidth()
        screen_height = self.root.winfo_screenheight()
        x = (screen_width - WINDOW_WIDTH) // 2
        y = (screen_height - WINDOW_HEIGHT) // 2
        self.root.geometry(f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}+{x}+{y}")
        
        self.serial_conn = None
        self.connected = False
        self.current_step = 0
        self.total_steps = 5  # Connect, WiFi, Garmin, Personalize, Options
        self.loading_overlay = None
        self.loading_animation_id = None
        
        # Track which steps are complete (properly validated)
        self.wifi_complete = False
        self.strava_complete = False
        
        # Track if setup has been completed at least once this session
        self.setup_done = False
        
        # Data variables
        self.port_var = tk.StringVar()
        self.ssid_var = tk.StringVar()
        self.password_var = tk.StringVar()
        self.client_id_var = tk.StringVar()
        self.client_secret_var = tk.StringVar()
        self.refresh_token_var = tk.StringVar()
        self.middleware_url_var = tk.StringVar()
        self.middleware_app_key_var = tk.StringVar()
        self.ibis_token_var = tk.StringVar()
        self.garmin_email_var = tk.StringVar()
        self.garmin_password_var = tk.StringVar()
        self.garmin_mfa_var = tk.StringVar()
        self.maps_api_key_var = tk.StringVar()
        self.name_var = tk.StringVar()
        self.sport_var = tk.StringVar(value="Run")
        self.sport2_var = tk.StringVar(value="")
        self.period_var = tk.StringVar(value="Yearly")
        self.goal_var = tk.StringVar(value="1000")
        self.goal2_var = tk.StringVar(value="")
        self.refresh_var = tk.StringVar(value="Once a day")
        self.battery_var = tk.StringVar(value="~2 months battery")
        self.token_status_var = tk.StringVar(value="")
        self.garmin_session_token = ""
        self.garmin_mfa_token = ""

        self._cache_save_after_id = None
        self._suspend_cache_save = False
        self.load_local_config()
        self.install_cache_traces()
        
        self.create_ui()
        self.show_step(0)
        self.scan_ports()

    # ==================== LOCAL CONFIG CACHE ====================
    def get_form_cache(self):
        """Return exactly what the user typed, including secrets, for local recall."""
        return {
            "ssid": self.ssid_var.get(),
            "password": self.password_var.get(),
            "clientID": self.client_id_var.get(),
            "clientSecret": self.client_secret_var.get(),
            "refreshToken": self.refresh_token_var.get(),
            "dataSource": "garmin_middleware" if self.has_middleware_credentials() else "strava",
            "middlewareUrl": self.middleware_url_var.get(),
            "middlewareAppKey": self.middleware_app_key_var.get(),
            "ibisToken": self.ibis_token_var.get(),
            "mapsApiKey": self.maps_api_key_var.get(),
            "name": self.name_var.get(),
            "sport": self.sport_var.get(),
            "sport2": self.sport2_var.get(),
            "period": self.period_var.get(),
            "goal": self.goal_var.get(),
            "goal2": self.goal2_var.get(),
            "refresh": self.refresh_var.get(),
            "battery": self.battery_var.get(),
        }

    def apply_cached_config(self, config, allow_blank=False):
        """Apply cached or board config without letting a blank board erase local fields."""
        def set_if_allowed(var, key, default=""):
            if key not in config:
                return
            value = config.get(key)
            if value is None:
                return
            value = str(value)
            if value or allow_blank:
                var.set(value if value else default)

        set_if_allowed(self.ssid_var, "ssid")
        set_if_allowed(self.password_var, "password")
        set_if_allowed(self.client_id_var, "clientID")
        set_if_allowed(self.client_secret_var, "clientSecret")
        set_if_allowed(self.refresh_token_var, "refreshToken")
        set_if_allowed(self.middleware_url_var, "middlewareUrl")
        set_if_allowed(self.middleware_app_key_var, "middlewareAppKey")
        set_if_allowed(self.ibis_token_var, "ibisToken")
        set_if_allowed(self.maps_api_key_var, "mapsApiKey")
        set_if_allowed(self.name_var, "name")

        if config.get("sport"):
            self.sport_var.set(config.get("sport"))
        if "sport2" in config and (config.get("sport2") or allow_blank):
            self.sport2_var.set(config.get("sport2", ""))

        if "goal" in config and (config.get("goal") not in ("", None) or allow_blank):
            self.goal_var.set(str(config.get("goal", "1000") or "1000"))
        if "goal2" in config and (config.get("goal2") not in ("", None) or allow_blank):
            g2 = config.get("goal2", "")
            self.goal2_var.set("" if g2 in (0, 0.0, "0", "0.0", None) else str(g2))

        if config.get("period") in TRACK_PERIODS:
            self.period_var.set(config.get("period"))
        elif "trackPeriod" in config:
            try:
                idx = int(config.get("trackPeriod", 0))
                if 0 <= idx < len(TRACK_PERIODS):
                    self.period_var.set(TRACK_PERIODS[idx])
            except (TypeError, ValueError):
                pass

        if config.get("refresh") in [o[0] for o in REFRESH_OPTIONS]:
            self.refresh_var.set(config.get("refresh"))
        elif "refreshHours" in config:
            try:
                hrs = int(config.get("refreshHours", 24))
                for n, v, e in REFRESH_OPTIONS:
                    if v == hrs:
                        self.refresh_var.set(n)
                        self.battery_var.set(e)
                        break
            except (TypeError, ValueError):
                pass

        if config.get("battery"):
            self.battery_var.set(config.get("battery"))
        else:
            self.update_battery()

        self.wifi_complete = bool(self.ssid_var.get().strip())
        self.strava_complete = self.has_dashboard_credentials()

    def load_local_config(self):
        try:
            with open(LOCAL_CONFIG_FILE, "r", encoding="utf-8") as f:
                self.apply_cached_config(json.load(f), allow_blank=False)
        except FileNotFoundError:
            return
        except Exception as e:
            print(f"Could not load saved Ibis settings: {e}")

    def install_cache_traces(self):
        vars_to_watch = [
            self.ssid_var, self.password_var, self.client_id_var,
            self.client_secret_var, self.refresh_token_var, self.middleware_url_var,
            self.middleware_app_key_var, self.ibis_token_var, self.maps_api_key_var,
            self.name_var, self.sport_var, self.sport2_var, self.period_var,
            self.goal_var, self.goal2_var, self.refresh_var, self.battery_var,
        ]
        for var in vars_to_watch:
            var.trace_add("write", lambda *_: self.schedule_local_config_save())

    def schedule_local_config_save(self):
        if self._suspend_cache_save:
            return
        if self._cache_save_after_id:
            try:
                self.root.after_cancel(self._cache_save_after_id)
            except Exception:
                pass
        self._cache_save_after_id = self.root.after(500, self.save_local_config)

    def save_local_config(self):
        self._cache_save_after_id = None
        try:
            os.makedirs(CONFIG_DIR, mode=0o700, exist_ok=True)
            tmp_file = f"{LOCAL_CONFIG_FILE}.tmp"
            with open(tmp_file, "w", encoding="utf-8") as f:
                json.dump(self.get_form_cache(), f, indent=2)
            os.chmod(tmp_file, 0o600)
            os.replace(tmp_file, LOCAL_CONFIG_FILE)
        except Exception as e:
            print(f"Could not save Ibis settings: {e}")

    def on_close(self):
        self.save_local_config()
        self.root.destroy()
    
    def create_ui(self):
        self.main_frame = tk.Frame(self.root, bg=COLOR_BG)
        self.main_frame.pack(fill=tk.BOTH, expand=True, padx=30, pady=20)
        
        # Header
        header = tk.Frame(self.main_frame, bg=COLOR_BG)
        header.pack(fill=tk.X, pady=(0, 12))
        tk.Label(header, text=f"{F} Ibis Setup {F}", font=('Segoe UI', 24, 'bold'),
                bg=COLOR_BG, fg=COLOR_ACCENT).pack()
        
        # Step indicator
        self.create_step_indicator()
        
        # Content area
        self.content_frame = tk.Frame(self.main_frame, bg=COLOR_CARD)
        self.content_frame.pack(fill=tk.BOTH, expand=True, pady=12)
        
        # Navigation
        self.create_navigation()
    
    def create_step_indicator(self):
        indicator = tk.Frame(self.main_frame, bg=COLOR_BG)
        indicator.pack(fill=tk.X, pady=(0, 8))
        
        steps = ["Connect", "WiFi", "Garmin", "Personalize", "Options"]
        self.step_labels = []
        self.step_dots = []
        
        inner = tk.Frame(indicator, bg=COLOR_BG)
        inner.pack(fill=tk.X)
        
        for i, name in enumerate(steps):
            frame = tk.Frame(inner, bg=COLOR_BG)
            frame.pack(side=tk.LEFT, expand=True, padx=6)

            
            dot = tk.Label(frame, text="\u25CF", font=('Segoe UI', 14), bg=COLOR_BG, 
                          fg=COLOR_STEP_INACTIVE, cursor='hand2')
            dot.pack()
            dot.bind('<Button-1>', lambda e, step=i: self.jump_to_step(step))
            self.step_dots.append(dot)
            
            label = tk.Label(frame, text=name, font=('Segoe UI', 8), bg=COLOR_BG, 
                           fg=COLOR_TEXT_DIM, cursor='hand2')
            label.pack()
            label.bind('<Button-1>', lambda e, step=i: self.jump_to_step(step))
            self.step_labels.append(label)
    
    def jump_to_step(self, step):
        """Allow jumping to any step once connected"""
        if not self.connected and step > 0:
            self.show_popup(f"{F} Not Connected", "Please connect to your board first!", popup_type="warning")
            return
        self.show_step(step)
    
    def update_step_indicator(self):
        """Update step indicator with checkmarks only for completed steps"""
        for i, (dot, label) in enumerate(zip(self.step_dots, self.step_labels)):
            # Options tab (4) always shows pinched fingers emoji, never changes
            if i == 4:
                if i == self.current_step:
                    dot.config(fg=COLOR_STEP_ACTIVE, text="\U0001F90C")
                    label.config(fg=COLOR_TEXT)
                else:
                    dot.config(fg=COLOR_STEP_INACTIVE, text="\U0001F90C")
                    label.config(fg=COLOR_TEXT_DIM)
                continue
            
            is_complete = False
            if i == 0:
                is_complete = self.connected
            elif i == 1:
                is_complete = self.wifi_complete
            elif i == 2:
                is_complete = self.strava_complete
            
            if is_complete:
                dot.config(fg=COLOR_STEP_COMPLETE, text="\u2713")
                label.config(fg=COLOR_STEP_COMPLETE)
            elif i == self.current_step:
                dot.config(fg=COLOR_STEP_ACTIVE, text="\u25CF")
                label.config(fg=COLOR_TEXT)
            else:
                dot.config(fg=COLOR_STEP_INACTIVE, text="\u25CF")
                label.config(fg=COLOR_TEXT_DIM)
    
    def create_navigation(self):
        nav = tk.Frame(self.main_frame, bg=COLOR_BG)
        nav.pack(fill=tk.X, pady=(8, 0))
        
        BTN_WIDTH = 12
        
        self.back_btn = tk.Button(nav, text="\u2190 Back", command=self.go_back,
                                 bg=COLOR_BTN_SECONDARY, fg=COLOR_BTN_TEXT,
                                 font=('Segoe UI', 10, 'bold'), relief=tk.FLAT,
                                 width=BTN_WIDTH, pady=8, cursor='hand2', highlightthickness=0,
                                 activebackground=COLOR_BTN_SECONDARY_HOVER)
        self.back_btn.pack(side=tk.LEFT)
        
        self.next_btn = tk.Button(nav, text="Next \u2192", command=self.go_next,
                                 bg=COLOR_BTN_PRIMARY, fg=COLOR_BTN_TEXT,
                                 font=('Segoe UI', 10, 'bold'), relief=tk.FLAT,
                                 width=BTN_WIDTH, pady=8, cursor='hand2', highlightthickness=0,
                                 activebackground=COLOR_BTN_PRIMARY_HOVER)
        self.next_btn.pack(side=tk.RIGHT)
    
    def show_step(self, step_num):
        self.current_step = step_num
        self.update_step_indicator()
        
        for widget in self.content_frame.winfo_children():
            widget.destroy()
        
        if step_num == 0:
            self.back_btn.pack_forget()
        else:
            self.back_btn.pack(side=tk.LEFT)
        
        if step_num == 0:
            self.show_connect_step()
            self.next_btn.config(text="Next \u2192", bg=COLOR_BTN_PRIMARY, 
                                activebackground=COLOR_BTN_PRIMARY_HOVER)
            self.next_btn.pack(side=tk.RIGHT)
        elif step_num == 1:
            self.show_wifi_step()
            self.next_btn.config(text="Save WiFi \u2192", bg=COLOR_BTN_SUCCESS,
                                activebackground=COLOR_BTN_SUCCESS_HOVER)
            self.next_btn.pack(side=tk.RIGHT)
        elif step_num == 2:
            self.show_strava_step()
            self.next_btn.config(text="Save Garmin \u2192", bg=COLOR_BTN_SUCCESS,
                                activebackground=COLOR_BTN_SUCCESS_HOVER)
            self.next_btn.pack(side=tk.RIGHT)
        elif step_num == 3:
            self.show_settings_step()
            self.next_btn.config(text="Finish Setup", bg=COLOR_BTN_SUCCESS,
                                activebackground=COLOR_BTN_SUCCESS_HOVER)
            self.next_btn.pack(side=tk.RIGHT)
        elif step_num == 4:
            self.show_options_step()
            self.next_btn.pack_forget()
    
    # ==================== STEP 1: CONNECT ====================
    def show_connect_step(self):
        content = tk.Frame(self.content_frame, bg=COLOR_CARD, padx=30, pady=25)
        content.pack(fill=tk.BOTH, expand=True)
        
        tk.Label(content, text=f"Connect Your Board",
                font=('Segoe UI', 16, 'bold'), bg=COLOR_CARD, fg=COLOR_ACCENT).pack(pady=(0, 8))
        
        tk.Label(content, text="Plug in your Ibis board via USB and select the port below.",
                font=SUBTITLE_FONT, bg=COLOR_CARD, fg=SUBTITLE_COLOR).pack(pady=(0, 20))
        
        port_row = tk.Frame(content, bg=COLOR_CARD)
        port_row.pack(pady=8)
        
        tk.Label(port_row, text="USB Port:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11)).pack(side=tk.LEFT, padx=(0, 10))
        
        self.port_combo = ttk.Combobox(port_row, textvariable=self.port_var, width=28, state='readonly')
        self.port_combo.pack(side=tk.LEFT, padx=(0, 10))
        
        tk.Button(port_row, text=f"{F} Refresh", command=self.scan_ports,
                 bg=COLOR_CARD, fg=COLOR_TEXT, font=('Segoe UI', 11),
                 relief=tk.FLAT, cursor='hand2').pack(side=tk.LEFT)
        
        self.connect_btn = tk.Button(content, text="Connect", command=self.toggle_connection,
                                    bg=COLOR_BTN_PRIMARY, fg=COLOR_BTN_TEXT,
                                    font=('Segoe UI', 12, 'bold'), relief=tk.FLAT,
                                    padx=35, pady=8, cursor='hand2', highlightthickness=0)
        self.connect_btn.pack(pady=15)
        
        self.conn_status = tk.Label(content, text="\u2716 Not connected",
                                   bg=COLOR_CARD, fg=COLOR_DISCONNECTED, font=('Segoe UI', 11))
        self.conn_status.pack()
        
        help_link = tk.Label(content, text=f"Connection issues? Click here for help",
                            bg=COLOR_CARD, fg=COLOR_LINK, font=('Segoe UI', 11, 'underline'), cursor='hand2')
        help_link.pack(pady=(15, 0))
        help_link.bind('<Button-1>', lambda e: self.show_connection_help())
        
        self.update_connect_ui()
    
    # ==================== STEP 2: WIFI ====================
    def show_wifi_step(self):
        content = tk.Frame(self.content_frame, bg=COLOR_CARD, padx=30, pady=25)
        content.pack(fill=tk.BOTH, expand=True)
        
        tk.Label(content, text=f"{F} WiFi Settings {F}",
                font=('Segoe UI', 16, 'bold'), bg=COLOR_CARD, fg=COLOR_ACCENT).pack(pady=(0, 8))
        
        tk.Label(content, text="Enter your home WiFi so Ibis can connect to the internet.*",
                font=SUBTITLE_FONT, bg=COLOR_CARD, fg=SUBTITLE_COLOR).pack(pady=(0, 20))
        
        form = tk.Frame(content, bg=COLOR_CARD)
        form.pack()
        
        row1 = tk.Frame(form, bg=COLOR_CARD)
        row1.pack(fill=tk.X, pady=8)
        tk.Label(row1, text="WiFi Name (SSID):", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=16, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row1, textvariable=self.ssid_var, width=28, font=('Segoe UI', 11)).pack(side=tk.LEFT)
        
        row2 = tk.Frame(form, bg=COLOR_CARD)
        row2.pack(fill=tk.X, pady=8)
        tk.Label(row2, text="WiFi Password:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=16, anchor='w').pack(side=tk.LEFT)
        self.pw_entry = tk.Entry(row2, textvariable=self.password_var, width=28,
                                font=('Segoe UI', 11), show='\u2022')
        self.pw_entry.pack(side=tk.LEFT)
        
        self.show_pw = tk.BooleanVar()
        tk.Checkbutton(row2, text="Show", variable=self.show_pw, bg=COLOR_CARD,
                      fg=COLOR_TEXT_DIM, selectcolor=COLOR_BG, activebackground=COLOR_CARD,
                      command=lambda: self.pw_entry.config(show='' if self.show_pw.get() else '\u2022')
                      ).pack(side=tk.LEFT, padx=10)
        
        tk.Label(content, 
                text="*WiFi only connects briefly to fetch Garmin stats at each refresh interval.",
                bg=COLOR_CARD, fg=COLOR_TEXT_DIM, font=('Segoe UI', 9),
                justify=tk.CENTER).pack(pady=(20, 0))
    
    # ==================== STEP 3: GARMIN MIDDLEWARE ====================
    def show_strava_step(self):
        content = tk.Frame(self.content_frame, bg=COLOR_CARD, padx=30, pady=25)
        content.pack(fill=tk.BOTH, expand=True)
        
        tk.Label(content, text="Connect Garmin Middleware",
                font=('Segoe UI', 16, 'bold'), bg=COLOR_CARD, fg=COLOR_ACCENT).pack(pady=(0, 8))

        tk.Label(content, text="Log in through your middleware. Ibis stores only the short board token.",
                font=SUBTITLE_FONT, bg=COLOR_CARD, fg=SUBTITLE_COLOR).pack(pady=(0, 5))

        form = tk.Frame(content, bg=COLOR_CARD)
        form.pack()
        
        row1 = tk.Frame(form, bg=COLOR_CARD)
        row1.pack(fill=tk.X, pady=5)
        tk.Label(row1, text="Middleware URL:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=16, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row1, textvariable=self.middleware_url_var, width=32, font=('Segoe UI', 11)).pack(side=tk.LEFT)
        
        row2 = tk.Frame(form, bg=COLOR_CARD)
        row2.pack(fill=tk.X, pady=5)
        tk.Label(row2, text="App Key:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=16, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row2, textvariable=self.middleware_app_key_var, width=32,
                font=('Segoe UI', 11), show='\u2022').pack(side=tk.LEFT)

        row3 = tk.Frame(form, bg=COLOR_CARD)
        row3.pack(fill=tk.X, pady=5)
        tk.Label(row3, text="Garmin Email:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=16, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row3, textvariable=self.garmin_email_var, width=32, font=('Segoe UI', 11)).pack(side=tk.LEFT)

        row4 = tk.Frame(form, bg=COLOR_CARD)
        row4.pack(fill=tk.X, pady=5)
        tk.Label(row4, text="Garmin Password:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=16, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row4, textvariable=self.garmin_password_var, width=32,
                font=('Segoe UI', 11), show='\u2022').pack(side=tk.LEFT)

        row5 = tk.Frame(form, bg=COLOR_CARD)
        row5.pack(fill=tk.X, pady=5)
        tk.Label(row5, text="MFA Code:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=16, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row5, textvariable=self.garmin_mfa_var, width=14, font=('Segoe UI', 11)).pack(side=tk.LEFT)
        tk.Label(row5, text="only when Garmin asks", bg=COLOR_CARD, fg=COLOR_TEXT_DIM,
                font=('Segoe UI', 9)).pack(side=tk.LEFT, padx=(10, 0))

        token_section = tk.Frame(content, bg=COLOR_CARD)
        token_section.pack(pady=(12, 0))

        tk.Button(token_section, text="Connect Garmin", command=self.save_strava,
                 bg=COLOR_BTN_SUCCESS, fg=COLOR_BTN_TEXT, font=('Segoe UI', 12, 'bold'),
                 relief=tk.FLAT, padx=25, pady=8, cursor='hand2', highlightthickness=0).pack()

        if self.garmin_mfa_token:
            tk.Label(token_section, text="MFA required. Enter the code and click Save Garmin.",
                    bg=COLOR_CARD, fg=COLOR_ACCENT, font=('Segoe UI', 10, 'bold')).pack(pady=(8, 0))
        elif self.ibis_token_var.get():
            tk.Label(token_section, text="\u2713 Ibis has a Garmin board token", bg=COLOR_CARD,
                    fg=COLOR_SUCCESS, font=('Segoe UI', 11, 'bold')).pack(pady=(8, 0))

        tk.Label(content, text="Old Strava settings are still loaded for hidden debug compatibility.",
                bg=COLOR_CARD, fg=COLOR_TEXT_DIM, font=('Segoe UI', 9)).pack(pady=(10, 0))
    
    # ==================== STEP 4: PERSONALIZE ====================
    def show_settings_step(self):
        content = tk.Frame(self.content_frame, bg=COLOR_CARD, padx=30, pady=15)
        content.pack(fill=tk.BOTH, expand=True)

        tk.Label(content, text=f"Personalize Your Dashboard",
                font=('Segoe UI', 16, 'bold'), bg=COLOR_CARD, fg=COLOR_ACCENT).pack(pady=(0, 4))

        tk.Label(content, text="Personalize your dashboard, set your name, sport, and goals.",
                font=SUBTITLE_FONT, bg=COLOR_CARD, fg=SUBTITLE_COLOR).pack(pady=(0, 10))

        form = tk.Frame(content, bg=COLOR_CARD)
        form.pack()
        
        row1 = tk.Frame(form, bg=COLOR_CARD)
        row1.pack(fill=tk.X, pady=3)
        tk.Label(row1, text="Your Name:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=18, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row1, textvariable=self.name_var, width=22, font=('Segoe UI', 11)).pack(side=tk.LEFT)

        tk.Label(form, text=f"Leave blank to display 'GARMIN STATS' as the title",
                bg=COLOR_CARD, fg=COLOR_TEXT_DIM, font=('Segoe UI', 9)).pack(anchor='w', pady=(0, 3))

        row2 = tk.Frame(form, bg=COLOR_CARD)
        row2.pack(fill=tk.X, pady=3)
        tk.Label(row2, text="Sport 1:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=18, anchor='w').pack(side=tk.LEFT)
        ttk.Combobox(row2, textvariable=self.sport_var, values=SPORT_TYPES,
                    state='readonly', width=19).pack(side=tk.LEFT)

        row3 = tk.Frame(form, bg=COLOR_CARD)
        row3.pack(fill=tk.X, pady=3)
        tk.Label(row3, text="Goal 1 (km):", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=18, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row3, textvariable=self.goal_var, width=12, font=('Segoe UI', 11)).pack(side=tk.LEFT)

        row3b = tk.Frame(form, bg=COLOR_CARD)
        row3b.pack(fill=tk.X, pady=3)
        tk.Label(row3b, text="Sport 2 (optional):", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=18, anchor='w').pack(side=tk.LEFT)
        ttk.Combobox(row3b, textvariable=self.sport2_var, values=[""] + SPORT_TYPES,
                    state='readonly', width=19).pack(side=tk.LEFT)

        row3c = tk.Frame(form, bg=COLOR_CARD)
        row3c.pack(fill=tk.X, pady=3)
        tk.Label(row3c, text="Goal 2 (km):", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=18, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row3c, textvariable=self.goal2_var, width=12, font=('Segoe UI', 11)).pack(side=tk.LEFT)

        tk.Label(form, text="Leave Sport 2 empty for single-sport mode.",
                bg=COLOR_CARD, fg=COLOR_TEXT_DIM, font=('Segoe UI', 9)).pack(anchor='w', pady=(0, 3))

        row4 = tk.Frame(form, bg=COLOR_CARD)
        row4.pack(fill=tk.X, pady=3)
        tk.Label(row4, text="Tracking Period:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=18, anchor='w').pack(side=tk.LEFT)
        ttk.Combobox(row4, textvariable=self.period_var, values=TRACK_PERIODS,
                    state='readonly', width=19).pack(side=tk.LEFT)
        
        row5 = tk.Frame(form, bg=COLOR_CARD)
        row5.pack(fill=tk.X, pady=3)
        tk.Label(row5, text="Refresh Interval:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=18, anchor='w').pack(side=tk.LEFT)
        combo = ttk.Combobox(row5, textvariable=self.refresh_var,
                            values=[o[0] for o in REFRESH_OPTIONS], state='readonly', width=14)
        combo.pack(side=tk.LEFT)
        combo.bind('<<ComboboxSelected>>', self.update_battery)
        
        tk.Label(row5, textvariable=self.battery_var, bg=COLOR_CARD,
                fg=COLOR_SUCCESS, font=('Segoe UI', 10, 'bold')).pack(side=tk.LEFT, padx=(10, 0))

        row6 = tk.Frame(form, bg=COLOR_CARD)
        row6.pack(fill=tk.X, pady=3)
        tk.Label(row6, text="Google Maps Key:", bg=COLOR_CARD, fg=COLOR_TEXT,
                font=('Segoe UI', 11), width=18, anchor='w').pack(side=tk.LEFT)
        tk.Entry(row6, textvariable=self.maps_api_key_var, width=22, font=('Segoe UI', 11)).pack(side=tk.LEFT)

        tk.Label(form, text="Optional. Shows a real map image for your route instead of a simple line drawing.",
                bg=COLOR_CARD, fg=COLOR_TEXT_DIM, font=('Segoe UI', 9)).pack(anchor='w', pady=(0, 5))

    # ==================== STEP 5: OPTIONS ====================
    def show_options_step(self):
        content = tk.Frame(self.content_frame, bg=COLOR_CARD, padx=30, pady=25)
        content.pack(fill=tk.BOTH, expand=True)
        
        tk.Label(content, text=" Delete Data From Board ",
                font=('Segoe UI', 16, 'bold'), bg=COLOR_CARD, fg=COLOR_DANGER).pack(pady=(0, 8))
        
        tk.Label(content, text="This will erase your WiFi credentials, Garmin token,\nand all personal settings from the board.",
                font=SUBTITLE_FONT, bg=COLOR_CARD, fg=SUBTITLE_COLOR,
                justify=tk.CENTER).pack(pady=(0, 15))
        
        tk.Button(content, text="Delete Data", command=self.wipe_config,
                 bg=COLOR_BTN_DANGER, fg=COLOR_BTN_TEXT,
                 font=('Segoe UI', 11, 'bold'), relief=tk.FLAT,
                 padx=25, pady=10, cursor='hand2', highlightthickness=0,
                 activebackground=COLOR_BTN_DANGER_HOVER).pack()
    
    # ==================== CUSTOM STYLED POPUP ====================
    def _center_on_parent(self, window, w, h):
        """Center a window over the main window (not the screen)"""
        self.root.update_idletasks()
        px = self.root.winfo_x()
        py = self.root.winfo_y()
        pw = self.root.winfo_width()
        ph = self.root.winfo_height()
        x = px + (pw - w) // 2
        y = py + (ph - h) // 2
        window.geometry(f"{w}x{h}+{x}+{y}")
    
    def show_popup(self, title, message, popup_type="info", yes_no=False):
        """Show a styled popup that matches the app theme - centered on parent"""
        popup = tk.Toplevel(self.root)
        popup.title(title)
        
        line_count = message.count('\n') + 1
        if line_count > 6 or len(message) > 300:
            popup_width = 500
            popup_height = 400
        else:
            popup_width = 450
            popup_height = 310
        
        popup.resizable(False, False)
        popup.transient(self.root)
        popup.grab_set()
        
        self._center_on_parent(popup, popup_width, popup_height)
        
        popup.configure(bg=COLOR_LOADING_BG)
        
        center = tk.Frame(popup, bg=COLOR_LOADING_BG)
        center.place(relx=0.5, rely=0.5, anchor='center')
        
        if popup_type == "error":
            title_color = COLOR_DANGER
        elif popup_type == "warning":
            title_color = COLOR_ACCENT
        elif popup_type == "success":
            title_color = COLOR_SUCCESS
        else:
            title_color = COLOR_ACCENT
        
        tk.Label(center, text=title, font=('Segoe UI', 14, 'bold'),
                bg=COLOR_LOADING_BG, fg=title_color).pack(pady=(0, 15))
        
        tk.Label(center, text=message, font=('Segoe UI', 11),
                bg=COLOR_LOADING_BG, fg=COLOR_TEXT, wraplength=popup_width - 60, justify=tk.CENTER).pack(pady=(0, 25))
        
        self.popup_result = False
        
        def on_yes():
            self.popup_result = True
            popup.destroy()
        
        def on_no():
            self.popup_result = False
            popup.destroy()
        
        def on_ok():
            popup.destroy()
        
        btn_frame = tk.Frame(center, bg=COLOR_LOADING_BG)
        btn_frame.pack()
        
        if yes_no:
            tk.Button(btn_frame, text="Yes", command=on_yes,
                     bg=COLOR_BTN_SUCCESS, fg=COLOR_BTN_TEXT, font=('Segoe UI', 10, 'bold'),
                     relief=tk.FLAT, padx=25, pady=8, cursor='hand2', highlightthickness=0
                     ).pack(side=tk.LEFT, padx=10)
            tk.Button(btn_frame, text="No", command=on_no,
                     bg=COLOR_BTN_SECONDARY, fg=COLOR_BTN_TEXT, font=('Segoe UI', 10, 'bold'),
                     relief=tk.FLAT, padx=25, pady=8, cursor='hand2', highlightthickness=0
                     ).pack(side=tk.LEFT, padx=10)
        else:
            tk.Button(btn_frame, text="OK", command=on_ok,
                     bg=COLOR_BTN_PRIMARY, fg=COLOR_BTN_TEXT, font=('Segoe UI', 10, 'bold'),
                     relief=tk.FLAT, padx=30, pady=8, cursor='hand2', highlightthickness=0
                     ).pack()
        
        popup.wait_window()
        return self.popup_result
    
    # ==================== SETUP COMPLETE POPUP ====================
    def show_setup_complete_popup(self):
        """Special popup after successful setup - Close app or Go back"""
        popup = tk.Toplevel(self.root)
        popup.title(f"{F} Setup Complete! {F}")
        
        popup_width = 480
        popup_height = 340
        
        popup.resizable(False, False)
        popup.transient(self.root)
        popup.grab_set()
        
        self._center_on_parent(popup, popup_width, popup_height)
        
        popup.configure(bg=COLOR_LOADING_BG)
        
        center = tk.Frame(popup, bg=COLOR_LOADING_BG)
        center.place(relx=0.5, rely=0.5, anchor='center')
        
        tk.Label(center, text="Setup Complete!",
                font=('Segoe UI', 18, 'bold'),
                bg=COLOR_LOADING_BG, fg=COLOR_SUCCESS).pack(pady=(0, 15))
        
        tk.Label(center, text="The dashboard is now correctly displaying\nyour Garmin stats! You can now:",
                font=('Segoe UI', 11),
                bg=COLOR_LOADING_BG, fg=COLOR_TEXT, justify=tk.CENTER).pack(pady=(0, 20))
        
        btn_frame = tk.Frame(center, bg=COLOR_LOADING_BG)
        btn_frame.pack()
        
        def close_app():
            popup.destroy()
            self.root.destroy()
        
        close_btn = tk.Button(btn_frame, text="\u2716  Close Ibis Setup  \u2716", command=close_app,
                             bg=COLOR_BTN_DANGER, fg=COLOR_BTN_TEXT, 
                             font=('Segoe UI', 12, 'bold'),
                             relief=tk.FLAT, padx=20, pady=12, cursor='hand2', highlightthickness=0,
                             activebackground=COLOR_BTN_DANGER_HOVER)
        close_btn.pack(pady=(0, 12), fill=tk.X)
        
        def go_back():
            popup.destroy()
            self.show_step(self.current_step)
        
        back_btn = tk.Button(btn_frame, text=f"{F}  Go back to Ibis Setup  {F}", command=go_back,
                            bg=COLOR_BTN_SECONDARY, fg=COLOR_BTN_TEXT,
                            font=('Segoe UI', 11, 'bold'),
                            relief=tk.FLAT, padx=20, pady=10, cursor='hand2', highlightthickness=0,
                            activebackground=COLOR_BTN_SECONDARY_HOVER)
        back_btn.pack(fill=tk.X)
    
    def update_battery(self, event=None):
        for name, val, est in REFRESH_OPTIONS:
            if name == self.refresh_var.get():
                self.battery_var.set(est)
                break
    
    def get_goal_value(self):
        goal_str = self.goal_var.get().strip()
        if not goal_str:
            return 1000.0
        try:
            return float(goal_str)
        except ValueError:
            return None

    def has_middleware_credentials(self):
        return bool(
            self.middleware_url_var.get().strip()
            and self.middleware_app_key_var.get().strip()
            and self.ibis_token_var.get().strip()
        )

    def has_legacy_strava_credentials(self):
        return bool(
            self.client_id_var.get().strip()
            and self.client_secret_var.get().strip()
            and self.refresh_token_var.get().strip()
        )

    def has_dashboard_credentials(self):
        return self.has_middleware_credentials() or self.has_legacy_strava_credentials()

    def build_board_config(self):
        config = {
            'ssid': self.ssid_var.get().strip(),
            'password': self.password_var.get(),
            'name': self.name_var.get().strip(),
            'title': '',
            'dataSource': 'garmin_middleware' if self.has_middleware_credentials() else 'strava',
            'middlewareUrl': self.middleware_url_var.get().strip(),
            'middlewareAppKey': self.middleware_app_key_var.get().strip(),
            'ibisToken': self.ibis_token_var.get().strip(),
            'clientID': self.client_id_var.get().strip(),
            'clientSecret': self.client_secret_var.get().strip(),
            'refreshToken': self.refresh_token_var.get().strip(),
            'mapsApiKey': self.maps_api_key_var.get().strip(),
            'sport': self.sport_var.get(),
            'sport2': self.sport2_var.get(),
            'goal2': float(self.goal2_var.get()) if self.goal2_var.get().strip() else 0,
            'goal': self.get_goal_value() or 1000,
            'trackPeriod': TRACK_PERIODS.index(self.period_var.get())
        }
        for n, v, _ in REFRESH_OPTIONS:
            if n == self.refresh_var.get():
                config['refreshHours'] = v
                break
        return config

    def middleware_base_url(self):
        url = self.middleware_url_var.get().strip().rstrip('/')
        if url and not url.startswith(('http://', 'https://')):
            url = 'https://' + url
        return url

    def middleware_json(self, method, path, body=None, bearer_token=None, timeout=60, app_key=True):
        base_url = self.middleware_base_url()
        if not base_url:
            raise ValueError("Middleware URL is missing")

        data = json.dumps(body).encode('utf-8') if body is not None else None
        headers = {"Accept": "application/json"}
        if body is not None:
            headers["Content-Type"] = "application/json"
        if app_key:
            headers["X-App-Key"] = self.middleware_app_key_var.get().strip()
        if bearer_token:
            headers["Authorization"] = f"Bearer {bearer_token}"

        req = urllib.request.Request(base_url + path, data=data, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                raw = resp.read().decode('utf-8')
                return json.loads(raw) if raw else {}
        except urllib.error.HTTPError as e:
            detail = e.read().decode('utf-8', errors='ignore')
            try:
                parsed = json.loads(detail)
                detail = parsed.get('detail') or detail
            except Exception:
                pass
            raise RuntimeError(f"Middleware HTTP {e.code}: {detail}") from e

    def provision_garmin_middleware(self):
        if self.has_middleware_credentials():
            return True

        if not self.middleware_url_var.get().strip():
            raise ValueError("Enter your middleware URL first")
        if not self.middleware_app_key_var.get().strip():
            raise ValueError("Enter your middleware app key first")

        self.update_loading("Checking middleware...")
        self.middleware_json("GET", "/api/ping", timeout=15, app_key=False)

        if not self.garmin_session_token:
            if self.garmin_mfa_token:
                code = self.garmin_mfa_var.get().strip()
                if not code:
                    return False
                self.update_loading("Sending Garmin MFA code...")
                result = self.middleware_json(
                    "POST",
                    "/api/mfa",
                    body={"mfaToken": self.garmin_mfa_token, "code": code},
                    timeout=60,
                )
                self.garmin_mfa_token = ""
                self.garmin_mfa_var.set("")
            else:
                email = self.garmin_email_var.get().strip()
                password = self.garmin_password_var.get()
                if not email or not password:
                    raise ValueError("Enter your Garmin email and password to register this board")
                self.update_loading("Logging in to Garmin...")
                result = self.middleware_json(
                    "POST",
                    "/api/login",
                    body={"email": email, "password": password},
                    timeout=90,
                )
                if result.get("status") == "mfa_required":
                    self.garmin_mfa_token = result.get("mfaToken", "")
                    return False

            self.garmin_session_token = result.get("token", "")
            if not self.garmin_session_token:
                raise RuntimeError("Middleware login did not return a Garmin session token")

        self.update_loading("Registering board token...")
        registered = self.middleware_json(
            "POST",
            "/api/ibis/register",
            bearer_token=self.garmin_session_token,
            timeout=60,
        )
        ibis_token = registered.get("ibisToken", "")
        if not ibis_token:
            raise RuntimeError("Middleware did not return an Ibis token")
        self.ibis_token_var.set(ibis_token)
        self.garmin_password_var.set("")
        self.garmin_session_token = ""
        return True
    
    # ==================== NAVIGATION ====================
    def go_back(self):
        if self.current_step > 0:
            self.show_step(self.current_step - 1)
    
    def go_next(self):
        if self.current_step == 0:
            if not self.connected:
                self.show_popup(f"{F} Not Connected", "Please connect to your board first!", popup_type="warning")
                return
            self.show_step(1)
        elif self.current_step == 1:
            if not self.ssid_var.get().strip():
                self.show_popup(f"{F} Missing WiFi", "Please enter your WiFi name!", popup_type="warning")
                return
            self.save_wifi()
        elif self.current_step == 2:
            self.save_strava()
        elif self.current_step == 3:
            self.finish_setup()
    
    # ==================== LOADING OVERLAY ====================
    def show_loading(self, title, message, show_dashboard_msg=False):
        if self.loading_overlay:
            self.loading_overlay.destroy()
        
        self.loading_overlay = tk.Toplevel(self.root)
        self.loading_overlay.title(title)
        self.loading_overlay.geometry(f"{LOADING_WIDTH}x{LOADING_HEIGHT}")
        self.loading_overlay.resizable(False, False)
        self.loading_overlay.transient(self.root)
        self.loading_overlay.grab_set()
        
        self._center_on_parent(self.loading_overlay, LOADING_WIDTH, LOADING_HEIGHT)
        
        self.loading_overlay.configure(bg=COLOR_LOADING_BG)
        
        center = tk.Frame(self.loading_overlay, bg=COLOR_LOADING_BG)
        center.place(relx=0.5, rely=0.5, anchor='center')
        
        tk.Label(center, text=title, font=('Segoe UI', 16, 'bold'),
                bg=COLOR_LOADING_BG, fg=COLOR_ACCENT).pack(pady=(0, 15))
        
        self.feather_label = tk.Label(center, text=F, font=('Segoe UI', 12),
                                      bg=COLOR_LOADING_BG, fg=COLOR_ACCENT)
        self.feather_label.pack(pady=(0, 12))
        
        self.loading_msg = tk.Label(center, text=message, font=('Segoe UI', 11),
                                   bg=COLOR_LOADING_BG, fg=COLOR_SUCCESS, wraplength=400)
        self.loading_msg.pack(pady=(0, 8))
        
        # Only show the dashboard message for Finish Setup
        if show_dashboard_msg:
            tk.Label(center, text="This window will close once your dashboard\nsuccessfully displays your stats",
                    font=('Segoe UI', 11), bg=COLOR_LOADING_BG, fg=COLOR_TEXT,
                    justify=tk.CENTER).pack()
        
        self.grow_feathers()
        # Don't call update() here - it blocks the animation loop
        self.loading_overlay.update_idletasks()
    
    def grow_feathers(self):
        try:
            if not self.loading_overlay or not self.loading_overlay.winfo_exists():
                return
            
            current = self.feather_label.cget('text')
            # Keep adding feathers up to 15, then cycle back to 1
            # This ensures continuous animation that stays within the box
            if len(current) < 15:
                self.feather_label.config(text=current + F)
            else:
                self.feather_label.config(text=F)
            
            # Always reschedule the next animation frame
            self.loading_animation_id = self.loading_overlay.after(150, self.grow_feathers)
        except Exception as e:
            print(f"Animation error (continuing): {e}")
            # Even on error, try to reschedule
            if self.loading_overlay and self.loading_overlay.winfo_exists():
                self.loading_animation_id = self.loading_overlay.after(150, self.grow_feathers)
    
    def update_loading(self, message):
        if self.loading_overlay and self.loading_overlay.winfo_exists():
            self.loading_msg.config(text=message)
            # Use update_idletasks instead of update to avoid blocking the animation
            self.loading_overlay.update_idletasks()
            # Ensure animation is still running
            if not self.loading_animation_id or self.loading_animation_id not in self.loading_overlay.tk.call('after', 'info'):
                self.grow_feathers()
    
    def hide_loading(self):
        if self.loading_animation_id and self.loading_overlay:
            try:
                self.loading_overlay.after_cancel(self.loading_animation_id)
            except:
                pass
        if self.loading_overlay:
            self.loading_overlay.destroy()
            self.loading_overlay = None
    
    def get_funny_message(self, category):
        if category in FUNNY_MESSAGES:
            return random.choice(FUNNY_MESSAGES[category])
        return "Ibis is doing ibis things..."
    
    # ==================== SERIAL COMMUNICATION ====================
    def scan_ports(self):
        ports = [f"{p.device} - {p.description}" for p in serial.tools.list_ports.comports()]
        if hasattr(self, 'port_combo'):
            self.port_combo['values'] = ports
            if ports:
                self.port_combo.current(0)
    
    def toggle_connection(self):
        if self.connected:
            self.disconnect()
        else:
            self.connect()
    
    def connect(self):
        port_str = self.port_var.get()
        if not port_str:
            self.show_popup(f"{F} No Port", "Please select a USB port!", popup_type="warning")
            return
        
        self.show_loading(f"{F} Connecting {F}", self.get_funny_message('connect'))
        
        try:
            port = port_str.split(' - ')[0]
            self.serial_conn = serial.Serial(port, BAUD_RATE, timeout=SERIAL_TIMEOUT,
                                            write_timeout=WRITE_TIMEOUT)
            time.sleep(2)
            self.serial_conn.reset_input_buffer()
            
            self.update_loading(self.get_funny_message('connect'))
            
            response = self.send_command("PING", wait_for="PONG")
            if response and "PONG" in response:
                self.connected = True
                self.update_connect_ui()
                
                self.update_loading(self.get_funny_message('load_config'))
                self.auto_load_config()
                
                self.hide_loading()
                
                # Auto-advance after successful connect
                if self.wifi_complete and self.strava_complete:
                    self.show_step(3)  # Everything set up -> Personalize
                elif self.wifi_complete:
                    self.show_step(2)  # WiFi done -> Garmin
                else:
                    self.show_step(1)  # Fresh board -> WiFi
            else:
                raise Exception("No response from board")
        except Exception as e:
            self.hide_loading()
            self.show_popup(f"{F} Connection Failed", f"Could not connect:\n{e}\n\nMake sure board is in setup mode!", popup_type="error")
            if self.serial_conn:
                self.serial_conn.close()
                self.serial_conn = None
    
    def auto_load_config(self):
        response = self.send_command("GET_CONFIG", wait_for="OK")
        
        if not response:
            return
        
        try:
            json_text = self.extract_json_object(response)
            if json_text:
                c = json.loads(json_text)
                if c.get('error'):
                    print(f"Board config warning: {c.get('error')}; using locally saved settings.")
                    return
                board_has_config = bool(
                    c.get('configured') or c.get('hasDashboard') or c.get('hasStrava') or
                    c.get('ssid') or c.get('middlewareUrl') or c.get('ibisToken') or
                    c.get('clientID') or c.get('mapsApiKey') or c.get('name')
                )
                self.apply_cached_config(c, allow_blank=board_has_config)
                if board_has_config:
                    self.save_local_config()
                self.update_step_indicator()
        except Exception as e:
            print(f"Board config could not be parsed ({e}); using locally saved settings.")

    def extract_json_object(self, text):
        """Return the first balanced JSON object from a noisy serial response."""
        start = text.find('{')
        if start < 0:
            return None

        depth = 0
        in_string = False
        escaped = False
        for i in range(start, len(text)):
            ch = text[i]
            if in_string:
                if escaped:
                    escaped = False
                elif ch == '\\':
                    escaped = True
                elif ch == '"':
                    in_string = False
            else:
                if ch == '"':
                    in_string = True
                elif ch == '{':
                    depth += 1
                elif ch == '}':
                    depth -= 1
                    if depth == 0:
                        return text[start:i + 1]
        return None
    
    def disconnect(self):
        if self.serial_conn:
            self.serial_conn.close()
            self.serial_conn = None
        self.connected = False
        self.wifi_complete = False
        self.strava_complete = False
        self.update_connect_ui()
        self.update_step_indicator()
    
    def update_connect_ui(self):
        try:
            if self.connected:
                self.conn_status.config(text=f"\u2713 Connected {F}", fg=COLOR_CONNECTED)
                self.connect_btn.config(text="Disconnect", bg=COLOR_BTN_SUCCESS)
                self.next_btn.config(bg=COLOR_BTN_SUCCESS, activebackground=COLOR_BTN_SUCCESS_HOVER)
            else:
                self.conn_status.config(text="\u2716 Not connected", fg=COLOR_DISCONNECTED)
                self.connect_btn.config(text="Connect", bg=COLOR_BTN_PRIMARY)
                self.next_btn.config(bg=COLOR_BTN_PRIMARY, activebackground=COLOR_BTN_PRIMARY_HOVER)
        except:
            pass
    
    def send_command(self, command, wait_for=None, timeout=SERIAL_TIMEOUT):
        if not self.serial_conn:
            return None
        
        max_retries = 3
        for attempt in range(max_retries):
            try:
                self.serial_conn.reset_input_buffer()
                self.serial_conn.reset_output_buffer()
                time.sleep(0.2)
                
                cmd_bytes = f"{command}\n".encode('utf-8')
                
                for i in range(0, len(cmd_bytes), 32):
                    self.serial_conn.write(cmd_bytes[i:i+32])
                    self.serial_conn.flush()
                    time.sleep(0.05)
                
                time.sleep(0.5)
                
                response = ""
                start_time = time.time()
                while time.time() - start_time < timeout:
                    # Process Tkinter events to keep animations running
                    try:
                        self.root.update()
                    except:
                        pass
                    
                    if self.serial_conn.in_waiting > 0:
                        response += self.serial_conn.read(self.serial_conn.in_waiting).decode('utf-8', errors='ignore')
                        if wait_for and wait_for in response:
                            time.sleep(0.3)
                            if self.serial_conn.in_waiting > 0:
                                response += self.serial_conn.read(self.serial_conn.in_waiting).decode('utf-8', errors='ignore')
                            break
                        if any(x in response for x in ["OK", "SUCCESS", "WIPED", "ERROR", "FAILED"]):
                            if not wait_for:
                                break
                    time.sleep(0.05)
                
                if response:
                    return response
                    
            except (PermissionError, OSError, serial.SerialException) as e:
                print(f"Connection lost or port busy (attempt {attempt+1}/{max_retries}): {e}")
                # Connection lost - update UI
                if self.connected and (isinstance(e, OSError) or isinstance(e, serial.SerialException)):
                    print("Board disconnected!")
                    self.root.after(0, self.handle_disconnection)
                    return None
                time.sleep(1)
                if attempt < max_retries - 1:
                    continue
                else:
                    print(f"Send error after {max_retries} attempts: {e}")
                    return None
            except Exception as e:
                print(f"Send error: {e}")
                if attempt < max_retries - 1:
                    time.sleep(0.5)
                    continue
                return None
        
        return None
    
    def handle_disconnection(self):
        """Handle board disconnection - update UI to show disconnected state"""
        if self.serial_conn:
            try:
                self.serial_conn.close()
            except:
                pass
            self.serial_conn = None
        
        self.connected = False
        self.update_connect_ui()
        self.update_step_indicator()
        
        # Hide any loading overlay
        if self.loading_overlay:
            self.hide_loading()
        
        # Show error popup
        self.show_popup(f"{F} Connection Lost", 
                       "Board disconnected!\n\nPlease reconnect and try again.",
                       popup_type="error")
    
    # ==================== WIPE ====================
    def wipe_config(self):
        if not self.connected:
            self.show_popup(f"{F} Not Connected", "Please connect to your board first!", popup_type="warning")
            return
        
        if not self.show_popup(f"{F} Confirm Delete", 
                "Are you sure you want to erase ALL settings?\n\n"
                "This will remove:\n"
                "\u2022 WiFi credentials\n"
                "\u2022 Garmin middleware token\n"
                "\u2022 All personalization\n\n"
                "This cannot be undone!\n\n"
                "(This may take up to a minute)",
                popup_type="warning", yes_no=True):
            return
        
        self.show_loading(f"{F} Deleting Data {F}", self.get_funny_message('wipe'))
        
        response = self.send_command("DELETE_DATA", wait_for="SETUP_SCREEN_DRAWN", timeout=60)
        
        if response and "WIPED" in response:
            if self._cache_save_after_id:
                try:
                    self.root.after_cancel(self._cache_save_after_id)
                except Exception:
                    pass
                self._cache_save_after_id = None
            self._suspend_cache_save = True
            self.ssid_var.set('')
            self.password_var.set('')
            self.name_var.set('')
            self.client_id_var.set('')
            self.client_secret_var.set('')
            self.refresh_token_var.set('')
            self.middleware_url_var.set('')
            self.middleware_app_key_var.set('')
            self.ibis_token_var.set('')
            self.garmin_email_var.set('')
            self.garmin_password_var.set('')
            self.garmin_mfa_var.set('')
            self.garmin_session_token = ""
            self.garmin_mfa_token = ""
            self.maps_api_key_var.set('')
            self.sport_var.set('Run')
            self.sport2_var.set('')
            self.goal2_var.set('')
            self.goal_var.set('1000')
            self.period_var.set('Yearly')
            self.refresh_var.set('Once a day')
            self.battery_var.set('~2 months battery')
            self._suspend_cache_save = False
            
            self.wifi_complete = False
            self.strava_complete = False
            self.setup_done = False
            self.update_step_indicator()
            
            self.hide_loading()
            self.show_popup(f"{F} Data Deleted", "All settings erased!\n\nBoard is ready for fresh setup.", popup_type="success")
        else:
            self.hide_loading()
            self.show_popup(f"{F} Error", "Failed to delete data from board.", popup_type="error")
    
    # ==================== SAVE FUNCTIONS ====================
    def save_wifi(self):
        self.show_loading(f"{F} Saving WiFi {F}", self.get_funny_message('save_config'))
        
        config = self.build_board_config()

        self.save_local_config()
        response = self.send_command(f"SET_CONFIG:{json.dumps(config, separators=(',', ':'))}", wait_for="SUCCESS", timeout=10)

        if not response or "SUCCESS" not in response:
            self.hide_loading()
            self.show_popup(f"{F} Error", "Failed to save configuration.", popup_type="error")
            return
        
        self.update_loading(self.get_funny_message('test_wifi'))
        
        wifi_response = self.send_command("TEST_WIFI", wait_for="WIFI_", timeout=25)
        
        self.hide_loading()
        
        if not wifi_response or "WIFI_OK" not in wifi_response:
            self.show_popup(f"{F} WiFi Failed", "WiFi connection failed!\n\nPlease check your credentials.", popup_type="warning")
            return
        
        self.wifi_complete = True
        self.update_step_indicator()
        
        # Auto-advance
        if self.strava_complete:
            self.show_popup(f"{F} WiFi Updated", "WiFi credentials saved and tested!", popup_type="success")
            self.show_step(3)  # Garmin already done -> Personalize
        else:
            self.show_popup(f"{F} WiFi Saved", "WiFi credentials saved and tested!\n\nNow let's connect Garmin.", popup_type="success")
            self.show_step(2)  # Next: Garmin
    
    def save_strava(self):
        self.show_loading(f"{F} Saving Garmin {F}", self.get_funny_message('save_config'))

        try:
            registered = self.provision_garmin_middleware()
        except Exception as e:
            self.hide_loading()
            self.show_popup(f"{F} Garmin Setup Failed", str(e), popup_type="error")
            return

        if not registered:
            self.hide_loading()
            self.show_popup(
                f"{F} Garmin MFA Required",
                "Garmin asked for a multi-factor code.\n\nEnter the code in the MFA field and click Save Garmin again.",
                popup_type="warning",
            )
            self.show_step(2)
            return

        config = self.build_board_config()

        self.save_local_config()
        response = self.send_command(f"SET_CONFIG:{json.dumps(config, separators=(',', ':'))}", wait_for="SUCCESS", timeout=15)
        
        self.hide_loading()
        
        if not response or "SUCCESS" not in response:
            if response and ("Configuration saved" in response or "saved" in response.lower() or "OK" in response):
                self.strava_complete = True
                self.update_step_indicator()
                self.show_popup(f"{F} Garmin Saved", 
                               "Garmin middleware token saved!\n\n(Minor communication glitch ignored)", 
                               popup_type="success")
                self.show_step(3)
            else:
                self.show_popup(f"{F} Save Error", 
                               "Communication error while saving.\n\nYou can:\n\u2022 Try 'Save Garmin' again\n\u2022 Skip to 'Finish Setup' (it will save then)", 
                               popup_type="warning")
            return
        
        self.strava_complete = True
        self.update_step_indicator()
        
        # Auto-advance to Personalize
        self.show_popup(f"{F} Garmin Saved", "Garmin middleware token saved!", popup_type="success")
        self.show_step(3)
    
    def finish_setup(self):
        has_wifi = bool(self.ssid_var.get().strip())
        has_dashboard = self.has_dashboard_credentials()
        
        if not has_wifi or not has_dashboard:
            missing = []
            if not has_wifi:
                missing.append("WiFi")
            if not has_dashboard:
                missing.append("Garmin")
            self.show_popup(f"{F} Missing Settings", 
                           f"Please enter and save {' and '.join(missing)}\ncredentials first!",
                           popup_type="warning")
            return
        
        goal_str = self.goal_var.get().strip()
        if goal_str:
            try:
                float(goal_str)
            except ValueError:
                self.show_popup(f"{F} Invalid Goal", 
                               "Distance goal must be a number!\n\nExample: 1000",
                               popup_type="warning")
                return
        
        self.show_loading(f"{F} Finishing Setup {F}", self.get_funny_message('save_config'), show_dashboard_msg=True)
        
        config = self.build_board_config()

        self.save_local_config()
        response = self.send_command(f"SET_CONFIG:{json.dumps(config, separators=(',', ':'))}", wait_for="SUCCESS", timeout=10)

        if not response or "SUCCESS" not in response:
            self.hide_loading()
            self.show_popup(f"{F} Error {F}", "Failed to save configuration.", popup_type="error")
            return
        
        has_dashboard = bool(config.get('ibisToken') and config.get('middlewareUrl') and config.get('middlewareAppKey'))
        has_legacy_strava = bool(config['clientID'] and config['clientSecret'] and config['refreshToken'])
        has_wifi = bool(config['ssid'])
        
        if has_wifi and (has_dashboard or has_legacy_strava):
            self.update_loading(self.get_funny_message('fetch_strava'))
            fetch_command = "FETCH_DASHBOARD" if has_dashboard else "FETCH_STRAVA"
            dashboard_response = self.send_command(fetch_command, wait_for="DASHBOARD_DRAWN", timeout=90)
            
            self.hide_loading()
            
            if dashboard_response and "DASHBOARD_DRAWN" in dashboard_response:
                self.setup_done = True
                self.show_setup_complete_popup()
            else:
                self.show_popup(f"{F} Almost Done", 
                               "Settings saved but couldn't fetch Garmin data.\n\n"
                               "Try pressing BOOT button on the board\n"
                               "or click Finish Setup again.",
                               popup_type="warning")
        elif has_wifi:
            self.update_loading("Updating display...")
            self.send_command("SHOW_SETUP_SCREEN", wait_for="SETUP_SCREEN_DRAWN", timeout=60)
            
            self.hide_loading()
            self.show_popup(f"{F} WiFi Saved", 
                           "WiFi settings saved!\n\n"
                           "Add Garmin middleware settings to see your stats.",
                           popup_type="success")
        else:
            self.hide_loading()
            self.show_popup(f"{F} Missing Settings", 
                           "Please enter WiFi credentials first!",
                           popup_type="warning")
    
    # ==================== OAUTH ====================
    def start_oauth_flow(self):
        cid = self.client_id_var.get().strip()
        sec = self.client_secret_var.get().strip()
        
        if not cid or not sec:
            self.show_popup(f"{F} Missing Info", "Enter Client ID and Secret first!", popup_type="warning")
            return
        
        self.token_status_var.set(f"{F} Opening browser...")
        self.root.update()
        
        def run_server():
            try:
                handler = lambda *a, **k: OAuthCallbackHandler(*a, callback=self.oauth_callback, **k)
                with socketserver.TCPServer(("", OAUTH_REDIRECT_PORT), handler) as httpd:
                    httpd.timeout = 120
                    httpd.handle_request()
            except Exception as e:
                self.root.after(0, lambda: self.token_status_var.set(f"Error: {e}"))
        
        threading.Thread(target=run_server, daemon=True).start()
        time.sleep(0.3)
        
        auth_params = {
            'client_id': cid,
            'redirect_uri': OAUTH_REDIRECT_URI,
            'response_type': 'code',
            'scope': 'read,activity:read_all',
            'approval_prompt': 'auto'
        }
        webbrowser.open(f"{STRAVA_AUTH_URL}?{urllib.parse.urlencode(auth_params)}")
    
    def oauth_callback(self, code):
        if code:
            self.root.after(100, lambda: self.exchange_token(code))
        else:
            self.root.after(0, lambda: self.token_status_var.set("Authorization failed"))
    
    def exchange_token(self, code):
        self.token_status_var.set(f"{F} Getting token...")
        try:
            data = urllib.parse.urlencode({
                'client_id': self.client_id_var.get().strip(),
                'client_secret': self.client_secret_var.get().strip(),
                'code': code,
                'grant_type': 'authorization_code'
            }).encode()
            
            req = urllib.request.Request(STRAVA_TOKEN_URL, data=data, method='POST')
            with urllib.request.urlopen(req, timeout=30) as resp:
                result = json.loads(resp.read().decode())
                if 'refresh_token' in result:
                    self.refresh_token_var.set(result['refresh_token'])
                    self.token_status_var.set("")
                    self.show_step(2)  # Refresh to show checkmark
                else:
                    self.token_status_var.set("No token in response")
        except Exception as e:
            self.token_status_var.set(f"Error: {e}")
    
    # ==================== HELP ====================
    def show_connection_help(self):
        self.show_popup(f"{F} Connection Help",
            "1. Make sure USB cable is plugged in\n\n"
            "2. Wait for screen to finish drawing\n\n"
            "3. Stare at the board to assert dominance\n\n"
            "4. Try turning it off and on again\n\n"
            "5. ACT LED should be blinking green\n\n"
            f"6. Click '{F} Refresh' and try again")
    
    def show_strava_help(self):
        self.show_popup(f"{F} Strava Setup Guide",
            "1. Go to strava.com/settings/api and log in\n\n"
            "2. Click 'Create an App' (or 'My API Application')\n\n"
            "3. In the 'Application Name' box, type 'Ibis Dash'\n"
            "   Fill in the other fields as you please\n\n"
            "4. Set the 'Authorization Callback Domain'\n"
            "   box to: localhost and hit 'Save'\n\n"
            "5. Strava will now show your Client ID & Client Secret\n"
            "   Enter these in Ibis Setup.\n\n"
            "6. Then click 'Let Ibis In' to connect!")


def main():
    root = tk.Tk()
    app = IbisSetupWizard(root)
    root.mainloop()


if __name__ == "__main__":
    main()
