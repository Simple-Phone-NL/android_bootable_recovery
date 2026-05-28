#!/bin/bash
# RecoveryCtl Usage Examples
# These are shell scripts demonstrating common use cases for recoveryctl

# ============================================================================
# Example 1: Simple Factory Reset
# ============================================================================
factory_reset() {
    echo "Starting factory reset..."
    adb shell recoveryctl wipe
    echo "Rebooting to recovery..."
    adb reboot recovery
}

# ============================================================================
# Example 2: Flash OTA Update
# ============================================================================
flash_ota() {
    local ota_file=$1
    
    if [ ! -f "$ota_file" ]; then
        echo "Error: OTA file not found: $ota_file"
        exit 1
    fi
    
    echo "Pushing OTA file to device..."
    adb push "$ota_file" /data/update.zip
    
    echo "Queuing flash operation..."
    adb shell recoveryctl flash /data/update.zip
    
    echo "Rebooting to recovery..."
    adb reboot recovery
}

# ============================================================================
# Example 3: Batch Update Multiple Devices
# ============================================================================
batch_update_devices() {
    local ota_file=$1
    
    if [ ! -f "$ota_file" ]; then
        echo "Error: OTA file not found: $ota_file"
        exit 1
    fi
    
    # Get list of connected devices
    local devices=$(adb devices | grep -v "^List" | awk '{print $1}' | grep -v '^$')
    
    if [ -z "$devices" ]; then
        echo "No devices connected"
        exit 1
    fi
    
    echo "Found devices: $devices"
    
    for device in $devices; do
        echo "Updating device: $device"
        
        # Push update to device
        adb -s $device push "$ota_file" /data/update.zip
        
        # Queue flash operation
        adb -s $device shell recoveryctl flash /data/update.zip
        
        # Reboot to recovery
        adb -s $device reboot recovery
        
        echo "Device $device will update on reboot"
    done
    
    echo "All devices queued for update"
}

# ============================================================================
# Example 4: Verify Command Queue
# ============================================================================
verify_command_queue() {
    echo "Checking if recovery is running..."
    
    # Try to connect to recovery socket
    local response=$(adb shell ls -la /dev/socket/recoveryctl 2>&1)
    
    if [ $? -eq 0 ]; then
        echo "Recovery socket is available"
        echo "$response"
    else
        echo "Recovery is not running or socket is not available"
        echo "Boot into recovery: adb reboot recovery"
    fi
}

# ============================================================================
# Example 5: Safe Factory Reset with Confirmation
# ============================================================================
safe_factory_reset() {
    echo "WARNING: Factory reset will erase all user data!"
    echo -n "Are you sure? Type 'yes' to continue: "
    read confirmation
    
    if [ "$confirmation" != "yes" ]; then
        echo "Cancelled"
        exit 0
    fi
    
    echo "Executing factory reset..."
    adb shell recoveryctl wipe
    adb reboot recovery
}

# ============================================================================
# Example 6: OTA Update with Progress Reporting
# ============================================================================
ota_update_with_progress() {
    local ota_file=$1
    
    if [ ! -f "$ota_file" ]; then
        echo "Error: OTA file not found: $ota_file"
        exit 1
    fi
    
    echo "=== OTA Update Process ==="
    
    echo "[1/4] Pushing OTA file..."
    adb push "$ota_file" /data/update.zip || exit 1
    
    echo "[2/4] Queueing flash operation..."
    adb shell recoveryctl flash /data/update.zip || exit 1
    
    echo "[3/4] Rebooting to recovery..."
    adb reboot recovery
    
    echo "[4/4] Update in progress..."
    echo "Device will reboot automatically when complete"
    echo "You can monitor progress with: adb logcat"
}

# ============================================================================
# Example 7: Roll-out to Device Fleet
# ============================================================================
fleet_rollout() {
    local ota_file=$1
    local max_concurrent=${2:-3}
    
    if [ ! -f "$ota_file" ]; then
        echo "Error: OTA file not found: $ota_file"
        exit 1
    fi
    
    local devices=$(adb devices | grep -v "^List" | awk '{print $1}' | grep -v '^$')
    local count=0
    
    echo "Starting fleet rollout with max $max_concurrent concurrent updates"
    echo "Devices to update: $(echo $devices | wc -w)"
    
    for device in $devices; do
        # Limit concurrent updates
        while [ $(jobs -r | wc -l) -ge $max_concurrent ]; do
            sleep 2
        done
        
        {
            echo "Updating $device..."
            adb -s $device push "$ota_file" /data/update.zip 2>/dev/null
            adb -s $device shell recoveryctl flash /data/update.zip 2>/dev/null
            adb -s $device reboot recovery 2>/dev/null
            echo "Device $device queued for update"
        } &
        
        ((count++))
    done
    
    wait
    echo "All $count devices queued for update"
}

# ============================================================================
# Example 8: Automated Testing Workflow
# ============================================================================
test_workflow() {
    echo "=== Recovery Test Workflow ==="
    
    echo "1. Verifying recovery is accessible..."
    adb shell recoveryctl wipe > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "ERROR: Recovery not accessible"
        exit 1
    fi
    
    echo "2. Recovery is accessible"
    echo "3. Testing would proceed here..."
    echo "4. Test complete"
}

# ============================================================================
# Main Script - Argument Parser
# ============================================================================
main() {
    if [ $# -lt 1 ]; then
        cat << EOF
RecoveryCtl Usage Examples

Usage: $0 <command> [options]

Commands:
  factory_reset              - Perform a factory reset
  flash_ota <ota_file>      - Flash an OTA update
  batch_update <ota_file>   - Update all connected devices
  verify_queue              - Verify recovery command socket
  safe_reset                - Factory reset with confirmation
  ota_progress <ota_file>   - OTA with progress reporting
  fleet_rollout <ota_file>  - Update device fleet (max 3 concurrent)
  test                      - Run test workflow

Examples:
  $0 factory_reset
  $0 flash_ota update.zip
  $0 batch_update update.zip
  $0 verify_queue
  $0 safe_reset
  $0 ota_progress update.zip
  $0 fleet_rollout update.zip 5
  $0 test

EOF
        exit 1
    fi
    
    case "$1" in
        factory_reset)
            factory_reset
            ;;
        flash_ota)
            if [ $# -lt 2 ]; then
                echo "Error: OTA file required"
                exit 1
            fi
            flash_ota "$2"
            ;;
        batch_update)
            if [ $# -lt 2 ]; then
                echo "Error: OTA file required"
                exit 1
            fi
            batch_update_devices "$2"
            ;;
        verify_queue)
            verify_command_queue
            ;;
        safe_reset)
            safe_factory_reset
            ;;
        ota_progress)
            if [ $# -lt 2 ]; then
                echo "Error: OTA file required"
                exit 1
            fi
            ota_update_with_progress "$2"
            ;;
        fleet_rollout)
            if [ $# -lt 2 ]; then
                echo "Error: OTA file required"
                exit 1
            fi
            fleet_rollout "$2" "${3:-3}"
            ;;
        test)
            test_workflow
            ;;
        *)
            echo "Unknown command: $1"
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
