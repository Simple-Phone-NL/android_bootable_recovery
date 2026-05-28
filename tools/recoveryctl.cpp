/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <android-base/logging.h>
#include <android-base/strings.h>

static constexpr const char* RECOVERY_SOCKET = "/dev/socket/recoveryctl";
static constexpr int SOCKET_BUFFER_SIZE = 4096;

bool SendCommand(const std::string& command_line) {
  // Connect to recovery socket
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    PLOG(ERROR) << "Failed to create socket";
    return false;
  }

  struct sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, RECOVERY_SOCKET, sizeof(addr.sun_path) - 1);

  if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    PLOG(ERROR) << "Failed to connect to recovery socket. Make sure recovery is running.";
    close(sock);
    return false;
  }

  // Send command
  if (write(sock, command_line.c_str(), command_line.length()) < 0) {
    PLOG(ERROR) << "Failed to send command";
    close(sock);
    return false;
  }

  // Send EOF marker
  if (write(sock, "\0", 1) < 0) {
    PLOG(ERROR) << "Failed to send EOF marker";
    close(sock);
    return false;
  }

  // Read response
  char buffer[SOCKET_BUFFER_SIZE];
  ssize_t bytes_read = read(sock, buffer, sizeof(buffer) - 1);
  if (bytes_read < 0) {
    PLOG(ERROR) << "Failed to read response";
    close(sock);
    return false;
  }

  buffer[bytes_read] = '\0';
  printf("%s\n", buffer);

  close(sock);

  // Check if response indicates success
  return (strncmp(buffer, "OK:", 3) == 0);
}

void PrintUsage(const char* prog_name) {
  fprintf(stderr,
          "Usage:\n"
          "  %s wipe          - Perform a factory reset (wipe all user data)\n"
          "  %s flash <file>  - Flash an OTA package\n"
          "\n"
          "Note: This tool only works when booted into recovery mode.\n"
          "Example:\n"
          "  adb shell recoveryctl wipe\n"
          "  adb shell recoveryctl flash /sdcard/update.zip\n",
          prog_name, prog_name);
}

int main(int argc, char* argv[]) {
  android::base::InitLogging(argv, android::base::StderrLogger);

  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string command = argv[1];

  if (command == "wipe") {
    if (!SendCommand("wipe")) {
      fprintf(stderr, "Failed to execute wipe command\n");
      return 1;
    }
    return 0;
  } else if (command == "flash") {
    if (argc < 3) {
      fprintf(stderr, "Error: flash requires a file path argument\n");
      PrintUsage(argv[0]);
      return 1;
    }
    std::string flash_cmd = "flash:" + std::string(argv[2]);
    if (!SendCommand(flash_cmd)) {
      fprintf(stderr, "Failed to execute flash command\n");
      return 1;
    }
    return 0;
  } else {
    fprintf(stderr, "Unknown command: %s\n", command.c_str());
    PrintUsage(argv[0]);
    return 1;
  }
}
