"""
Idle Easter Egg for Cloudar Browser™
Displays a fun image popup when the user is inactive for a period of time.
"""
import os
import sys
import random
import time
import subprocess
import shutil
from core.browser_qt import (QObject, QTimer, QLabel, QVBoxLayout, QHBoxLayout,
                             QWidget, QApplication, Qt, QPixmap, QSize, QPushButton,
                             QPoint, QFrame, QEvent, QtVersion)

# Cross-compatible imports for animations
if QtVersion == 6:
    from PyQt6.QtWidgets import QGraphicsOpacityEffect
    from PyQt6.QtCore import QPropertyAnimation, QUrl
else:
    from PyQt5.QtWidgets import QGraphicsOpacityEffect
    from PyQt5.QtCore import QPropertyAnimation, QUrl

# Cross-compatible import for sound playback (optional feature)
try:
    if QtVersion == 6:
        from PyQt6.QtMultimedia import QSoundEffect, QMediaPlayer, QAudioOutput, QMediaDevices
    else:
        from PyQt5.QtMultimedia import QSoundEffect, QMediaPlayer, QAudioOutput
        from PyQt5.QtMultimedia import QAudioDeviceInfo as QMediaDevices
    _SOUND_AVAILABLE = True
except ImportError as _e:
    QSoundEffect = None
    QMediaPlayer = None
    QAudioOutput = None
    QMediaDevices = None
    _SOUND_AVAILABLE = False
    print(f"[IdleEasterEgg] QtMultimedia not available, sound effects disabled: {_e}")
    print("[IdleEasterEgg] Fix: pip install PyQt6-QtMultimedia (or PyQt5-QtMultimedia)")

APP_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EASTER_EGG_DIR = os.path.join(APP_DIR, "Easter_Egg")

# Sound playback priority: bundled mpv -> system mpv -> ffplay -> Qt
# Multimedia -> silent. Nothing here is required to be pre-installed by
# the user: we ship mpv alongside Cloudar Browser under APP_DIR/mpv/<os>/,
# so playback works out of the box on both Windows and Linux. If for any
# reason no player is found at all, the browser stays completely silent
# and never surfaces an error to the user.
if sys.platform.startswith("win"):
    _PLATFORM_FOLDER = "windows"
    _MPV_EXE = "mpv.exe"
    _FFPLAY_EXE = "ffplay.exe"
elif sys.platform.startswith("darwin"):
    _PLATFORM_FOLDER = "macos"
    _MPV_EXE = "mpv"
    _FFPLAY_EXE = "ffplay"
else:
    _PLATFORM_FOLDER = "linux"
    _MPV_EXE = "mpv"
    _FFPLAY_EXE = "ffplay"


def _resolve_player_path(exe_name, bundled_subdir):
    """
    Find a media player executable, preferring the copy bundled with
    Cloudar Browser (APP_DIR/<bundled_subdir>/<platform>/<exe_name>) so
    the user never has to install anything themselves. Falls back to
    whatever is available on the system PATH.
    """
    bundled_path = os.path.join(APP_DIR, bundled_subdir, _PLATFORM_FOLDER, exe_name)
    if os.path.isfile(bundled_path) and os.access(bundled_path, os.X_OK):
        return bundled_path
    # Some archives may ship the binary without the exec bit set.
    if os.path.isfile(bundled_path):
        return bundled_path
    return shutil.which(os.path.splitext(exe_name)[0])


# Resolved once at import time. Both are optional; the app degrades
# gracefully through the fallback chain if either (or both) are missing.
MPV_PATH = _resolve_player_path(_MPV_EXE, "mpv")
FFPLAY_PATH = _resolve_player_path(_FFPLAY_EXE, "ffplay")

# Images to use for the easter egg
EASTER_EGG_IMAGES = [
    "shiroko-catoon-meo.png",
    "shiroko.png",
    "shiroko1.png",
    "shiroko2.jpg",
    "Arisu.jpg",
    "Hoshino_Uheeee.png",
    "momoi.png",
    "Yuuka.png",
]

