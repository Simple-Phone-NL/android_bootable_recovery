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

#pragma once

#include <string>
#include <thread>

class RecoveryCommandServer {
 public:
  RecoveryCommandServer();
  ~RecoveryCommandServer();

  // Start the socket server in a background thread
  bool Start();

  // Stop the socket server
  void Stop();

  // Check if server is running
  bool IsRunning() const;

 private:
  int server_socket_ = -1;
  std::thread server_thread_;
  bool is_running_ = false;
  bool should_stop_ = false;

  // Main server loop
  void ServerLoop();

  // Handle a client command
  void HandleCommand(int client_fd);

  // Parse and execute command
  std::string ExecuteCommand(const std::string& command_str);

  // Command handlers
  std::string HandleWipe();
  std::string HandleFlash(const std::string& file_path);
};
