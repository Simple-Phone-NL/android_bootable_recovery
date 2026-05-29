# ADB automation mode

Automation mode exposes **sideload**, **factory reset (wipe data)**, and **reboot** over USB through `minadbd`, in a single session without using the recovery menu.

## Entering automation mode

1. **From the recovery menu** (userdebug / eng builds): *Advanced* → *Enter ADB automation*
2. **On boot** with recovery argument `--automation` (bootloader control block / `recovery` command line)
3. **From the host** (when your `adb` supports it): `adb reboot automation`  
   Host `adb` must pass `--automation` in the recovery BCB (patch `system/core/adb` if needed).

Disable automation on a product by setting `ro.recovery.adb_automation=0` (default is `1` in this tree).

## Host commands

After entering automation mode, `adb devices` should show the device in **sideload** state.

| Action | Command |
|--------|---------|
| Install OTA | `adb sideload /path/to/package.zip` |
| Reboot to system | `adb reboot` |
| Reboot to bootloader | `adb reboot bootloader` |
| Reboot to recovery | `adb reboot recovery` |
| Reboot to fastboot | `adb reboot fastboot` |
| Factory reset | Open ADB service `wipe-data:userdata:8` or `rescue-wipe:userdata:8` (see below) |

### Factory reset over ADB

Wipe uses the same minadbd service protocol as rescue mode:

- Service name: `wipe-data:userdata:<message-size>` or `rescue-wipe:userdata:<message-size>`
- `<message-size>` must be at least `8` (length of the success token the host reads back)

Host-side `adb` does not ship a user-facing `adb wipe` command; use a small client that opens the service (as rescue tests do), or wipe from Android with:

```bash
adb reboot recovery --wipe_data
```

## Compared to other modes

| Mode | Sideload | Wipe data | Reboot | Cancel menu |
|------|----------|-----------|--------|-------------|
| Sideload (`--sideload`) | Yes | No | Yes | Optional |
| Rescue (`--rescue`) | No* | Yes | Yes | No |
| Automation (`--automation`) | Yes | Yes | Yes | No |

\* Rescue uses `rescue-install:` instead of `adb sideload`.

## Implementation notes

- Recovery forks `/system/bin/minadbd --socket_fd … --automation`
- `ApplyFromAdb()` uses `AdbInteractionMode::kAutomation` with install, wipe, noop, and reboot commands
- Only one of `adbd` or `minadbd` is active at a time; USB switches to sideload configuration
