# RecoveryCtl Quick Reference

## Installation

1. Build and flash your device:
```bash
m recovery recoveryctl
adb reboot bootloader
fastboot flash recovery out/target/product/<device>/recovery.img
fastboot reboot
```

2. Boot into recovery:
```bash
adb reboot recovery
```

## Commands

### Factory Reset
```bash
adb shell recoveryctl wipe
```

### Flash OTA Package
```bash
adb push update.zip /data/
adb shell recoveryctl flash /data/update.zip
```

## Complete Workflow Examples

### Scenario 1: One-Time Factory Reset
```bash
adb reboot recovery
adb shell recoveryctl wipe
# Device will perform factory reset
```

### Scenario 2: Install OTA Update
```bash
adb reboot recovery
adb push myupdate.zip /sdcard/
adb shell recoveryctl flash /sdcard/myupdate.zip
# Device will install update on next recovery boot
```

### Scenario 3: Automated Script
```bash
#!/bin/bash
adb push update.zip /data/
adb shell recoveryctl flash /data/update.zip
adb reboot recovery
# Monitor with: adb logcat
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "Failed to connect to recovery socket" | Boot into recovery first: `adb reboot recovery` |
| "File not found" | Use absolute path: `/data/update.zip` or `/sdcard/update.zip` |
| "File not readable" | Check permissions: `adb shell ls -la /data/update.zip` |
| Command not in PATH | Device must be running updated recovery image |

## Common Patterns

### Validate Before Pushing
```bash
adb shell recoveryctl flash /nonexistent.zip  # See error
adb push valid.zip /data/
adb shell recoveryctl flash /data/valid.zip   # Succeeds
```

### Verify Queue
```bash
adb reboot recovery
adb shell ls -la /dev/socket/recoveryctl
```

### Monitor Progress
```bash
adb logcat | grep -i recovery
```

## File Locations

- OTA packages: `/data/`, `/sdcard/`, `/cache/`
- Recovery logs: `/cache/recovery/last_log`
- Bootloader message: `/misc` partition
- Recovery socket: `/dev/socket/recoveryctl`

## See Also

- Full documentation: `tools/RECOVERYCTL.md`
- Examples script: `tools/recoveryctl_examples.sh`
- Source code:
  - Client: `tools/recoveryctl.cpp`
  - Server: `tools/recovery_command_server.cpp`
  - Header: `tools/recovery_command_server.h`