# Idle timeout before showing the easter egg (in seconds)
DEFAULT_IDLE_TIMEOUT = 600  # 600 seconds (10 minutes) of inactivity

# Map specific easter egg images to a sound effect to play when they appear.
# Sound files are looked up first in EASTER_EGG_DIR, then in
# EASTER_EGG_DIR/sounds, whichever exists.
# NOTE: QSoundEffect is designed for short, UNCOMPRESSED audio (WAV/OGG).
# It can be unreliable decoding compressed formats like MP3 depending on
# the system's Qt Multimedia backend. Convert to WAV for reliability:
#   ffmpeg -i hoshino-uhee.mp3 -ar 44100 -ac 2 hoshino-uhee.wav
EASTER_EGG_SOUNDS = {
    "Hoshino_Uheeee.png": "hoshino-uhee.wav",
    "Yuuka.png": "yuuka.wav", 
}

# Alternate fallback sounds to try if the primary mapping cannot be played.
EASTER_EGG_SOUND_FALLBACKS = {
    "Hoshino_Uheeee.png": ["Hoshino_Uheeee.wav", "Hoshino_Uheeee.mp3"],
    "Yuuka.png": ["Yuuka.mp3", "Yuuka.wav"],
}

# Keywords used to recognize a Bluetooth audio output device by its
# description string (varies by OS/driver, so we match loosely).
BLUETOOTH_DEVICE_KEYWORDS = (
    "bluetooth", "bt", "a2dp", "headset", "hands-free", "handsfree",
    "airpods", "buds", "earbuds",
)

# Grace period after showing the egg during which we ignore activity
# events that the popup itself might generate (show/focus/paint), so it
# doesn't get closed the instant it appears.
SHOW_GRACE_PERIOD = 0.5  # seconds


