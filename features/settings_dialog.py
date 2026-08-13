"""
Settings dialog for browser preferences
"""
from core.browser_qt import (QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
                     QLineEdit, QPushButton, QGroupBox, QFormLayout,
                     QFileDialog, pyqtSignal, Qt, QMessageBox,
                     QListWidget, QStackedWidget, QWidget, QCheckBox, 
                     QComboBox, QListWidgetItem, QIcon, QSize, QScrollArea)
from core.browser_resources import SETTINGS_FILE, DEFAULT_SETTINGS, load_json_file, save_json_file


class ClickableRow(QWidget):
    """A row widget that can be clicked"""
    def __init__(self, on_click=None, parent=None):
        super().__init__(parent)
        self.on_click = on_click
        self.setObjectName("settingsRow")
        
    def mousePressEvent(self, event):
        if self.on_click and event.button() == Qt.MouseButton.LeftButton:
            self.on_click()
        super().mousePressEvent(event)


class SettingsDialog(QDialog):
    """Settings/Preferences dialog"""

    settings_changed = pyqtSignal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Settings")
        self.resize(1000, 700)
        self.setMinimumSize(900, 600)
        self.setMaximumSize(1200, 800)
        self.settings = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        self.setup_ui()

    def create_settings_row(self, title, description, widget=None, clickable=False, on_click=None):
        """Helper to create a settings row with title and description"""
        if clickable:
            row = ClickableRow(on_click=on_click)
            row.setCursor(Qt.CursorShape.PointingHandCursor)
        else:
            row = QWidget()
            row.setObjectName("settingsRow")

        row.setStyleSheet("""
            QWidget#settingsRow {
                background-color: transparent;
                border-bottom: 1px solid #3e3e3e;
            }
            QWidget#settingsRow:hover {
                background-color: #2b2b2b;
            }
        """)

        layout = QHBoxLayout(row)
        layout.setContentsMargins(10, 15, 10, 15)

        text_container = QWidget()
        text_layout = QVBoxLayout(text_container)
        text_layout.setContentsMargins(0, 0, 0, 0)
        text_layout.setSpacing(4)

        title_label = QLabel(title)
        title_label.setStyleSheet("font-size: 15px; font-weight: bold; color: #e0e0e0; background: transparent;")

        description_label = QLabel(description)
        description_label.setStyleSheet("font-size: 13px; color: #b0b0b0; background: transparent;")
        description_label.setWordWrap(True)

        text_layout.addWidget(title_label)
        text_layout.addWidget(description_label)

        layout.addWidget(text_container, 1)

        if widget:
            layout.addWidget(widget)
        elif clickable:
            arrow = QLabel("â€º")
            arrow.setStyleSheet("font-size: 24px; color: #808080; background: transparent; padding-right: 10px;")
            layout.addWidget(arrow)

        return row

    def setup_ui(self):
        """Setup the UI with Sidebar + Scrollable Content + Fixed Buttons"""

        # Main layout
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # Body layout (sidebar + content)
        body_layout = QHBoxLayout()
        body_layout.setContentsMargins(0, 0, 0, 0)
        body_layout.setSpacing(0)

        # ================== SIDEBAR ==================
        sidebar_container = QWidget()
        sidebar_container.setFixedWidth(250)
        sidebar_container.setStyleSheet("""
            QWidget {
                background-color: #2b2b2b;
                border-right: 1px solid #3e3e3e;
            }
            QListWidget {
                border: none;
                background-color: transparent;
                outline: none;
            }
            QListWidget::item {
                height: 50px;
                padding-left: 15px;
                color: #e0e0e0;
                border-radius: 5px;
                margin: 5px 10px;
            }
            QListWidget::item:selected {
                background-color: #3e3e3e;
                color: #ffffff;
                border-left: 4px solid #60cdff;
            }
            QListWidget::item:hover {
                background-color: #353535;
            }
        """)

        sidebar_layout = QVBoxLayout(sidebar_container)
        sidebar_layout.setContentsMargins(0, 20, 0, 20)

        title_label = QLabel("Settings")
        title_label.setStyleSheet("font-size: 22px; font-weight: bold; color: #ffffff; margin-left: 20px; margin-bottom: 20px;")
        sidebar_layout.addWidget(title_label)

        self.nav_list = QListWidget()
        self.nav_list.setIconSize(QSize(24, 24))

        nav_items = [
            "General",
            "Downloads",
            "Privacy",
            "Appearance",
            "Performance",
            "About"
        ]

        for name in nav_items:
            item = QListWidgetItem(name)
            item.setTextAlignment(Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft)
            self.nav_list.addItem(item)

        sidebar_layout.addWidget(self.nav_list)
        sidebar_layout.addStretch()

        # ================== CONTENT AREA ==================
        content_container = QWidget()
        content_container.setStyleSheet("background-color: #202020;")
        content_layout = QVBoxLayout(content_container)
        content_layout.setContentsMargins(0, 0, 0, 0)
        content_layout.setSpacing(0)

        self.pages = QStackedWidget()

        # --- Page 1: General ---
        page_general = QWidget()
        pg_layout = QVBoxLayout()
        pg_layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        pg_title = QLabel("General")
        pg_title.setStyleSheet("font-size: 28px; font-weight: bold; margin-bottom: 20px; color: white;")
        pg_layout.addWidget(pg_title)

        form_widget = QWidget()
        form_layout = QFormLayout(form_widget)
        form_layout.setSpacing(20)
        form_layout.setLabelAlignment(Qt.AlignmentFlag.AlignLeft)

        self.home_page_input = QLineEdit(self.settings.get("home_page", ""))
        self.home_page_input.setPlaceholderText("cloudar://newtab")
        self.home_page_input.setMinimumWidth(400)
        form_layout.addRow("Home Page:", self.home_page_input)

        self.search_engine_input = QLineEdit(self.settings.get("search_engine", ""))
        self.search_engine_input.setPlaceholderText("https://www.google.com/search?q={}")
        form_layout.addRow("Search Engine:", self.search_engine_input)

        self.startup_combo = QComboBox()
        self.startup_combo.addItems(["Open New Tab", "Continue where you left off", "Open Home Page"])
        self.startup_combo.setCurrentText(self.settings.get("startup_behavior", "Open New Tab"))
        form_layout.addRow("On Startup:", self.startup_combo)

        self.translate_check = QCheckBox("Enable Translate Web Button")
        self.translate_check.setChecked(self.settings.get("translation_enabled", True))
        form_layout.addRow("", self.translate_check)

        self.theme_combo = QComboBox()
        self.theme_combo.addItems(["Dark", "Light"])
        self.theme_combo.setCurrentText(self.settings.get("theme", "Dark"))
        form_layout.addRow("Theme:", self.theme_combo)

        pg_layout.addWidget(form_widget)
        page_general.setLayout(pg_layout)
        self.pages.addWidget(page_general)

        # --- Page 2: Downloads ---
        page_downloads = QWidget()
        pd_layout = QVBoxLayout()
        pd_layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        pd_title = QLabel("Downloads")
        pd_title.setStyleSheet("font-size: 28px; font-weight: bold; margin-bottom: 20px; color: white;")
        pd_layout.addWidget(pd_title)

        pd_form = QFormLayout()
        pd_form.setSpacing(20)

        dl_path_layout = QHBoxLayout()
        self.download_path_input = QLineEdit(self.settings.get("download_location", ""))
        self.download_path_input.setMinimumWidth(300)
        dl_path_layout.addWidget(self.download_path_input)

        browse_btn = QPushButton("Change")
        browse_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        browse_btn.clicked.connect(self.browse_download_location)
        dl_path_layout.addWidget(browse_btn)

        pd_form.addRow("Location:", dl_path_layout)

        self.ask_download_check = QCheckBox("Ask where to save each file before downloading")
        self.ask_download_check.setChecked(self.settings.get("ask_download", False))
        pd_form.addRow("", self.ask_download_check)

        # Force download directory (VD) section
        pd_form.addRow(QLabel(""))  # Spacer
        
        force_vd_group = QGroupBox("Force Download Directory (VD)")
        force_vd_group.setStyleSheet("""
            QGroupBox {
                font-weight: bold;
                font-size: 14px;
                color: #e0e0e0;
                border: 1px solid #505050;
                border-radius: 5px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
            }
        """)
        force_vd_layout = QVBoxLayout()
        
        self.force_vd_check = QCheckBox("Always use this directory for downloads")
        self.force_vd_check.setChecked(self.settings.get("force_download_directory", False))
        self.force_vd_check.setStyleSheet("font-size: 14px; color: #e0e0e0;")
        self.force_vd_check.toggled.connect(self._on_force_vd_toggled)
        force_vd_layout.addWidget(self.force_vd_check)
        
        forced_path_layout = QHBoxLayout()
        self.forced_path_input = QLineEdit(self.settings.get("forced_download_path", ""))
        self.forced_path_input.setPlaceholderText("/path/to/download/folder")
        self.forced_path_input.setMinimumWidth(300)
        self.forced_path_input.setEnabled(self.force_vd_check.isChecked())
        forced_path_layout.addWidget(self.forced_path_input)
        
        browse_forced_btn = QPushButton("Change")
        browse_forced_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        browse_forced_btn.clicked.connect(self.browse_forced_download_location)
        browse_forced_btn.setEnabled(self.force_vd_check.isChecked())
        forced_path_layout.addWidget(browse_forced_btn)
        
        force_vd_layout.addLayout(forced_path_layout)
        
        force_vd_desc = QLabel("When enabled, all downloads will be saved to the specified directory, ignoring the browser's default download location.")
        force_vd_desc.setWordWrap(True)
        force_vd_desc.setStyleSheet("color: #b0b0b0; font-size: 12px; margin-top: 5px;")
        force_vd_layout.addWidget(force_vd_desc)
        
        force_vd_group.setLayout(force_vd_layout)
        pd_form.addRow("", force_vd_group)

        pd_layout.addLayout(pd_form)
        page_downloads.setLayout(pd_layout)
        self.pages.addWidget(page_downloads)

        # --- Page 3: Privacy ---
        page_privacy = QWidget()
        pp_layout = QVBoxLayout()
        pp_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        pp_layout.setSpacing(0)

        pp_title = QLabel("Privacy & Security")
        pp_title.setStyleSheet("font-size: 28px; font-weight: bold; margin-bottom: 20px; color: white;")
        pp_layout.addWidget(pp_title)

        pp_layout.addWidget(self.create_settings_row(
            "Clear browsing data",
            "Clear history, cookies, cache, and more",
            clickable=True,
            on_click=self.clear_browsing_data
        ))

        pp_layout.addWidget(self.create_settings_row(
            "Privacy Guide",
            "Review key privacy and security controls",
            clickable=True,
            on_click=lambda: QMessageBox.information(self, "Privacy Guide", "The Privacy Guide helps you choose the right protection for you.")
        ))

        self.cookies_check = QCheckBox()
        self.cookies_check.setChecked(self.settings.get("third_party_cookies", "block") == "block")
        pp_layout.addWidget(self.create_settings_row(
            "Third-party cookies",
            "Block third-party cookies when browsing",
            widget=self.cookies_check
        ))

        self.ad_privacy_check = QCheckBox()
        self.ad_privacy_check.setChecked(self.settings.get("ad_privacy", True))
        pp_layout.addWidget(self.create_settings_row(
            "Ad privacy",
            "Choose the info that sites can use to show you ads",
            widget=self.ad_privacy_check
        ))

        self.safe_browsing_check = QCheckBox()
        self.safe_browsing_check.setChecked(self.settings.get("safe_browsing", True))
        pp_layout.addWidget(self.create_settings_row(
            "Security",
            "Safe Browsing (protection from dangerous sites) and other security settings",
            widget=self.safe_browsing_check
        ))

        pp_layout.addWidget(self.create_settings_row(
            "Site settings",
            "Controls what information sites can use and show (location, camera, pop-ups, and more)",
            clickable=True,
            on_click=lambda: QMessageBox.information(self, "Site Settings", "Site settings management is coming soon!")
        ))

        self.absolute_security_check = QCheckBox()
        self.absolute_security_check.setChecked(self.settings.get("absolute_security", False))
        pp_layout.addWidget(self.create_settings_row(
            "Absolute Security Mode",
            "Hardens security by disabling JavaScript, LocalStorage, and enforcing HTTPS.",
            widget=self.absolute_security_check
        ))

        pp_layout.addSpacing(20)

        self.dnt_check = QCheckBox("Send a 'Do Not Track' request with your browsing traffic")
        self.dnt_check.setChecked(self.settings.get("do_not_track", False))
        self.dnt_check.setStyleSheet("font-size: 14px; margin-top: 20px; margin-bottom: 10px; color: #b0b0b0;")
        pp_layout.addWidget(self.dnt_check)

        page_privacy.setLayout(pp_layout)
        self.pages.addWidget(page_privacy)

        # --- Page 4: Appearance ---
        page_appearance = QWidget()
        pa_layout = QVBoxLayout()
        pa_layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        pa_title = QLabel("Appearance")
        pa_title.setStyleSheet("font-size: 28px; font-weight: bold; margin-bottom: 20px; color: white;")
        pa_layout.addWidget(pa_title)

        pa_form = QFormLayout()
        pa_form.setSpacing(20)

        pa_form.addRow("Theme:", QLabel("Modern Dark (Default)"))

        self.zoom_combo = QComboBox()
        self.zoom_combo.addItems(["50%", "75%", "100%", "125%", "150%", "200%"])
        self.zoom_combo.setCurrentText(self.settings.get("default_zoom", "100%"))
        pa_form.addRow("Page Zoom:", self.zoom_combo)

        pa_form.addRow(QLabel("New Tab Page:"))

        self.nt_color_input = QLineEdit(self.settings.get("newtab_frame_color", "#60cdff"))
        self.nt_color_input.setPlaceholderText("#RRGGBB")
        pa_form.addRow("  Frame Color:", self.nt_color_input)

        self.nt_bg_input = QLineEdit(self.settings.get("newtab_background_image", ""))
        self.nt_bg_input.setPlaceholderText("https:// or file:// URL")
        pa_form.addRow("  Background Image:", self.nt_bg_input)

        pa_layout.addLayout(pa_form)
        page_appearance.setLayout(pa_layout)
        self.pages.addWidget(page_appearance)

        # --- Page 5: Performance ---
        page_performance = QWidget()
        pperf_layout = QVBoxLayout()
        pperf_layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        pperf_title = QLabel("Performance")
        pperf_title.setStyleSheet("font-size: 28px; font-weight: bold; margin-bottom: 20px; color: white;")
        pperf_layout.addWidget(pperf_title)

        self.memory_saver_check = QCheckBox()
        self.memory_saver_check.setChecked(self.settings.get("performance_memory_saver", True))
        self.memory_saver_check.setText("Memory Saver")
        self.memory_saver_check.setStyleSheet("font-size: 16px; font-weight: bold; color: #e0e0e0;")

        mem_desc = QLabel("When on, Cloudar frees up memory from inactive tabs. This gives active tabs and other apps more computer resources and keeps Cloudar fast. Your inactive tabs automatically become active again when you go back to them.")
        mem_desc.setWordWrap(True)
        mem_desc.setStyleSheet("color: #b0b0b0; margin-left: 20px; margin-bottom: 15px;")

        pperf_layout.addWidget(self.memory_saver_check)
        pperf_layout.addWidget(mem_desc)

        pperf_layout.addSpacing(10)

        self.energy_saver_check = QCheckBox()
        self.energy_saver_check.setChecked(self.settings.get("performance_energy_saver", True))
        self.energy_saver_check.setText("Energy Saver")
        self.energy_saver_check.setStyleSheet("font-size: 16px; font-weight: bold; color: #e0e0e0;")

        energy_desc = QLabel("When on, Cloudar conserves battery power by limiting background activity and visual effects, such as smooth scrolling and video frame rates.")
        energy_desc.setWordWrap(True)
        energy_desc.setStyleSheet("color: #b0b0b0; margin-left: 20px; margin-bottom: 15px;")

        pperf_layout.addWidget(self.energy_saver_check)
        pperf_layout.addWidget(energy_desc)

        pperf_layout.addSpacing(10)

        pperf_sys_label = QLabel("System")
        pperf_sys_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #e0e0e0; margin-top: 10px;")
        pperf_layout.addWidget(pperf_sys_label)

        self.hardware_accel_check = QCheckBox("Use graphics acceleration when available")
        self.hardware_accel_check.setChecked(self.settings.get("performance_hardware_acceleration", True))
        pperf_layout.addWidget(self.hardware_accel_check)

        page_performance.setLayout(pperf_layout)
        self.pages.addWidget(page_performance)

        # --- Page 6: About ---
        page_about = QWidget()
        pab_layout = QVBoxLayout()
        pab_layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        pab_title = QLabel("About Cloudar Browser™")
        pab_title.setStyleSheet("font-size: 28px; font-weight: bold; margin-bottom: 20px; color: white;")
        pab_layout.addWidget(pab_title)

        logo = QLabel("C")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setStyleSheet("font-size: 80px; font-weight: bold; color: #60cdff; border: 4px solid #60cdff; border-radius: 50px; width: 100px; height: 100px;")
        logo.setFixedSize(120, 120)

        logo_container = QHBoxLayout()
        logo_container.addStretch()
        logo_container.addWidget(logo)
        logo_container.addStretch()
        pab_layout.addLayout(logo_container)

        ver_label = QLabel("Cloudar Browser™\nVersion 1.0.0 (Official Build)\nOS: Windows")
        ver_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        ver_label.setStyleSheet("font-size: 16px; margin-top: 20px; color: #b0b0b0;")
        pab_layout.addWidget(ver_label)

        pab_layout.addSpacing(30)

        update_btn = QPushButton("Check for Updates")
        update_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        update_btn.setFixedSize(200, 40)
        update_btn.clicked.connect(lambda: QMessageBox.information(self, "Update", "You are using the latest version."))

        btn_container = QHBoxLayout()
        btn_container.addStretch()
        btn_container.addWidget(update_btn)
        btn_container.addStretch()
        pab_layout.addLayout(btn_container)

        page_about.setLayout(pab_layout)
        self.pages.addWidget(page_about)

        # Scroll area
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setFrameShape(QScrollArea.Shape.NoFrame)
        scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)

        scroll_content = QWidget()
        scroll_layout = QVBoxLayout(scroll_content)
        scroll_layout.setContentsMargins(30, 30, 30, 30)
        scroll_layout.addWidget(self.pages)
        scroll_layout.addStretch()
        scroll_area.setWidget(scroll_content)

        content_layout.addWidget(scroll_area)

        body_layout.addWidget(sidebar_container)
        body_layout.addWidget(content_container, 1)

        # ================== BUTTONS ==================
        button_layout = QHBoxLayout()
        button_layout.setContentsMargins(20, 10, 20, 10)
        button_layout.addStretch()

        ok_btn = QPushButton("OK")
        ok_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        ok_btn.setFixedSize(100, 36)
        ok_btn.setStyleSheet("""
            QPushButton {
                background-color: #0078d4;
                color: white;
                font-weight: bold;
                border-radius: 4px;
            }
            QPushButton:hover {
                background-color: #1084d9;
            }
        """)
        ok_btn.clicked.connect(self.save_settings)
        button_layout.addWidget(ok_btn)

        cancel_btn = QPushButton("Cancel")
        cancel_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        cancel_btn.setFixedSize(100, 36)
        cancel_btn.setStyleSheet("""
            QPushButton {
                background-color: #3e3e3e;
                color: white;
                border: 1px solid #505050;
                border-radius: 4px;
            }
            QPushButton:hover {
                background-color: #4e4e4e;
            }
        """)
        cancel_btn.clicked.connect(self.reject)
        button_layout.addWidget(cancel_btn)

        apply_btn = QPushButton("Apply")
        apply_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        apply_btn.setFixedSize(100, 36)
        apply_btn.setStyleSheet("""
            QPushButton {
                background-color: #2d2d2d;
                color: white;
                border: 1px solid #505050;
                border-radius: 4px;
            }
            QPushButton:hover {
                background-color: #3a3a3a;
            }
        """)
        apply_btn.clicked.connect(self.apply_settings)
        button_layout.addWidget(apply_btn)

        main_layout.addLayout(body_layout, 1)
        main_layout.addLayout(button_layout)
        self.setLayout(main_layout)

        # Connect Navigation
        self.nav_list.currentRowChanged.connect(self.pages.setCurrentIndex)
        self.nav_list.setCurrentRow(0)

    def browse_download_location(self):
        """Browse for download location"""
        folder = QFileDialog.getExistingDirectory(self, "Select Download Location")
        if folder:
            self.download_path_input.setText(folder)

    def _on_force_vd_toggled(self, checked):
        """Enable/disable forced path input when checkbox is toggled"""
        self.forced_path_input.setEnabled(checked)
        # Find the browse button in the parent layout
        parent_widget = self.forced_path_input.parent()
        if parent_widget:
            for child in parent_widget.children():
                if isinstance(child, QPushButton):
                    child.setEnabled(checked)
                    break

    def browse_forced_download_location(self):
        """Browse for forced download location"""
        folder = QFileDialog.getExistingDirectory(self, "Select Forced Download Location")
        if folder:
            self.forced_path_input.setText(folder)

    def _collect_settings(self):
        """Collect settings from UI widgets."""
        self.settings["home_page"] = self.home_page_input.text()
        self.settings["search_engine"] = self.search_engine_input.text()
        self.settings["startup_behavior"] = self.startup_combo.currentText()

        self.settings["download_location"] = self.download_path_input.text()
        self.settings["ask_download"] = self.ask_download_check.isChecked()
        self.settings["force_download_directory"] = self.force_vd_check.isChecked()
        self.settings["forced_download_path"] = self.forced_path_input.text()

        self.settings["do_not_track"] = self.dnt_check.isChecked()
        self.settings["translation_enabled"] = self.translate_check.isChecked()
        self.settings["theme"] = self.theme_combo.currentText()
        self.settings["default_zoom"] = self.zoom_combo.currentText()

        self.settings["newtab_frame_color"] = self.nt_color_input.text()
        self.settings["newtab_background_image"] = self.nt_bg_input.text()

        self.settings["performance_memory_saver"] = self.memory_saver_check.isChecked()
        self.settings["performance_energy_saver"] = self.energy_saver_check.isChecked()
        self.settings["performance_hardware_acceleration"] = self.hardware_accel_check.isChecked()

        self.settings["absolute_security"] = self.absolute_security_check.isChecked()
        self.settings["third_party_cookies"] = "block" if self.cookies_check.isChecked() else "allow"
        self.settings["ad_privacy"] = self.ad_privacy_check.isChecked()
        self.settings["safe_browsing"] = self.safe_browsing_check.isChecked()

    def save_settings(self):
        """Save settings and close dialog (OK)."""
        self._collect_settings()
        save_json_file(SETTINGS_FILE, self.settings)
        self.settings_changed.emit(self.settings)
        self.accept()

    def apply_settings(self):
        """Save settings without closing dialog (Apply)."""
        self._collect_settings()
        save_json_file(SETTINGS_FILE, self.settings)
        self.settings_changed.emit(self.settings)

    def clear_browsing_data(self):
        """Clear browsing data"""
        from core.browser_qt import QMessageBox
        reply = QMessageBox.question(
            self, "Clear Data",
            "Are you sure? This will clear history and cache.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.Yes:
            self.settings_changed.emit({"action": "clear_data"})
            QMessageBox.information(self, "Clear Data", "Browsing data cleared successfully.")

    def get_settings(self):
        """Get current settings"""
        return self.settings

