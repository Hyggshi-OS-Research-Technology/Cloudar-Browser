# RAM Monitoring for Cloudar Browser™

## Overview

The Cloudar Browser now includes real-time RAM monitoring that measures and displays the memory usage of the Cloudar process separately. This feature helps users track the browser's memory consumption in real-time.

## Features

- **Real-time Monitoring**: RAM usage is updated every 2 seconds
- **Cross-Platform Support**: Works on Windows, macOS, and Linux
- **Status Bar Display**: Shows RAM usage directly in the browser's status bar
- **Accurate Measurement**: Uses platform-specific APIs for precise memory readings

## Implementation Details

### Platform-Specific Methods

#### Linux
- Reads from `/proc/{pid}/status`
- Extracts `VmRSS` (Virtual Memory Resident Set Size)
- Converts from kB to MB

#### Windows
- Uses Windows API via `ctypes`
- Calls `GetProcessMemoryInfo` from `psapi.dll`
- Reads `WorkingSetSize` (physical memory in use)
- Converts from bytes to MB

#### macOS
- Uses `task_info` from macOS kernel
- Reads `resident_size` (resident memory)
- Converts from bytes to MB

### Code Location

The RAM monitoring functionality is implemented in:
- **File**: `features/performance_manager.py`
- **Class**: `PerformanceManager`
- **Key Methods**:
  - `get_process_ram_mb()` - Main method to get RAM usage
  - `_get_ram_linux(pid)` - Linux-specific implementation
  - `_get_ram_windows(pid)` - Windows-specific implementation
  - `_get_ram_macos(pid)` - macOS-specific implementation
  - `update_ram_display()` - Updates the status bar label
  - `get_current_ram_mb()` - Returns the last measured value

### Display

The RAM usage is displayed in the status bar with:
- **Format**: `RAM: X.X MB`
- **Update Frequency**: Every 2 seconds
- **Tooltip**: Shows detailed information on hover
- **Styling**: Uses the `#RAMStatus` style from `core/styles.py`

## Usage

### Automatic Display

When running Cloudar Browser, the RAM usage is automatically displayed in the status bar at the bottom-right corner of the window, next to the AdBlock counter.

### Programmatic Access

You can access the RAM usage programmatically:

```python
# Get the performance manager from the browser window
perf_manager = browser_window.performance_manager

# Get current RAM usage in MB
ram_mb = perf_manager.get_current_ram_mb()
print(f"Current RAM usage: {ram_mb:.2f} MB")
```

## Testing

A test script is provided to verify the RAM monitoring functionality:

```bash
python3 test_ram_monitor.py
```

This script will:
1. Measure RAM usage 5 times with 0.5-second intervals
2. Display platform information
3. Verify the measurement is working correctly

## Technical Notes

### Memory Metrics

The implementation uses **Working Set Size** (Windows) or **VmRSS** (Linux/macOS), which represents the actual physical memory currently in use by the process. This is different from:
- **Virtual Memory**: Includes swapped/paged memory
- **Peak Working Set**: Maximum memory usage ever reached

### Performance Impact

- **Minimal Overhead**: Reading from `/proc` or using system APIs is very fast
- **Update Frequency**: 2-second intervals balance accuracy with performance
- **No External Dependencies**: Uses only Python standard library and `ctypes`

### Accuracy Considerations

- **Linux**: VmRSS is accurate for resident memory
- **Windows**: WorkingSetSize reflects physical memory in use
- **macOS**: resident_size is the actual resident memory
- All methods exclude shared libraries that are counted separately by the OS

## Troubleshooting

### RAM shows 0.0 MB

If the RAM display shows 0.0 MB:
1. Check that the process has permission to read memory info
2. Verify the platform is supported (Windows, macOS, or Linux)
3. Check the debug log for errors

### Platform Not Supported

The current implementation supports:
- Windows (via psapi)
- macOS (via task_info)
- Linux (via /proc filesystem)

Other platforms will return 0.0 MB.

## Future Enhancements

Possible improvements:
- Add memory usage graphs
- Show per-tab memory usage (if Qt WebEngine provides API)
- Add memory warnings when usage exceeds thresholds
- Include swap memory usage
- Add export functionality for memory logs

## Related Files

- `features/performance_manager.py` - Main implementation
- `core/styles.py` - Styling for RAM label
- `test_ram_monitor.py` - Test script
- `docs/RAM_MONITORING.md` - This documentation

</parameter>
<task_progress>
- [x] Analyze the application structure and entry point
- [x] Check existing performance monitoring code
- [x] Check project dependencies
- [x] Understand the browser window structure
- [x] Implement RAM monitoring functionality
- [x] Add styling for RAM label
- [x] Test the implementation
- [x] Create documentation
</task_progress>
</write_to_file>