class IdleEasterEgg(QObject):
    """Tracks user inactivity and shows an Easter egg popup when idle."""

    def __init__(self, browser_window):
        super().__init__(browser_window)
        self.browser = browser_window
        self._last_activity = time.time()
        self._idle_timeout = self._load_idle_timeout()
        self._active = True
        self._egg_window = None
        self._fade_animation = None
        self._showing_egg = False
        self._egg_shown_at = 0.0  # timestamp of when the egg was last shown
        self._sound_effect = None  # keep a reference so it isn't GC'd mid-playback
        self._media_player = None
        self._audio_output = None

        # Install event filter to track all user input
        self.browser.installEventFilter(self)

        # Idle check timer - runs every 5 seconds
        self._idle_timer = QTimer(self)
        self._idle_timer.timeout.connect(self._check_idle)
        self._idle_timer.start(5000)

        # Also track activity on the main QApplication
        app = QApplication.instance()
        if app:
            app.installEventFilter(self)

        # Track mouse position changes
        self._last_mouse_pos = None

        print(f"[IdleEasterEgg] Initialized. Timeout: {self._idle_timeout}s")

    def _load_idle_timeout(self):
        """Load idle timeout from settings, or use default."""
        try:
            settings = self.browser.settings
            return int(settings.get("easter_egg_idle_timeout", DEFAULT_IDLE_TIMEOUT))
        except Exception:
            return DEFAULT_IDLE_TIMEOUT

    def _is_descendant_of_egg(self, obj):
        """Check whether obj belongs to the easter egg popup window itself."""
        if self._egg_window is None:
            return False
        w = obj
        while w is not None:
            if w is self._egg_window:
                return True
            try:
                w = w.parent()
            except Exception:
                break
        return False

    def eventFilter(self, obj, event):
        """Filter events to track user activity."""
        event_type = event.type()

        # Cross-version event type detection using raw integer values
        # PyQt5 uses QEvent.MouseMove, PyQt6 uses QEvent.Type.MouseMove
        # Using raw values ensures compatibility
        activity_event_values = {
            # Mouse events
            2,   # MouseButtonPress
            3,   # MouseButtonRelease
            5,   # MouseMove
            # Keyboard events
            6,   # KeyPress
            7,   # KeyRelease
            # Focus events
            8,   # FocusIn
            9,   # FocusOut
            # Scroll
            31,  # Wheel
        }

        if event_type in activity_event_values:
            # Ignore events that originate from the easter egg popup itself
            # (e.g. FocusIn from show(), synthetic MouseMove on paint, etc.)
            # so the popup doesn't get treated as "user activity" and closed
            # the instant it appears. Real clicks on the popup are handled
            # separately via mousePressEvent -> _hide_egg().
            if not self._is_descendant_of_egg(obj):
                self._on_user_activity()

        return super().eventFilter(obj, event)

    def _on_user_activity(self):
        """Called whenever the user interacts with the browser."""
        self._last_activity = time.time()

        # Close the egg window if it's open, but only if it has been visible
        # for at least SHOW_GRACE_PERIOD seconds. This avoids a race where
        # activity events fired as a side-effect of showing the popup
        # immediately close it again.
        if self._egg_window and self._egg_window.isVisible():
            if (time.time() - self._egg_shown_at) >= SHOW_GRACE_PERIOD:
                self._hide_egg()

    def _check_idle(self):
        """Check if the user has been idle too long."""
        if not self._active:
            return

        # Only show if browser window is visible and active
        if not self.browser.isVisible():
            return

        # Don't show if already shown
        if self._showing_egg or (self._egg_window and self._egg_window.isVisible()):
            return

        # Check if enough time has passed since last activity
        idle_time = time.time() - self._last_activity
        if idle_time >= self._idle_timeout:
            self._show_egg()

    def _get_random_image_path(self):
        """Get a random image path from the Easter_Egg directory."""
        # Filter to only existing files
        available = []
        for img_name in EASTER_EGG_IMAGES:
            img_path = os.path.join(EASTER_EGG_DIR, img_name)
            if os.path.exists(img_path):
                available.append(img_path)

        if not available:
            # Fallback: list any image files in the directory
            if os.path.isdir(EASTER_EGG_DIR):
                for f in os.listdir(EASTER_EGG_DIR):
                    if f.lower().endswith(('.png', '.jpg', '.jpeg', '.gif', '.bmp')):
                        img_path = os.path.join(EASTER_EGG_DIR, f)
                        if os.path.isfile(img_path):
                            available.append(img_path)

        return random.choice(available) if available else None

    def _find_sound_path(self, sound_names):
        """Locate a sound file, checking EASTER_EGG_DIR then a sounds/ subfolder."""
        if isinstance(sound_names, str):
            sound_names = [sound_names]
        candidates = [
            os.path.join(EASTER_EGG_DIR, name)
            for name in sound_names
        ]
        candidates.extend(
            os.path.join(EASTER_EGG_DIR, "sounds", name)
            for name in sound_names
        )
        for path in candidates:
            if os.path.isfile(path):
                return path
        return None

    def _find_bluetooth_audio_device(self):
        """
        Try to find a connected Bluetooth audio output device among the
        system's available audio outputs. Returns a QAudioDevice (PyQt6) or
        None if none is found / detection isn't supported.
        """
        if QMediaDevices is None or QtVersion != 6:
            # Bluetooth device targeting via QAudioOutput.setDevice() is a
            # Qt6-only API; on PyQt5 we fall back to the system default.
            return None

        try:
            devices = QMediaDevices.audioOutputs()
        except Exception:
            return None

        for device in devices:
            try:
                description = device.description().lower()
            except Exception:
                continue
            if any(keyword in description for keyword in BLUETOOTH_DEVICE_KEYWORDS):
                return device
        return None

    def _list_audio_devices_debug(self):
        """Print all available audio output devices, for diagnostics."""
        if QMediaDevices is None or QtVersion != 6:
            return
        try:
            devices = QMediaDevices.audioOutputs()
            if not devices:
                print("[IdleEasterEgg] No audio output devices found by QMediaDevices")
                return
            print("[IdleEasterEgg] Available audio output devices:")
            for d in devices:
                try:
                    print(f"  - {d.description()}")
                except Exception:
                    pass
        except Exception as e:
            print(f"[IdleEasterEgg] Could not list audio devices: {e}")

    def _play_via_mpv(self, sound_path):
        """
        Play a sound file using the external mpv media player as a
        detached subprocess. This bypasses QtMultimedia/PipeWire device
        detection entirely, since mpv talks to the system audio backend
        directly on its own and works even when Qt reports no audio
        devices are available.
        """
        if not MPV_PATH:
            return False
        try:
            subprocess.Popen(
                [MPV_PATH, "--no-video", "--really-quiet", sound_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            print(f"[IdleEasterEgg] Playing sound via mpv: {sound_path}")
            return True
        except Exception as e:
            print(f"[IdleEasterEgg] Failed to launch mpv: {e}")
            return False

    def _play_via_ffplay(self, sound_path):
        """
        Play a sound file using ffplay (from FFmpeg) as a detached
        subprocess. This is the second choice in the fallback chain,
        used only when mpv (bundled or system) isn't available. Like
        mpv, ffplay talks to the system audio backend directly and
        doesn't depend on Qt's device detection.
        """
        if not FFPLAY_PATH:
            return False
        try:
            subprocess.Popen(
                [FFPLAY_PATH, "-nodisp", "-autoexit", "-loglevel", "quiet", sound_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            print(f"[IdleEasterEgg] Playing sound via ffplay: {sound_path}")
            return True
        except Exception as e:
            print(f"[IdleEasterEgg] Failed to launch ffplay: {e}")
            return False

    def _play_sound_for_image(self, img_path):
        """Play the mapped sound effect for the given image, if one exists."""
        if not img_path:
            return

        img_name = os.path.basename(img_path)
        sound_names = EASTER_EGG_SOUND_FALLBACKS.get(
            img_name,
            [EASTER_EGG_SOUNDS.get(img_name)] if EASTER_EGG_SOUNDS.get(img_name) else []
        )
        if not sound_names:
            return

        sound_path = self._find_sound_path(sound_names)
        if not sound_path:
            tried = [
                os.path.join(EASTER_EGG_DIR, name)
                for name in sound_names
            ]
            tried.extend(
                os.path.join(EASTER_EGG_DIR, "sounds", name)
                for name in sound_names
            )
            print(f"[IdleEasterEgg] Sound file not found: {sound_names}. Tried: {tried}")
            return

        # Fallback chain: mpv -> ffplay -> Qt Multimedia -> silent.
        # mpv (bundled with the app, or found on PATH) is preferred: it
        # manages its own audio backend/device selection independently of
        # Qt, so it works even when QtMultimedia/PipeWire device detection
        # fails inside this app (a known issue on some Linux audio setups).
        if self._play_via_mpv(sound_path):
            return

        # ffplay is the next choice if mpv isn't available for some reason.
        if self._play_via_ffplay(sound_path):
            return

        # Fall back to QtMultimedia if neither mpv nor ffplay are available.
        if not _SOUND_AVAILABLE:
            print("[IdleEasterEgg] No mpv/ffplay/QtMultimedia available; "
                  "playing silently. This is expected and not an error the "
                  "user needs to fix.")
            return

        if QMediaDevices is not None:
            try:
                if QtVersion == 6:
                    if not QMediaDevices.audioOutputs():
                        print("[IdleEasterEgg] No audio output device detected; skipping sound")
                        self._list_audio_devices_debug()
                        return
                else:
                    if not QMediaDevices.availableDevices(QMediaDevices.Output):
                        print("[IdleEasterEgg] No audio output device detected; skipping sound")
                        return
            except Exception:
                # If device probing fails, fall back to attempting playback.
                pass

        try:
            bt_device = self._find_bluetooth_audio_device()

            # If a Bluetooth output device is connected, explicitly route
            # playback through it using QMediaPlayer + QAudioOutput, since
            # QSoundEffect does not support selecting a specific device.
            if bt_device is not None and QMediaPlayer is not None and QAudioOutput is not None:
                audio_output = QAudioOutput(self)
                audio_output.setDevice(bt_device)
                audio_output.setVolume(0.8)
                player = QMediaPlayer(self)
                player.setAudioOutput(audio_output)
                player.setSource(QUrl.fromLocalFile(sound_path))
                player.play()
                self._audio_output = audio_output
                self._media_player = player
                self._sound_effect = None
                print(f"[IdleEasterEgg] Playing sound via Bluetooth device "
                      f"'{bt_device.description()}': {sound_path}")
            # QSoundEffect is ideal when supported; fall back to QMediaPlayer
            # for Qt builds where QSoundEffect is present but silent.
            elif QSoundEffect is not None:
                effect = QSoundEffect(self)
                effect.setSource(QUrl.fromLocalFile(sound_path))
                effect.setLoopCount(1)
                effect.setVolume(0.8)
                effect.play()
                self._sound_effect = effect
                self._media_player = None
                self._audio_output = None
                print(f"[IdleEasterEgg] Playing sound: {sound_path}")
            elif QMediaPlayer is not None and QAudioOutput is not None:
                audio_output = QAudioOutput(self)
                audio_output.setVolume(0.8)
                player = QMediaPlayer(self)
                player.setAudioOutput(audio_output)
                player.setSource(QUrl.fromLocalFile(sound_path))
                player.play()
                self._audio_output = audio_output
                self._media_player = player
                self._sound_effect = None
                print(f"[IdleEasterEgg] Playing sound: {sound_path}")

            if not sound_path.lower().endswith((".wav", ".ogg")):
                print("[IdleEasterEgg] Note: QSoundEffect works best with "
                      "uncompressed WAV/OGG files. If playback fails or is "
                      "silent, convert with: ffmpeg -i <file> -ar 44100 "
                      "-ac 2 <file>.wav")
        except Exception as e:
            print(f"[IdleEasterEgg] Failed to play sound {sound_names}: {e}")
            self._list_audio_devices_debug()

    def _show_egg(self):
        """Show the Easter egg popup window."""
        if self._showing_egg:
            return

        self._showing_egg = True
        img_path = self._get_random_image_path()
        if not img_path:
            self._showing_egg = False
            return

        # Check again if already open (race condition guard)
        if self._egg_window and self._egg_window.isVisible():
            self._showing_egg = False
            return

        # Play a matching sound effect if this image has one mapped
        self._play_sound_for_image(img_path)

        # Create popup window
        self._egg_window = QWidget(None, Qt.WindowType.FramelessWindowHint | Qt.WindowType.WindowStaysOnTopHint | Qt.WindowType.Tool)
        self._egg_window.setObjectName("EasterEggWindow")
        self._egg_window.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self._egg_window.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating)

        # Main layout
        layout = QVBoxLayout(self._egg_window)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Image container with rounded frame
        img_container = QFrame()
        img_container.setObjectName("EasterEggContainer")
        img_container.setStyleSheet("""
            QFrame#EasterEggContainer {
                background: rgba(20, 20, 24, 220);
                border-radius: 16px;
                border: 2px solid rgba(255, 255, 255, 60);
            }
        """)

        container_layout = QVBoxLayout(img_container)
        container_layout.setContentsMargins(16, 16, 16, 16)
        container_layout.setSpacing(8)
        container_layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # Image label
        img_label = QLabel()
        img_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        pixmap = QPixmap(img_path)
        if pixmap.isNull():
            # Try loading with full path
            pixmap = QPixmap(img_path)

        if not pixmap.isNull():
            # Scale the image to a reasonable size (max 400x400 keeping aspect ratio)
            scaled = pixmap.scaled(
                400, 400,
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation
            )
            img_label.setPixmap(scaled)

        container_layout.addWidget(img_label)

        # Hint text
        hint_label = QLabel("Found you looking away! Click to dismiss.")
        hint_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        hint_label.setStyleSheet("""
            QLabel {
                color: rgba(232, 234, 237, 200);
                font-family: 'Segoe UI', Arial, sans-serif;
                font-size: 11pt;
                font-weight: 500;
                background: transparent;
                padding: 4px;
            }
        """)
        container_layout.addWidget(hint_label)

        layout.addWidget(img_container)

        # Position the window
        self._position_window()

        # Add fade-in animation
        self._fade_animation = self._create_fade_in_animation(self._egg_window)

        # Make clickable to dismiss
        self._egg_window.mousePressEvent = lambda e: self._hide_egg()
        img_label.mousePressEvent = lambda e: self._hide_egg()
        hint_label.mousePressEvent = lambda e: self._hide_egg()
        img_container.mousePressEvent = lambda e: self._hide_egg()

        # Record the show timestamp BEFORE calling show(), so that any
        # activity events synthesized during show()/raise_() fall inside
        # the grace period and are ignored by _on_user_activity().
        self._egg_shown_at = time.time()

        # Show the window (do NOT call activateWindow(): combined with
        # WA_ShowWithoutActivating this generated spurious FocusIn events
        # that were misread as "user activity" and closed the popup
        # immediately after it appeared).
        self._egg_window.show()
        self._egg_window.raise_()
        self._egg_window.update()
        self._fade_animation.start()
        self._showing_egg = False

        print(f"[IdleEasterEgg] Showing easter egg: {os.path.basename(img_path)}")

    def _position_window(self):
        """Position the egg window at a random location on screen."""
        if not self._egg_window:
            return

        # Get the screen geometry
        screen = QApplication.primaryScreen()
        if not screen:
            return

        screen_geo = screen.availableGeometry()
        win_width = 440
        win_height = 480
        self._egg_window.resize(win_width, win_height)

        # Random position (keep within screen bounds)
        max_x = max(0, screen_geo.width() - win_width)
        max_y = max(0, screen_geo.height() - win_height)

        # Pick one of several preset positions for variety
        positions = [
            # Center
            (screen_geo.width() // 2 - win_width // 2,
             screen_geo.height() // 2 - win_height // 2),
            # Top-right
            (screen_geo.width() - win_width - 40, 40),
            # Bottom-right
            (screen_geo.width() - win_width - 40,
             screen_geo.height() - win_height - 60),
            # Bottom-left
            (40, screen_geo.height() - win_height - 60),
            # Top-left
            (40, 40),
        ]

        pos = random.choice(positions)
        self._egg_window.move(pos[0], pos[1])

    def _create_fade_in_animation(self, widget):
        """Create a fade-in opacity animation for the given widget."""
        opacity_effect = QGraphicsOpacityEffect(widget)
        opacity_effect.setOpacity(0.0)
        widget.setGraphicsEffect(opacity_effect)

        animation = QPropertyAnimation(opacity_effect, b"opacity")
        animation.setDuration(600)
        animation.setStartValue(0.0)
        animation.setEndValue(1.0)
        return animation

    def _hide_egg(self):
        """Hide the Easter egg window with fade-out."""
        if not self._egg_window:
            return

        # Reset last activity so it doesn't immediately reappear
        self._last_activity = time.time()

        # Fade out and close
        try:
            # Remove the fade animation first to avoid conflicts
            self._egg_window.setGraphicsEffect(None)
            self._egg_window.close()
            self._egg_window.deleteLater()
        except Exception:
            pass

        self._egg_window = None
        self._fade_animation = None
        self._showing_egg = False
        self._egg_shown_at = 0.0

        # Stop any sound that might still be playing
        if self._sound_effect:
            try:
                self._sound_effect.stop()
            except Exception:
                pass
            self._sound_effect = None
        if self._media_player:
            try:
                self._media_player.stop()
            except Exception:
                pass
            self._media_player = None
        self._audio_output = None

    def set_active(self, active: bool):
        """Enable or disable the Easter egg feature."""
        self._active = active
        if not active and self._egg_window:
            self._hide_egg()

    def is_active(self) -> bool:
        """Check if the Easter egg feature is active."""
        return self._active

    def cleanup(self):
        """Clean up resources."""
        self._active = False
        self._idle_timer.stop()
        if self._egg_window:
            try:
                self._egg_window.close()
                self._egg_window.deleteLater()
            except Exception:
                pass
            self._egg_window = None