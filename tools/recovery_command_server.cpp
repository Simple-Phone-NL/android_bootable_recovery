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

#include "recovery_command_server.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <thread>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <bootloader_message/bootloader_message.h>

static constexpr const char* RECOVERY_SOCKET = "/dev/socket/recoveryctl";
static constexpr int SOCKET_BACKLOG = 4;
static constexpr int SOCKET_BUFFER_SIZE = 4096;
static constexpr const char* RECOVERY_COMMAND_FILE = "/cache/recovery/command";

RecoveryCommandServer::RecoveryCommandServer() = default;

RecoveryCommandServer::~RecoveryCommandServer() {
  Stop();
}

bool RecoveryCommandServer::Start() {
  if (is_running_) {
    LOG(WARNING) << "Command server is already running";
    return true;
  }

  // Create Unix domain socket
  server_socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_socket_ < 0) {
    PLOG(ERROR) << "Failed to create Unix socket";
    return false;
  }

  // Remove existing socket file if present
  unlink(RECOVERY_SOCKET);

  struct sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, RECOVERY_SOCKET, sizeof(addr.sun_path) - 1);

  if (bind(server_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    PLOG(ERROR) << "Failed to bind socket";
    close(server_socket_);
    server_socket_ = -1;
    return false;
  }

  // Set proper permissions for the socket
  if (chmod(RECOVERY_SOCKET, 0666) < 0) {
    PLOG(WARNING) << "Failed to set socket permissions";
  }

  if (listen(server_socket_, SOCKET_BACKLOG) < 0) {
    PLOG(ERROR) << "Failed to listen on socket";
    close(server_socket_);
    server_socket_ = -1;
    return false;
  }

  is_running_ = true;
  should_stop_ = false;

  server_thread_ = std::thread(&RecoveryCommandServer::ServerLoop, this);
  LOG(INFO) << "Recovery command server started on " << RECOVERY_SOCKET;

  return true;
}

void RecoveryCommandServer::Stop() {
  if (!is_running_) {
    return;
  }

  should_stop_ = true;

  if (server_socket_ >= 0) {
    close(server_socket_);
    server_socket_ = -1;
  }

  if (server_thread_.joinable()) {
    server_thread_.join();
  }

  unlink(RECOVERY_SOCKET);
  is_running_ = false;
  LOG(INFO) << "Recovery command server stopped";
}

bool RecoveryCommandServer::IsRunning() const {
  return is_running_;
}

void RecoveryCommandServer::ServerLoop() {
  LOG(INFO) << "Command server loop started";

  while (!should_stop_) {
    int client_fd = accept(server_socket_, nullptr, nullptr);
    if (client_fd < 0) {
      if (should_stop_) {
        break;
      }
      PLOG(ERROR) << "Accept failed";
      continue;
    }

    LOG(INFO) << "Client connected";
    HandleCommand(client_fd);
    close(client_fd);
  }

  LOG(INFO) << "Command server loop ended";
}

void RecoveryCommandServer::HandleCommand(int client_fd) {
  char buffer[SOCKET_BUFFER_SIZE] = {};
  ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

  if (bytes_read <= 0) {
    PLOG(ERROR) << "Failed to read from client";
    return;
  }

  buffer[bytes_read] = '\0';
  std::string command_str(buffer);

  LOG(INFO) << "Received command: " << command_str;

  std::string response = ExecuteCommand(command_str);

  if (write(client_fd, response.c_str(), response.length()) < 0) {
    PLOG(ERROR) << "Failed to write response";
  }
}

std::string RecoveryCommandServer::ExecuteCommand(const std::string& command_str) {
  if (command_str == "wipe") {
    return HandleWipe();
  } else if (android::base::StartsWith(command_str, "flash:")) {
    std::string file_path = command_str.substr(6);  // Remove "flash:" prefix
    return HandleFlash(file_path);
  } else {
    return "ERROR: Unknown command";
  }
}

std::string RecoveryCommandServer::HandleWipe() {
  LOG(INFO) << "Executing wipe command";

  // Set the bootloader message to trigger a data wipe on next boot
  bootloader_message boot = {};
  boot.command[0] = '\0';
  boot.status[0] = '\0';
  strlcpy(boot.recovery, "recovery\n--wipe_data\n", sizeof(boot.recovery));

  std::string err;
  if (!write_bootloader_message(boot, &err)) {
    LOG(ERROR) << "Failed to write bootloader message: " << err;
    return std::string("ERROR: ") + err;
  }

  LOG(INFO) << "Wipe command queued, will execute on next boot";
  return "OK: Wipe queued for next recovery boot. Please reboot to recovery.";
}

std::string RecoveryCommandServer::HandleFlash(const std::string& file_path) {
  LOG(INFO) << "Executing flash command with file: " << file_path;

  // Validate file exists and is readable
  if (access(file_path.c_str(), F_OK) != 0) {
    LOG(ERROR) << "File not found: " << file_path;
    return "ERROR: File not found";
  }

  if (access(file_path.c_str(), R_OK) != 0) {
    LOG(ERROR) << "File not readable: " << file_path;
    return "ERROR: File not readable";
  }

  // Set the bootloader message to install the package on next boot
  bootloader_message boot = {};
  boot.command[0] = '\0';
  boot.status[0] = '\0';

  std::string recovery_cmd = std::string("recovery\n--update_package=") + file_path + "\n";
  if (recovery_cmd.length() > sizeof(boot.recovery)) {
    LOG(ERROR) << "File path too long for bootloader message";
    return "ERROR: File path too long";
  }

  strlcpy(boot.recovery, recovery_cmd.c_str(), sizeof(boot.recovery));

  std::string err;
  if (!write_bootloader_message(boot, &err)) {
    LOG(ERROR) << "Failed to write bootloader message: " << err;
    return std::string("ERROR: ") + err;
  }

  LOG(INFO) << "Flash command queued for file: " << file_path;
  return "OK: Flash queued for next recovery boot. Please reboot to recovery.";
}
