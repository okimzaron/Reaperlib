/* This Source Code Form is subject to the Mozilla Public License, v. 2.0.
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once
#include <atomic>
#include "Reaper-Template/touch_gate.hpp"
#include <vector>
#include <functional>
#include <array>
#include "Reaper-Template/motor_status.hpp"
#include "Reaper-Template/auton.hpp"
#include "liblvgl/lvgl.h"

namespace ez {
class AutonSelector {
 public:
  // Flat registry: match slots first, then skills. Retained for legacy SD/LCD APIs.
  std::vector<Auton> Autons;
  int auton_page_current = 0;
  int auton_count = 0;
  int last_auton_page_current = 0;
  AutonSelector() = default;
  explicit AutonSelector(std::vector<Auton> autons);
  void autons_add(std::vector<Auton> autons); // Up to 15 match routes; resets registry.
  void skills_add(std::vector<Auton> skills); // Up to 3 skills routes; replaces skills.
  void selected_auton_call();
  void selected_auton_print();
  void ui_initialize();
  void ui_show();
  bool ui_enabled() const;
  void selection_lock(bool locked); // Manual runner clears this when cancelled.
  void dashboard_configure(const void* logo, std::function<std::vector<MotorStatus>()> provider);
 private:
  detail::TouchGate touch_;
  static void touch_event_cb(lv_event_t* event);
  bool accept_touch(lv_event_t* event);
  enum class View { Home, Routes, Motors, Sensors };
  View view_ = View::Home;
  const void* logo_ = nullptr;
  std::function<std::vector<MotorStatus>()> motor_provider_;
  std::array<lv_obj_t*, 4> nav_{};
  lv_obj_t *home_panel_ = nullptr, *routes_panel_ = nullptr, *motors_panel_ = nullptr;
  // Sensors page (VMP): one card per IMU slot, a warning line and a "zero tilt" button.
  lv_obj_t *sensors_panel_ = nullptr, *sensors_warning_ = nullptr, *tilt_zero_button_ = nullptr;
  struct SensorCard { lv_obj_t *box, *title, *body; };
  std::array<SensorCard, 3> sensor_cards_{};
  void build_sensors_page(int width, int height);
  void refresh_sensors();
  static void tilt_zero_cb(lv_event_t* event);
  lv_obj_t *home_status_ = nullptr, *motor_page_label_ = nullptr;
  lv_obj_t* roller_label_ = nullptr;  // Motors page: live optical colour readout
  void refresh_roller();
  lv_obj_t *motor_prev_ = nullptr, *motor_next_ = nullptr;
  struct MotorCard { lv_obj_t *box, *icon, *name, *status, *temperature; };
  std::vector<MotorCard> motor_cards_;
  int motor_page_ = 0, motor_pages_ = 1, motor_tick_ = 0;
  void set_view(View view);
  void build_dashboard(int width, int height);
  void refresh_motors();
  static void nav_button_cb(lv_event_t* event);
  static void motor_page_cb(lv_event_t* event);
  int match_count_ = 0;
  int match_selected_ = 0;
  int skills_selected_ = 0;
  bool skills_panel_ = false;
  int grid_width_ = 0;
  int grid_height_ = 0;
  std::atomic<int> selected_index_{0};
  std::atomic<bool> running_{false};
  std::atomic<bool> ui_requested_{false}, ui_ready_{false}, ui_dirty_{false};
  lv_obj_t *screen_ = nullptr, *list_panel_ = nullptr, *status_label_ = nullptr;
  lv_obj_t *color_button_ = nullptr, *color_label_ = nullptr;
  lv_obj_t *autons_tab_ = nullptr, *skills_tab_ = nullptr;
  lv_timer_t* refresh_timer_ = nullptr;
  std::vector<lv_obj_t*> entry_buttons_;
  bool selection_allowed() const;
  void select_index(int index);
  void set_panel(bool skills);
  void build_ui();
  void rebuild_entry_list();
  void refresh_ui();
  static void build_ui_async(void* context);
  static void show_ui_async(void* context);
  static void refresh_timer_cb(lv_timer_t* timer);
  static void entry_button_cb(lv_event_t* event);
  static void tab_button_cb(lv_event_t* event);
  static void color_button_cb(lv_event_t* event);
};
}
