/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "ui.h"

// Headless recovery UI class - to support devices with no display
class HeadlessRecoveryUI : public RecoveryUI {
 public:
  HeadlessRecoveryUI() = default;

  void SetLocale(const char* locale) override {}
  void SetBackground(Icon icon) override {}
  void SetSystemUpdateText(bool security_update) override {}
  void SetProgressType(ProgressType type) override {}
  void ShowProgress(float portion, float seconds) override {}
  void SetProgress(float fraction) override {}
  void SetStage(int current, int max) override {}
  void ShowText(bool visible) override {}
  bool IsTextVisible() override {
    return false;
  }
  bool WasTextEverVisible() override {
    return false;
  }
  void Print(const char* fmt, ...) override {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
  void PrintOnScreenOnly(const char* fmt, ...) override {}
  void ShowFile(const char* filename) override {}
  void StartMenu(const char* const* headers, const char* const* items,
                 int initial_selection) override {}
  int SelectMenu(int sel) override {
    return sel;
  }
  void EndMenu() override {}
};
