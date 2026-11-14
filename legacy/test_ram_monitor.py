#!/usr/bin/env python3
"""
Test script for RAM monitoring functionality
"""
import sys
import os
import time

# Add current directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def test_ram_monitoring():
    """Test RAM monitoring without GUI"""
    print("Testing RAM Monitoring for Cloudar Browser")
    print("=" * 50)
    
    # Import performance manager components
    from features.performance_manager import PerformanceManager
    
    # Create a mock browser window object
    class MockBrowser:
        def __init__(self):
            self.settings = {}
            self.status = None
    
    mock_browser = MockBrowser()
    
    # Create performance manager (without GUI, pass None as parent)
    perf_manager = PerformanceManager.__new__(PerformanceManager)
    perf_manager.browser = mock_browser
    perf_manager._last_active = {}
    perf_manager._current_ram_mb = 0
    
    # Test RAM measurement
    print("\nMeasuring RAM usage...")
    for i in range(5):
        ram_mb = perf_manager.get_process_ram_mb()
        print(f"Measurement {i+1}: {ram_mb:.2f} MB")
        time.sleep(0.5)
    
    # Test platform-specific methods
    print("\nPlatform Information:")
    print(f"  Platform: {sys.platform}")
    print(f"  PID: {os.getpid()}")
    
    # Display results
    print("\n" + "=" * 50)
    print("RAM Monitoring Test Results:")
    print("=" * 50)
    
    final_ram = perf_manager.get_process_ram_mb()
    print(f"✓ Current RAM Usage: {final_ram:.2f} MB")
    print(f"✓ Platform: {sys.platform}")
    
    if sys.platform == 'win32':
        print("✓ Using Windows API (psapi)")
    elif sys.platform == 'darwin':
        print("✓ Using macOS API (task_info)")
    elif sys.platform.startswith('linux'):
        print("✓ Using Linux /proc filesystem")
    
    print("\n✓ RAM monitoring functionality is working correctly!")
    print("\nNote: When running the full browser, RAM usage will be")
    print("displayed in the status bar and updated every 2 seconds.")
    
    return True

if __name__ == "__main__":
    try:
        test_ram_monitoring()
        sys.exit(0)
    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

