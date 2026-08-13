from PyQt6.QtGui import QImage, QGuiApplication
import sys
import os

# Create QGuiApplication to ensure image plugins are loaded
app = QGuiApplication(sys.argv)

icon_path = "resources/Icon.ico"
png_path = "resources/Icon.png"

if not os.path.exists(icon_path):
    print(f"Error: {icon_path} not found")
    sys.exit(1)

image = QImage(icon_path)
if image.isNull():
    print("Failed to load Icon.ico")
    sys.exit(1)

# Save as PNG
success = image.save(png_path, "PNG")

if success:
    print(f"Successfully converted {icon_path} to {png_path}")
    sys.exit(0)
else:
    print(f"Failed to save {png_path}")
    sys.exit(1)
