#!/bin/bash
# Build .deb package for Cloudar Browser
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

APP_NAME="cloudar-browser"
APP_VERSION="1.0.0"
ARCH="amd64"
PACKAGE_NAME="${APP_NAME}_${APP_VERSION}_${ARCH}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }
print_header() { echo -e "${BLUE}================================${NC}"; echo -e "${BLUE}$1${NC}"; echo -e "${BLUE}================================${NC}"; }

# Check prerequisites
check_prereqs() {
    print_header "Checking prerequisites"
    
    if ! command -v dpkg-deb &> /dev/null; then
        print_error "dpkg-deb not found. Install dpkg-dev package."
        exit 1
    fi
    print_info "dpkg-deb found"
    
    if ! command -v pyinstaller &> /dev/null; then
        print_warn "PyInstaller not found in PATH. Checking venv..."
        if [ -d "venv" ] && [ -f "venv/bin/pyinstaller" ]; then
            print_info "Using PyInstaller from venv"
            export PATH="$SCRIPT_DIR/venv/bin:$PATH"
        else
            print_error "PyInstaller not found. Install it: pip install pyinstaller"
            exit 1
        fi
    fi
    print_info "PyInstaller found: $(which pyinstaller)"
}

# Clean previous builds
clean_build() {
    print_header "Cleaning previous builds"
    rm -rf build/ build_deb/ dist/ *.deb
    print_info "Cleaned build artifacts"
}

# Build with PyInstaller
bundle_with_pyinstaller() {
    print_header "Bundling application with PyInstaller"
    
    # Use existing spec file
    print_info "Running PyInstaller..."
    pyinstaller --clean --noconfirm cloudar-browser.spec 2>&1
    
    if [ -d "dist/CloudarBrowser" ]; then
        print_info "PyInstaller bundle created at dist/CloudarBrowser (directory)"
    elif [ -f "dist/CloudarBrowser" ]; then
        print_info "PyInstaller bundle created at dist/CloudarBrowser (single file)"
    else
        print_error "PyInstaller failed to create bundle"
        ls -la dist/ 2>/dev/null || true
        exit 1
    fi
}

