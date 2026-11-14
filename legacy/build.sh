#!/bin/bash
# Build script for Cloudar Browser
# Supports building for Linux, macOS, and Windows using Briefcase

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Configuration
APP_NAME="cloudar"
PLATFORM=""
BUILD_MODE="release"  # or "debug"
CLEAN_BUILD=false
RUN_AFTER_BUILD=false
AUTO_SETUP=false
PACKAGE_APPIMAGE=false

# Print colored message
print_message() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

# Show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --platform PLATFORM    Target platform (linux, macOS, windows)"
    echo "  -d, --debug                Build in debug mode"
    echo "  -c, --clean                Clean build (remove previous builds)"
    echo "  -r, --run                  Run the app after building"
    echo "  --setup                    Auto-setup virtual environment and install dependencies"
    echo "  --appimage                 Build Linux AppImage package"
    echo "  -h, --help                 Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --setup                 Setup virtual environment and install dependencies"
    echo "  $0 -p linux                Build for Linux"
    echo "  $0 -p linux --appimage     Build Linux AppImage"
    echo "  $0 -p macOS -d             Build for macOS in debug mode"
    echo "  $0 -p windows -c -r        Clean build for Windows and run"
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -p|--platform)
                PLATFORM="$2"
                shift 2
                ;;
            -d|--debug)
                BUILD_MODE="debug"
                shift
                ;;
            -c|--clean)
                CLEAN_BUILD=true
                shift
                ;;
            -r|--run)
                RUN_AFTER_BUILD=true
                shift
                ;;
            --setup)
                AUTO_SETUP=true
                shift
                ;;
            --appimage)
                PACKAGE_APPIMAGE=true
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
}

# Auto-setup virtual environment
auto_setup() {
    print_header "Auto-Setup Mode"
    
    if [[ -d "venv" ]]; then
        print_warning "Virtual environment 'venv' already exists."
        read -p "Do you want to recreate it? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            print_message "Removing existing virtual environment..."
            rm -rf venv
        else
            print_message "Using existing virtual environment."
        fi
    fi
    
    if [[ ! -d "venv" ]]; then
        print_message "Creating virtual environment..."
        python3 -m venv venv
    fi
    
    print_message "Activating virtual environment..."
    source venv/bin/activate
    
    print_message "Upgrading pip..."
    pip install --upgrade pip
    
    print_message "Installing Briefcase..."
    pip install briefcase
    
    print_message "Installing project dependencies..."
    if [[ -f "requirements.txt" ]]; then
        pip install -r requirements.txt
    else
        pip install PyQt6 PyQt6-WebEngine Pillow
    fi
    
    print_message ""
    print_message "Setup complete! Virtual environment created at: venv/"
    print_message ""
    print_message "To activate the virtual environment, run:"
    echo -e "  ${YELLOW}source venv/bin/activate${NC}"
    print_message ""
    print_message "To build the app, run:"
    echo -e "  ${YELLOW}./build.sh -p linux${NC}"
    print_message ""
    print_message "Or use the convenience script:"
    echo -e "  ${YELLOW}./run.sh${NC}"
    
    exit 0
}

# Check prerequisites
check_prerequisites() {
    print_header "Checking Prerequisites"
    
    # Check Python
    if ! command -v python3 &> /dev/null; then
        print_error "Python 3 is not installed"
        exit 1
    fi
    print_message "Python version: $(python3 --version)"
    
    # Check pip
    if ! command -v pip3 &> /dev/null; then
        print_error "pip3 is not installed"
        exit 1
    fi
    print_message "pip3 found"
    
    # Check if briefcase is installed
    if ! python3 -c "import briefcase" 2>/dev/null; then
        print_warning "Briefcase is not installed."
        
        # Check if we're in a virtual environment
        if [[ -z "$VIRTUAL_ENV" ]] && [[ -z "$CONDA_DEFAULT_ENV" ]]; then
            if [[ "$AUTO_SETUP" == true ]]; then
                auto_setup
            else
                print_error "This system requires a virtual environment for package installation."
                print_message "Please create a virtual environment first:"
                echo -e "  ${YELLOW}python3 -m venv venv${NC}"
                echo -e "  ${YELLOW}source venv/bin/activate${NC}"
                echo -e "  ${YELLOW}./build.sh $@${NC}"
                echo ""
                print_message "Or use the --setup flag for automatic setup:"
                echo -e "  ${YELLOW}./build.sh --setup${NC}"
                echo ""
                print_message "Or run with --break-system-packages flag (not recommended):"
                echo -e "  ${YELLOW}pip3 install briefcase --break-system-packages${NC}"
                exit 1
            fi
        else
            print_message "Virtual environment detected. Installing Briefcase..."
            pip3 install briefcase
        fi
    else
        print_message "Briefcase is installed"
    fi
    
    # Check platform
    if [[ -z "$PLATFORM" ]]; then
        # Auto-detect platform
        case "$(uname -s)" in
            Linux*)
                PLATFORM="linux"
                ;;
            Darwin*)
                PLATFORM="macOS"
                ;;
            CYGWIN*|MINGW*|MSYS*)
                PLATFORM="windows"
                ;;
            *)
                print_error "Unable to detect platform. Please specify with -p flag"
                exit 1
                ;;
        esac
        print_message "Auto-detected platform: $PLATFORM"
    else
        print_message "Target platform: $PLATFORM"
    fi
}

