# RecoveryCtl - Command Line Interface for Android Recovery

## Overview

`recoveryctl` is a command-line tool that allows you to interact with Android recovery from the shell. It communicates with the recovery daemon via a Unix domain socket (`/dev/socket/recoveryctl`) to queue recovery operations that will be executed on the next boot into recovery mode.

## Prerequisites

- Device must be running Android recovery
- Recovery must have been built with the updated recovery binary that includes the RecoveryCommandServer
- ADB access to the device

## Usage

### Commands

#### Wipe Data
Perform a factory reset (wipe all user data and cache):

```bash
adb shell recoveryctl wipe
```

This will:
1. Queue a factory reset operation
2. Return immediately with a confirmation message
3. On next boot into recovery, the data wipe will be performed

#### Flash/Install OTA Package
Flash an OTA package or software update:

```bash
adb shell recoveryctl flash /path/to/update.zip
```

This will:
1. Validate the package exists and is readable
2. Queue the installation with recovery
3. Return immediately with a confirmation message
4. On next boot into recovery, the package will be installed

### Error Handling

The tool validates commands and returns appropriate error messages:

```bash
# File not found
$ adb shell recoveryctl flash /data/nonexistent.zip
ERROR: File not found

# File not readable
$ adb shell recoveryctl flash /sdcard/protected.zip
ERROR: File not readable

# Invalid command
$ adb shell recoveryctl invalid
Unknown command: invalid
```

## How It Works

1. **Command Reception**: When you run `recoveryctl` with a command, it connects to the recovery socket (`/dev/socket/recoveryctl`)

2. **Message Format**: The command is sent as a text string:
   - Wipe: `"wipe"`
   - Flash: `"flash:/path/to/file.zip"`

3. **Bootloader Message**: The recovery daemon uses the bootloader control block (BCB) at `/misc/bootloader_message` to communicate with recovery

4. **Execution**: On next boot into recovery:
   - `--wipe_data` command performs factory reset
   - `--update_package=path` command installs the OTA package

5. **Cleanup**: Recovery automatically processes and clears the command after execution

## Architecture

### Components

1. **recoveryctl** (client)
   - Location: `tools/recoveryctl.cpp`
   - Function: Parses command-line arguments and sends them to the recovery daemon
   - Binary name: `recoveryctl`

2. **RecoveryCommandServer** (server)
   - Location: `tools/recovery_command_server.{h,cpp}`
   - Function: Listens for commands on Unix socket and processes them
   - Library name: `librecovery_command_server`
   - Runs in recovery daemon process

### Message Flow

```
User Shell (adb shell)
    ↓
recoveryctl (client) - connects to socket
    ↓
/dev/socket/recoveryctl
    ↓
RecoveryCommandServer (in recovery daemon)
    ↓
Bootloader Message (/misc)
    ↓
Next Boot to Recovery
    ↓
Recovery executes command
```

## Building

The tool is built as part of the recovery binary. Ensure your `Android.bp` includes:
- `tools/Android.bp` with `recoveryctl` and `librecovery_command_server` targets
- Recovery binary depends on `librecovery_command_server`

To build just recoveryctl:
```bash
m recoveryctl
```

## Limitations

- Commands only work when recovery is running
- Commands are queued via bootloader message, not executed immediately
- Maximum file path length is limited by bootloader message size (typically 32 bytes for commands)
- Only supports wipe and flash operations (extensible for more commands)

## Security Considerations

- Socket is world-readable/writable (`0666` permissions) by design for ADB access
- Validation of file paths is performed (existence and readability checks)
- Bootloader message changes are persisted to prevent loss of commands

## Future Enhancements

- Add more commands (e.g., `mount`, `unmount`, `sideload`)
- Add command status tracking
- Support for scripting multiple operations
- Immediate execution mode (if recovery becomes more responsive)
- Progress reporting during operations

## Example Workflows

### Automated Factory Reset
```bash
#!/bin/bash
adb shell recoveryctl wipe
adb reboot recovery
```

### Automated OTA Update
```bash
#!/bin/bash
# Push update to device
adb push update.zip /data/update.zip

# Queue installation
adb shell recoveryctl flash /data/update.zip

# Reboot to recovery
adb reboot recovery
```

### Batch Device Management
```bash
for device in $(adb devices | grep -v List | awk '{print $1}'); do
    adb -s $device shell recoveryctl wipe
done
adb shell reboot recovery
```

## Troubleshooting

### "Failed to connect to recovery socket"
- Recovery is not currently running
- Try rebooting into recovery: `adb reboot recovery`

### "File not found"
- Verify the file path exists on the device
- Use full absolute paths (e.g., `/sdcard/update.zip`, not `~/update.zip`)

### "File not readable"
- Check file permissions on the device
- Ensure the file is accessible from the recovery environment

### Command doesn't execute on reboot
- Verify recovery booted successfully
- Check recovery logs: `adb shell cat /cache/recovery/last_log`

## See Also

- Recovery documentation: `README.md` in recovery root
- Bootloader Message: `bootloader_message/` directory
- OTA utilities: `otautil/` directory
