"""
Performance Manager for Cloudar Browser™
Smart Tab Sleeping: Frozen (5 min) â†’ Discarded (15 min)
Energy Saver with Windows EcoQoS support.
RAM monitoring for Cloudar process.
"""
import sys
import ctypes
import time
import os
from core.browser_qt import QObject, QTimer, QWebEnginePage, QLabel


class PerformanceManager(QObject):
    """Manages browser performance with smart tab sleeping"""

    FREEZE_TIMEOUT  = 5  * 60   # seconds â†’ Frozen
    DISCARD_TIMEOUT = 15 * 60   # seconds â†’ Discarded

    def __init__(self, browser_window):
        super().__init__(browser_window)
        self.browser = browser_window

        # last_active[tab_widget] = timestamp
        self._last_active: dict = {}

        # Memory Saver: check every 60 seconds
        self.memory_timer = QTimer(self)
        self.memory_timer.timeout.connect(self.check_inactive_tabs)
        self.memory_timer.start(60 * 1000)

        # Energy Saver: check every minute
        self.energy_timer = QTimer(self)
        self.energy_timer.timeout.connect(self.check_power_status)
        self.energy_timer.start(60 * 1000)

        # RAM Monitor: check every 2 seconds
        self.ram_timer = QTimer(self)
        self.ram_timer.timeout.connect(self.update_ram_display)
        self.ram_timer.start(2 * 1000)

        # RAM display label
        self.ram_label = None
        self._current_ram_mb = 0

        QTimer.singleShot(1000, self.check_power_status)
        QTimer.singleShot(1000, self.update_ram_display)

    # â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    # Track active tab
    # â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    def on_tab_activated(self, web_view):
        """Call this whenever a tab becomes active (switch or first load)."""
        self._last_active[id(web_view)] = time.time()
        # Wake sleeping tab
        if hasattr(web_view, 'wake') and getattr(web_view, 'is_sleeping', False):
            web_view.wake()
            self._update_sleep_indicator(web_view, sleeping=False)

    def on_tab_created(self, web_view):
        """Register a new tab."""
        self._last_active[id(web_view)] = time.time()

    def on_tab_closed(self, web_view):
        """Unregister a closed tab."""
        self._last_active.pop(id(web_view), None)

    # â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    # Smart Tab Sleeping
    # â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    def check_inactive_tabs(self):
        """Check all background tabs and sleep/discard as appropriate."""
        if not self.browser.settings.get("performance_memory_saver", True):
            return

        freeze_timeout, discard_timeout = self._get_timeouts()
        if freeze_timeout <= 0:
            return

        tabs = self.browser.tabs
        current_index = tabs.currentIndex()
        now = time.time()

        for i in range(tabs.count()):
            if i == current_index:
                continue

            web_view = tabs.widget(i)
            if web_view is None:
                continue

            page = web_view.page()

            # Skip audible tabs
            try:
                if page.recentlyAudible():
                    continue
            except Exception:
                pass

            # Get inactivity duration
            last = self._last_active.get(id(web_view), now)
            inactive_for = now - last

            try:
                current_state = page.lifecycleState()
                frozen_state   = QWebEnginePage.LifecycleState.FrozenLifecycleState
                discarded_state = QWebEnginePage.LifecycleState.DiscardedLifecycleState
                active_state   = QWebEnginePage.LifecycleState.ActiveLifecycleState

                if discard_timeout and inactive_for >= discard_timeout:
                    if current_state != discarded_state:
                        page.setLifecycleState(discarded_state)
                        if hasattr(web_view, '_is_sleeping'):
                            web_view._is_sleeping = True
                        self._update_sleep_indicator(web_view, sleeping=True)

                elif inactive_for >= freeze_timeout:
                    if current_state == active_state:
                        page.setLifecycleState(frozen_state)
                        if hasattr(web_view, '_is_sleeping'):
                            web_view._is_sleeping = True
                        self._update_sleep_indicator(web_view, sleeping=True)

            except Exception:
                pass

    def _get_timeouts(self):
        """Compute freeze/discard timeouts from settings."""
        minutes = self.browser.settings.get("tab_sleep_minutes", 5)
        try:
            minutes = int(minutes)
        except Exception:
            minutes = 5
        if minutes <= 0:
            return 0, 0

        freeze_timeout = max(60, minutes * 60)
        discard_minutes = self.browser.settings.get("tab_discard_minutes", minutes * 3)
        try:
            discard_minutes = int(discard_minutes)
        except Exception:
            discard_minutes = minutes * 3
        discard_timeout = max(freeze_timeout, discard_minutes * 60) if discard_minutes > 0 else 0
        return freeze_timeout, discard_timeout

    def _update_sleep_indicator(self, web_view, sleeping: bool):
        """Add or remove ðŸ’¤ prefix on the tab's title."""
        try:
            tabs = self.browser.tabs
            i = tabs.indexOf(web_view)
            if i < 0:
                return
            current_text = tabs.tabText(i)
            if sleeping:
                if not current_text.startswith("ðŸ’¤ "):
                    tabs.setTabText(i, "ðŸ’¤ " + current_text)
            else:
                if current_text.startswith("ðŸ’¤ "):
                    tabs.setTabText(i, current_text[3:])
        except Exception:
            pass

    # â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    # Energy Saver
    # â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    def check_power_status(self):
        if not self.browser.settings.get("performance_energy_saver", True):
            return
        if sys.platform == 'win32':
            on_battery = self._is_on_battery()
            if hasattr(self.browser, '_set_efficiency_mode'):
                self.browser._set_efficiency_mode(on_battery)

    def _is_on_battery(self):
        if sys.platform != 'win32':
            return False
        try:
            class SYSTEM_POWER_STATUS(ctypes.Structure):
                _fields_ = [
                    ('ACLineStatus',       ctypes.c_byte),
                    ('BatteryFlag',        ctypes.c_byte),
                    ('BatteryLifePercent', ctypes.c_byte),
                    ('SystemStatusFlag',   ctypes.c_byte),
                    ('BatteryLifeTime',    ctypes.c_ulong),
                    ('BatteryFullLifeTime',ctypes.c_ulong),
                ]
            status = SYSTEM_POWER_STATUS()
            if ctypes.windll.kernel32.GetSystemPowerStatus(ctypes.byref(status)):
                return status.ACLineStatus == 0
        except Exception:
            pass
        return False

    # â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    # RAM Monitoring
    # â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    def get_process_ram_mb(self):
        """Get current process RAM usage in MB"""
        try:
            pid = os.getpid()
            
            if sys.platform == 'win32':
                return self._get_ram_windows(pid)
            elif sys.platform == 'darwin':
                return self._get_ram_macos(pid)
            elif sys.platform.startswith('linux'):
                return self._get_ram_linux(pid)
        except Exception:
            pass
        return 0.0

    def _get_ram_windows(self, pid):
        """Get RAM usage on Windows using psapi"""
        try:
            import ctypes.wintypes as wintypes
            
            class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
                _fields_ = [
                    ('cb', wintypes.DWORD),
                    ('PageFaultCount', wintypes.DWORD),
                    ('PeakWorkingSetSize', ctypes.c_size_t),
                    ('WorkingSetSize', ctypes.c_size_t),
                    ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaNonPagedPoolUsage', ctypes.c_size_t),
                    ('PagefileUsage', ctypes.c_size_t),
                    ('PeakPagefileUsage', ctypes.c_size_t),
                ]
            
            counters = PROCESS_MEMORY_COUNTERS()
            counters.cb = ctypes.sizeof(PROCESS_MEMORY_COUNTERS)
            
            handle = ctypes.windll.kernel32.OpenProcess(
                0x0400 | 0x0010,  # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
                False,
                pid
            )
            
            if handle:
                try:
                    if ctypes.windll.psapi.GetProcessMemoryInfo(
                        handle,
                        ctypes.byref(counters),
                        counters.cb
                    ):
                        # WorkingSetSize is in bytes, convert to MB
                        return counters.WorkingSetSize / (1024 * 1024)
                finally:
                    ctypes.windll.kernel32.CloseHandle(handle)
        except Exception:
            pass
        return 0.0

    def _get_ram_linux(self, pid):
        """Get RAM usage on Linux from /proc/pid/status"""
        try:
            status_file = f"/proc/{pid}/status"
            with open(status_file, 'r') as f:
                for line in f:
                    if line.startswith('VmRSS:'):
                        # VmRSS is in kB
                        parts = line.split()
                        if len(parts) >= 2:
                            rss_kb = int(parts[1])
                            return rss_kb / 1024.0  # Convert to MB
        except Exception:
            pass
        return 0.0

    def _get_ram_macos(self, pid):
        """Get RAM usage on macOS using task_info"""
        try:
            class TASK_BASIC_INFO(ctypes.Structure):
                _fields_ = [
                    ('virtual_size', ctypes.c_ulonglong),
                    ('resident_size', ctypes.c_ulonglong),
                    ('policy', ctypes.c_uint32),
                    ('suspend_count', ctypes.c_uint32),
                ]
            
            TASK_BASIC_INFO_COUNT = ctypes.sizeof(TASK_BASIC_INFO) // 4
            
            task_info = TASK_BASIC_INFO()
            count = TASK_BASIC_INFO_COUNT
            
            # KERN_SUCCESS = 0
            result = ctypes.CDLL('/usr/lib/libSystem.B.dylib').task_info(
                pid,  # task_t (mach_port_t on macOS)
                4,  # TASK_BASIC_INFO flavor
                ctypes.byref(task_info),
                ctypes.byref(ctypes.c_uint32(count))
            )
            
            if result == 0:  # KERN_SUCCESS
                # resident_size is in bytes, convert to MB
                return task_info.resident_size / (1024 * 1024)
        except Exception:
            pass
        return 0.0

    def update_ram_display(self):
        """Update RAM usage display"""
        ram_mb = self.get_process_ram_mb()
        self._current_ram_mb = ram_mb
        
        # Create label if it doesn't exist
        if self.ram_label is None:
            self._create_ram_label()
        
        # Update label text
        if self.ram_label:
            ram_text = f"RAM: {ram_mb:.1f} MB"
            self.ram_label.setText(ram_text)
            self.ram_label.setToolTip(f"Cloudar Browser RAM Usage: {ram_mb:.1f} MB")

    def _create_ram_label(self):
        """Create and add RAM label to status bar"""
        try:
            if hasattr(self.browser, 'status'):
                self.ram_label = QLabel("RAM: 0.0 MB")
                self.ram_label.setObjectName("RAMStatus")
                self.ram_label.setToolTip("Cloudar Browser RAM Usage")
                self.browser.status.addPermanentWidget(self.ram_label)
        except Exception:
            pass

    def get_current_ram_mb(self):
        """Get the last measured RAM value in MB"""
        return self._current_ram_mb