# Install dependencies
install_dependencies() {
    print_header "Installing Dependencies"
    
    # Check if we're in a virtual environment
    if [[ -z "$VIRTUAL_ENV" ]] && [[ -z "$CONDA_DEFAULT_ENV" ]]; then
        print_warning "Not in a virtual environment. Skipping dependency installation."
        print_message "Please ensure dependencies are installed:"
        echo -e "  ${YELLOW}pip3 install -r requirements.txt${NC}"
        echo ""
        print_message "Or create a virtual environment:"
        echo -e "  ${YELLOW}python3 -m venv venv${NC}"
        echo -e "  ${YELLOW}source venv/bin/activate${NC}"
        echo -e "  ${YELLOW}pip3 install -r requirements.txt${NC}"
    else
        if [[ -f "requirements.txt" ]]; then
            print_message "Installing from requirements.txt..."
            pip3 install -r requirements.txt
        else
            print_warning "requirements.txt not found, installing basic dependencies..."
            pip3 install PyQt6 PyQt6-WebEngine Pillow
        fi
        print_message "Dependencies installed successfully"
    fi
}

# Clean build artifacts
clean_build() {
    print_header "Cleaning Build Artifacts"
    
    if [[ "$CLEAN_BUILD" == true ]]; then
        print_message "Cleaning previous builds..."
        rm -rf build/
        rm -rf .briefcase/
        rm -rf dist/
        print_message "Build artifacts cleaned"
    else
        print_message "Skipping clean (use -c flag to clean)"
    fi
}

# Create briefcase project if needed
setup_briefcase() {
    print_header "Setting up Briefcase"
    
    if [[ ! -d ".briefcase" ]]; then
        print_message "Initializing Briefcase project..."
        briefcase create "$PLATFORM" --no-input
    else
        print_message "Briefcase project already exists"
    fi
}

# Build the application
build_app() {
    print_header "Building Application ($BUILD_MODE mode)"
    
    if [[ "$BUILD_MODE" == "debug" ]]; then
        print_message "Building in DEBUG mode..."
        briefcase build "$PLATFORM"
    else
        print_message "Building in RELEASE mode..."
        briefcase build "$PLATFORM"
    fi
    
    print_message "Build completed successfully!"
}

# Run the application
run_app() {
    if [[ "$RUN_AFTER_BUILD" == true ]]; then
        print_header "Running Application"
        print_message "Starting Cloudar Browser..."
        briefcase run "$PLATFORM"
    fi
}

# Package as AppImage
package_appimage() {
    if [[ "$PACKAGE_APPIMAGE" == true ]] && [[ "$PLATFORM" == "linux" ]]; then
        print_header "Packaging Linux AppImage"
        
        # Check if briefcase package is available
        if command -v briefcase &> /dev/null; then
            print_message "Creating AppImage package..."
            briefcase package linux
            
            # Find the generated AppImage
            APPIMAGE_PATH=$(find build -name "*.AppImage" -type f 2>/dev/null | head -n 1)
            
            if [[ -n "$APPIMAGE_PATH" ]]; then
                print_message "AppImage created successfully!"
                print_message "Location: $APPIMAGE_PATH"
                
                # Make it executable
                chmod +x "$APPIMAGE_PATH"
                
                print_message ""
                print_message "To run the AppImage:"
                echo -e "  ${YELLOW}./$APPIMAGE_PATH${NC}"
            else
                print_warning "AppImage not found in expected location."
                print_message "Check the build/linux/ directory for the package."
            fi
        else
            print_error "Briefcase command not found. Cannot create AppImage."
        fi
    fi
}

# Show build output location
show_build_info() {
    print_header "Build Information"
    
    case "$PLATFORM" in
        linux)
            print_message "Linux build location: build/linux/x86_64/"
            ;;
        macOS)
            print_message "macOS build location: build/macOS/app/"
            ;;
        windows)
            print_message "Windows build location: build/windows/amd64/"
            ;;
    esac
    
    print_message ""
    print_message "To run the app manually:"
    echo -e "  ${YELLOW}briefcase run $PLATFORM${NC}"
    print_message ""
    print_message "To create an installer/package:"
    echo -e "  ${YELLOW}briefcase package $PLATFORM${NC}"
}

# Main execution
main() {
    print_header "Cloudar Browser Build Script"
    
    parse_args "$@"
    
    # If --setup flag is used, run auto-setup and exit
    if [[ "$AUTO_SETUP" == true ]]; then
        auto_setup
    fi
    
    check_prerequisites
    install_dependencies
    clean_build
    setup_briefcase
    build_app
    run_app
    package_appimage
    show_build_info
    
    print_header "Build Process Complete"
    print_message "Thank you for using Cloudar Browser build script!"
}

# Run main function
main "$@"