# Create .deb package structure
create_deb_structure() {
    print_header "Creating .deb package structure"
    
    DEB_DIR="build_deb/${PACKAGE_NAME}"
    mkdir -p "${DEB_DIR}/DEBIAN"
    mkdir -p "${DEB_DIR}/usr/bin"
    mkdir -p "${DEB_DIR}/usr/share/applications"
    mkdir -p "${DEB_DIR}/usr/share/icons/hicolor/48x48/apps"
    mkdir -p "${DEB_DIR}/usr/share/icons/hicolor/128x128/apps"
    mkdir -p "${DEB_DIR}/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "${DEB_DIR}/usr/share/icons/hicolor/scalable/apps"
    mkdir -p "${DEB_DIR}/usr/share/metainfo"
    mkdir -p "${DEB_DIR}/usr/share/doc/${APP_NAME}"
    
    # Copy PyInstaller bundle
    print_info "Copying application bundle..."
    if [ -d "dist/CloudarBrowser" ]; then
        cp -r dist/CloudarBrowser/* "${DEB_DIR}/usr/bin/"
    elif [ -f "dist/CloudarBrowser" ]; then
        cp dist/CloudarBrowser "${DEB_DIR}/usr/bin/"
    fi
    
    # Create launcher wrapper script
    cat > "${DEB_DIR}/usr/bin/${APP_NAME}" << 'LAUNCHER'
#!/bin/bash
# Cloudar Browser launcher
DIR="$(dirname "$(readlink -f "$0")")"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR"
export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox --disable-gpu-driver-bug-workarounds"
export QTWEBENGINE_DISABLE_SANDBOX=1
exec "$DIR/CloudarBrowser" "$@"
LAUNCHER
    chmod +x "${DEB_DIR}/usr/bin/${APP_NAME}"
    
    # Create desktop entry
    cat > "${DEB_DIR}/usr/share/applications/${APP_NAME}.desktop" << DESKTOP
[Desktop Entry]
Version=1.0
Name=Cloudar Browser
Comment=A modern, private web browser with privacy tools and performance optimizations
GenericName=Web Browser
Exec=${APP_NAME} %u
Icon=${APP_NAME}
Terminal=false
Type=Application
Categories=Network;WebBrowser;
StartupNotify=true
StartupWMClass=CloudarBrowser
MimeType=text/html;text/xml;application/xhtml+xml;application/xml;application/rss+xml;application/rdf+xml;image/gif;image/jpeg;image/png;x-scheme-handler/http;x-scheme-handler/https;
Keywords=browser;web;internet;
DESKTOP
    
    # Copy icon to various sizes (scalable to default)
    if [ -f "resources/Icon.png" ]; then
        # Use the same icon for all sizes (scaling is done by the desktop environment)
        cp resources/Icon.png "${DEB_DIR}/usr/share/icons/hicolor/48x48/apps/${APP_NAME}.png"
        cp resources/Icon.png "${DEB_DIR}/usr/share/icons/hicolor/128x128/apps/${APP_NAME}.png"
        cp resources/Icon.png "${DEB_DIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
        print_info "Icon copied"
    fi
    
    # Create AppStream metainfo
    cat > "${DEB_DIR}/usr/share/metainfo/${APP_NAME}.appdata.xml" << APPDATA
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>${APP_NAME}</id>
  <name>Cloudar Browser</name>
  <summary>A modern, private web browser</summary>
  <description>
    <p>Cloudar Browser features a customizable New Tab page, privacy tools, and performance optimizations for a modern browsing experience.</p>
  </description>
  <url type="homepage">https://example.com/cloudar</url>
  <project_license>MIT</project_license>
  <metadata_license>MIT</metadata_license>
  <developer_name>Cloudar Team</developer_name>
  <releases>
    <release version="${APP_VERSION}" date="2024-01-01"/>
  </releases>
  <categories>
    <category>Network</category>
    <category>WebBrowser</category>
  </categories>
  <keywords>
    <keyword>browser</keyword>
    <keyword>web</keyword>
    <keyword>internet</keyword>
    <keyword>privacy</keyword>
  </keywords>
</component>
APPDATA
    
    # Create changelog
    cat > "${DEB_DIR}/usr/share/doc/${APP_NAME}/changelog" << CHANGELOG
cloudar-browser (1.0.0-1) stable; urgency=medium

  * Initial release.
  * Features include modern web browsing, privacy tools, and performance optimizations.

 -- Cloudar Team <contact@example.com>  Thu, 01 Jan 2024 00:00:00 +0000
CHANGELOG
    gzip -9 -n "${DEB_DIR}/usr/share/doc/${APP_NAME}/changelog"
    
    # Create copyright file
    cat > "${DEB_DIR}/usr/share/doc/${APP_NAME}/copyright" << COPYRIGHT
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Cloudar Browser
Upstream-Contact: Cloudar Team <contact@example.com>
Source: https://example.com/cloudar

Files: *
Copyright: 2024 Cloudar Team
License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 .
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 .
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
COPYRIGHT
    
    print_info "Package structure created at ${DEB_DIR}"
}

# Create DEBIAN control file
create_control() {
    print_header "Creating package control file"
    
    # Get total installed size in KB
    if [ -d "build_deb/${PACKAGE_NAME}" ]; then
        INSTALLED_SIZE=$(du -sk "build_deb/${PACKAGE_NAME}/usr" | cut -f1)
    else
        INSTALLED_SIZE=100000
    fi
    
    DEB_DIR="build_deb/${PACKAGE_NAME}"
    
    cat > "${DEB_DIR}/DEBIAN/control" << CONTROL
Package: ${APP_NAME}
Version: ${APP_VERSION}
Section: web
Priority: optional
Architecture: ${ARCH}
Depends: libc6 (>= 2.17), libglib2.0-0 (>= 2.0), libx11-6, libxcb1, libxkbcommon0, libfontconfig1, libfreetype6, libdbus-1-3, libgcc-s1, libstdc++6, libpcre2-8-0, zlib1g, libegl1, libgl1, libxext6, libxrender1, libxcb-xinerama0, libxcb-xinput0, libxcb-xfixes0, libxcb-shape0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render-util0, libxcb-render0, libxcb-shm0, libxcb-sync1, libxcb-util1 | libxcb-util0, libxcb-xkb1, libxcb-xv0, libx11-xcb1, libxcb-dri3-0, libxcb-present0, libxcb-glx0, libxcb-dri2-0, libice6, libsm6, libxi6, libxtst6, libxrandr2, libxfixes3, libxcursor1, libxcomposite1, libxdamage1, libxxf86vm1, libglib2.0-0, libnss3, libnspr4, libsqlite3-0, libasound2, libpulse0, libpango-1.0-0, libcairo2, libatk1.0-0, libatk-bridge2.0-0, libgdk-pixbuf2.0-0 | libgdk-pixbuf-2.0-0, libgtk-3-0, libdrm2, libgbm1
Recommends: ca-certificates
Maintainer: Cloudar Team <contact@example.com>
Description: A modern, private web browser
 Cloudar Browser is a modern web browser built with PyQt6 and QtWebEngine.
 It features a customizable New Tab page, privacy tools, ad blocking,
 and performance optimizations for a smooth browsing experience.
 .
 Features:
  * Customizable new tab page with backgrounds
  * Built-in ad blocker
  * Privacy-focused with do-not-track support
  * Tab management and session saving
  * YouTube video downloading
  * Multi-language support
  * Bookmark manager
  * Download manager
  * Extension support
  * Performance monitoring
CONTROL
    
    # Create conffiles (configuration files)
    # (none for now, config is in user's home dir)
    
    # Create preinst/postinst scripts if needed
    cat > "${DEB_DIR}/DEBIAN/postinst" << 'POSTINST'
#!/bin/bash
set -e

# Update desktop database if available
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database -q || true
fi

# Update icon cache if available
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi

echo "Cloudar Browser installed successfully!"
echo "You can launch it from your application menu or run 'cloudar-browser' in terminal."
POSTINST
    chmod +x "${DEB_DIR}/DEBIAN/postinst"
    
    cat > "${DEB_DIR}/DEBIAN/postrm" << 'POSTRM'
#!/bin/bash
set -e

# Update desktop database if available
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database -q || true
fi

# Update icon cache if available
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
POSTRM
    chmod +x "${DEB_DIR}/DEBIAN/postrm"
    
    print_info "Control file created"
}

# Build the .deb package
build_deb() {
    print_header "Building .deb package"
    
    DEB_DIR="build_deb/${PACKAGE_NAME}"
    
    # Fix permissions
    print_info "Setting correct permissions..."
    find "${DEB_DIR}/usr/bin" -type f -exec chmod 755 {} \;
    find "${DEB_DIR}/usr/share" -type f -exec chmod 644 {} \;
    
    # Ensure ownership
    chown -R root:root "${DEB_DIR}" 2>/dev/null || true
    
    # Build the package
    print_info "Running dpkg-deb..."
    dpkg-deb --build --root-owner-group "${DEB_DIR}" "${SCRIPT_DIR}/${PACKAGE_NAME}.deb"
    
    if [ -f "${SCRIPT_DIR}/${PACKAGE_NAME}.deb" ]; then
        print_info ".deb package created successfully!"
        ls -lh "${SCRIPT_DIR}/${PACKAGE_NAME}.deb"
    else
        print_error "Failed to create .deb package"
        exit 1
    fi
}

# Verify the package
verify_package() {
    print_header "Verifying package"
    
    DEB_FILE="${SCRIPT_DIR}/${PACKAGE_NAME}.deb"
    
    if [ ! -f "$DEB_FILE" ]; then
        print_error "Package file not found: $DEB_FILE"
        exit 1
    fi
    
    # Check package info
    print_info "Package info:"
    dpkg-deb --info "$DEB_FILE"
    
    echo ""
    print_info "Package contents (top-level):"
    dpkg-deb --contents "$DEB_FILE" | awk '{print $NF}' | head -20
    echo "..."
    dpkg-deb --contents "$DEB_FILE" | awk '{print $NF}' | tail -10
    
    echo ""
    echo "Total files: $(dpkg-deb --contents "$DEB_FILE" | wc -l)"
}

# Main execution
main() {
    print_header "Cloudar Browser .deb Package Builder"
    echo "App:     ${APP_NAME}"
    echo "Version: ${APP_VERSION}"
    echo "Arch:    ${ARCH}"
    echo ""
    
    # Activate virtual environment if exists
    if [ -d "venv" ]; then
        print_info "Activating virtual environment..."
        source venv/bin/activate
    fi
    
    check_prereqs
    clean_build
    bundle_with_pyinstaller
    create_deb_structure
    create_control
    build_deb
    verify_package
    
    print_header "Build Complete"
    echo ""
    echo "Package: ${SCRIPT_DIR}/${PACKAGE_NAME}.deb"
    echo "Install with: sudo apt install ./${PACKAGE_NAME}.deb"
    echo "Or:          sudo dpkg -i ${PACKAGE_NAME}.deb && sudo apt-get install -f"
    echo ""
    print_info "Done!"
}

main "$@"