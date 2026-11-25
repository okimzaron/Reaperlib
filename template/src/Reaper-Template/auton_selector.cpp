/* This Source Code Form is subject to the Mozilla Public License, v. 2.0.
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// [Reaper] Touch dashboard. See auton_selector.hpp.

#include "Reaper-Template/api.hpp"
#include "Reaper-Template/roller.hpp"
#include "Reaper-Template/vmp.hpp"
#include "team_theme.hpp"
#include <algorithm>
#include <utility>
#include <cstdio>
#include <cmath>
#include "pros/rtos.hpp"
#include "liblvgl/llemu.hpp"
#include "robot_hardware.hpp"

namespace {

ez::AutonSelector* g_selector_refresh_owner = nullptr;

void no_scroll(lv_obj_t* obj);

lv_obj_t* make_button(lv_obj_t* parent) {
#if LVGL_VERSION_MAJOR >= 9
  auto* button = lv_button_create(parent);
#else
  auto* button = lv_btn_create(parent);
#endif
  no_scroll(button);
  // Two pixels per side fits inside the tightest five-pixel route gap.
  lv_obj_set_ext_click_area(button, 2);
#if LVGL_VERSION_MAJOR >= 9
  lv_obj_remove_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
#else
  lv_obj_clear_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
#endif
  return button;
}

void load_screen(lv_obj_t* screen) {
#if LVGL_VERSION_MAJOR >= 9
  lv_screen_load(screen);
#else
  lv_scr_load(screen);
#endif
}

void no_scroll(lv_obj_t* obj) {
#if LVGL_VERSION_MAJOR >= 9
  lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLL_ON_FOCUS));
#else
  lv_obj_clear_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLL_ON_FOCUS));
#endif
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

// Reaper inverted theme: purple pages, peach text and accents.
// Semantic names keep surface/foreground roles clear when adjusting colours.
namespace theme {
// Values come from src/team/team_theme.hpp -- change colors there, not here.
constexpr uint32_t SURFACE = team_theme::PAGE;
constexpr uint32_t CARD = team_theme::CARD;
constexpr uint32_t SURFACE_PRESSED = team_theme::CARD_PRESSED;
constexpr uint32_t ACCENT = team_theme::TEXT;
constexpr uint32_t NAV_IDLE = team_theme::NAV_BUTTON;
constexpr uint32_t ACCENT_PRESSED = team_theme::TEXT_PRESSED;
constexpr uint32_t BORDER = team_theme::BORDER;
constexpr uint32_t WARN = team_theme::STATUS_CAUTION;
constexpr uint32_t HOT = team_theme::STATUS_HOT;
constexpr uint32_t OK = team_theme::STATUS_OK;
constexpr uint32_t BAD = team_theme::STATUS_BAD;
constexpr uint32_t WAITING = team_theme::STATUS_WAITING;
}  // namespace theme

// Buttons on purple pages: peach when selected, dark purple when idle.
void style_button(lv_obj_t* obj, bool selected) {
  lv_obj_set_style_bg_color(obj,
                             lv_color_hex(selected ? theme::ACCENT : theme::CARD), 0);
  lv_obj_set_style_bg_color(obj,
                             lv_color_hex(selected ? theme::ACCENT_PRESSED : theme::SURFACE_PRESSED),
                             LV_STATE_PRESSED);
  lv_obj_set_style_text_color(obj,
                               lv_color_hex(selected ? theme::SURFACE : theme::ACCENT), 0);
  lv_obj_set_style_border_width(obj, selected ? 2 : 1, 0);
  lv_obj_set_style_border_color(obj,
                                 lv_color_hex(selected ? theme::NAV_IDLE : theme::BORDER), 0);
  lv_obj_set_style_radius(obj, 5, 0);
}

// Buttons on the peach nav rail: purple when selected, peach when idle.
void style_nav_button(lv_obj_t* obj, bool selected) {
  lv_obj_set_style_bg_color(obj,
                             lv_color_hex(selected ? theme::SURFACE : theme::NAV_IDLE), 0);
  lv_obj_set_style_bg_color(obj,
                             lv_color_hex(selected ? theme::SURFACE_PRESSED : theme::ACCENT_PRESSED),
                             LV_STATE_PRESSED);
  lv_obj_set_style_text_color(obj,
                               lv_color_hex(selected ? theme::ACCENT : theme::SURFACE), 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_border_color(obj,
                                 lv_color_hex(selected ? theme::SURFACE : theme::BORDER), 0);
  lv_obj_set_style_radius(obj, 5, 0);
}

void visible(lv_obj_t* obj, bool show) {
  if (show) {
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
#else
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
#endif
  } else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h) {
  auto* obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  no_scroll(obj);
  return obj;
}

int clamp_index(int value, int count) {
  if (count <= 0) return 0;
  if (value < 0) return 0;
  if (value >= count) return count - 1;
  return value;
}

}  // namespace

ez::AutonSelector::AutonSelector(std::vector<Auton> autons) {
  autons_add(std::move(autons));
}

void ez::AutonSelector::autons_add(std::vector<Auton> autons) {
  if (autons.size() > 15) autons.resize(15);
  Autons = std::move(autons);
  match_count_ = static_cast<int>(Autons.size());
  auton_count = match_count_;
  auton_page_current = last_auton_page_current = 0;
  match_selected_ = skills_selected_ = 0;
  selected_index_.store(0);
  ui_dirty_.store(true);
}

void ez::AutonSelector::skills_add(std::vector<Auton> skills) {
  if (skills.size() > 3) skills.resize(3);
  Autons.resize(match_count_);
  Autons.insert(Autons.end(), skills.begin(), skills.end());
  auton_count = static_cast<int>(Autons.size());
  ui_dirty_.store(true);
}

void ez::AutonSelector::selected_auton_print() {
  auton_count = static_cast<int>(Autons.size());
  auton_page_current = clamp_index(auton_page_current, auton_count);
  last_auton_page_current = auton_page_current;
  selected_index_.store(auton_page_current);
  // Legacy LLEMU text only; the touch UI shows the selection itself. Never touch LLEMU unless something
  // (the PID tuner) already started it, or it would flash over the boot screen.
  if (!ui_ready_.load() && !Autons.empty() && pros::lcd::is_initialized()) {
    ez::screen_lines_clear();
    ez::screen_print(Autons[auton_page_current].Name);
  }
  ui_dirty_.store(true);
}

void ez::AutonSelector::selected_auton_call() {
  selection_lock(true);
  const int index = selected_index_.load();
  if (index >= 0 && index < static_cast<int>(Autons.size()) && Autons[index].auton_call)
    Autons[index].auton_call();
  selection_lock(false);
}

void ez::AutonSelector::selection_lock(bool locked) {
  running_.store(locked);
  ui_dirty_.store(true);
}

bool ez::AutonSelector::selection_allowed() const {
  return !running_.load() &&
      !(pros::competition::is_connected() && !pros::competition::is_disabled());
}

void ez::AutonSelector::select_index(int index) {
  if (!selection_allowed() || Autons.empty()) return;
  index = clamp_index(index, static_cast<int>(Autons.size()));
  auton_page_current = last_auton_page_current = index;
  selected_index_.store(index);
  if (index < match_count_) match_selected_ = index;
  else skills_selected_ = index - match_count_;
  ez::as::auto_sd_update();
  ui_dirty_.store(true);
}

bool ez::AutonSelector::ui_enabled() const { return ui_ready_.load(); }
void ez::AutonSelector::ui_initialize() {
  if (ui_ready_.load() || ui_requested_.exchange(true)) return;
  lv_async_call(&AutonSelector::build_ui_async, this);
  for (int i = 0; i < 100 && !ui_ready_.load(); ++i) pros::delay(5);
}
void ez::AutonSelector::ui_show() {
  if (ui_ready_.load()) lv_async_call(&AutonSelector::show_ui_async, this);
}
void ez::AutonSelector::build_ui_async(void* context) {
  auto* self = static_cast<AutonSelector*>(context);
  if (!self) return;
  self->build_ui();
  self->ui_ready_.store(true);
}
void ez::AutonSelector::show_ui_async(void* context) {
  auto* self = static_cast<AutonSelector*>(context);
  if (self && self->screen_) load_screen(self->screen_);
}

void ez::AutonSelector::build_ui() {
#if LVGL_VERSION_MAJOR >= 9
  const int full_width = lv_display_get_horizontal_resolution(nullptr);
  const int height = lv_display_get_vertical_resolution(nullptr);
#else
  const int full_width = lv_disp_get_hor_res(nullptr);
  const int height = lv_disp_get_ver_res(nullptr);
#endif
  // Standard V5 application canvas: 480 x 240 (system bar excluded).
  screen_ = lv_obj_create(nullptr);
  lv_obj_set_size(screen_, full_width, height);
  lv_obj_set_style_pad_all(screen_, 0, 0);
  lv_obj_set_style_border_width(screen_, 0, 0);
  lv_obj_set_style_bg_color(screen_, lv_color_hex(theme::ACCENT), 0);  // nav rail
  lv_obj_set_style_text_color(screen_, lv_color_hex(theme::ACCENT), 0);
  no_scroll(screen_);

  build_dashboard(full_width, height);
  const int width = full_width - 56;
  auto* title = lv_label_create(routes_panel_);
  lv_label_set_text(title, "REAPER | ROUTES");
  lv_obj_set_pos(title, 8, 12);

  color_button_ = make_button(routes_panel_);
  lv_obj_set_pos(color_button_, width - 94, 4);
  lv_obj_set_size(color_button_, 88, 34);
  style_button(color_button_, false);
  lv_obj_add_event_cb(color_button_, touch_event_cb, LV_EVENT_ALL, this);
  lv_obj_add_event_cb(color_button_, color_button_cb, LV_EVENT_CLICKED, this);
  color_label_ = lv_label_create(color_button_);
  lv_obj_center(color_label_);

  grid_width_ = width - 12;
  grid_height_ = height - 104;
  list_panel_ = lv_obj_create(routes_panel_);
  lv_obj_set_pos(list_panel_, 6, 42);
  lv_obj_set_size(list_panel_, grid_width_, grid_height_);
  lv_obj_set_style_pad_all(list_panel_, 0, 0);
  lv_obj_set_style_border_width(list_panel_, 0, 0);
  lv_obj_set_style_bg_opa(list_panel_, LV_OPA_TRANSP, 0);
  no_scroll(list_panel_);

  status_label_ = lv_label_create(routes_panel_);
  lv_obj_set_pos(status_label_, 8, height - 62);
  lv_obj_set_size(status_label_, width - 16, 20);
  lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);

  const int tab_w = (width - 18) / 2;
  autons_tab_ = make_button(routes_panel_);
  skills_tab_ = make_button(routes_panel_);
  for (auto* tab : {autons_tab_, skills_tab_}) {
    lv_obj_set_size(tab, tab_w, 34);
    lv_obj_add_event_cb(tab, touch_event_cb, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(tab, tab_button_cb, LV_EVENT_CLICKED, this);
  }
  lv_obj_set_pos(autons_tab_, 6, height - 38);
  lv_obj_set_pos(skills_tab_, 12 + tab_w, height - 38);
  auto* match_label = lv_label_create(autons_tab_);
  lv_label_set_text(match_label, "Main Match");
  lv_obj_center(match_label);
  auto* skills_label = lv_label_create(skills_tab_);
  lv_label_set_text(skills_label, "Skills Match");
  lv_obj_center(skills_label);

  skills_panel_ = selected_index_.load() >= match_count_ && auton_count > match_count_;
  if (skills_panel_) skills_selected_ = selected_index_.load() - match_count_;
  else match_selected_ = selected_index_.load();
  rebuild_entry_list();
  g_selector_refresh_owner = this;
  refresh_timer_ = lv_timer_create(refresh_timer_cb, 100, this);
  refresh_ui();
  set_view(View::Home);
  load_screen(screen_);
}

void ez::AutonSelector::rebuild_entry_list() {
  if (!list_panel_) return;
  lv_obj_clean(list_panel_);
  entry_buttons_.clear();
  const int start = skills_panel_ ? match_count_ : 0;
  const int count = skills_panel_ ? auton_count - match_count_ : match_count_;
  const int columns = skills_panel_ ? 3 : 5;
  const int rows = skills_panel_ ? 1 : 3;
  const int gap = 5;
  const int cell_w = (grid_width_ - (columns - 1) * gap) / columns;
  const int cell_h = (grid_height_ - 2 * gap) / 3;
  for (int i = 0; i < count; ++i) {
    auto* button = make_button(list_panel_);
    lv_obj_set_pos(button, (i % columns) * (cell_w + gap),
                   rows == 1 ? (grid_height_ - cell_h) / 2 : (i / columns) * (cell_h + gap));
    lv_obj_set_size(button, cell_w, cell_h);
    lv_obj_set_style_pad_all(button, 3, 0);
    no_scroll(button);
    lv_obj_add_event_cb(button, touch_event_cb, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(button, entry_button_cb, LV_EVENT_CLICKED, this);
    auto* label = lv_label_create(button);
    lv_obj_set_width(label, cell_w - 10);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, Autons[start + i].Name.c_str());
    lv_obj_center(label);
    entry_buttons_.push_back(button);
  }
}

void ez::AutonSelector::refresh_ui() {
  if (!screen_) return;
  const int selected = selected_index_.load();
  const bool skills = selected >= match_count_ && auton_count > match_count_;
  if (skills != skills_panel_) {
    skills_panel_ = skills;
    rebuild_entry_list();
  }
  const int start = skills_panel_ ? match_count_ : 0;
  for (std::size_t i = 0; i < entry_buttons_.size(); ++i)
    style_button(entry_buttons_[i], start + static_cast<int>(i) == selected);
  style_button(autons_tab_, !skills_panel_);
  style_button(skills_tab_, skills_panel_);
  const bool red = roller::target_get() == roller::Color::Red;
  lv_label_set_text(color_label_, red ? "RED" : "BLUE");
  lv_obj_center(color_label_);
  // Alliance colour stays red/blue, but with cream text so it reads on the theme.
  lv_obj_set_style_bg_color(color_button_, lv_color_hex(red ? team_theme::ALLIANCE_RED : team_theme::ALLIANCE_BLUE), 0);
  lv_obj_set_style_bg_color(color_button_, lv_color_hex(red ? team_theme::ALLIANCE_RED_PRESSED : team_theme::ALLIANCE_BLUE_PRESSED),
                            LV_STATE_PRESSED);
  lv_obj_set_style_text_color(color_button_, lv_color_hex(theme::ACCENT), 0);
  lv_obj_set_style_border_width(color_button_, 2, 0);
  lv_obj_set_style_border_color(color_button_, lv_color_hex(theme::NAV_IDLE), 0);
  std::string text = "No route registered";
  if (selected >= 0 && selected < auton_count) {
    text = running_.load() ? "Running: " : "Selected: ";
    text += (skills_panel_ ? "Skills / " : "Match / ") + Autons[selected].Name;
  }
  lv_label_set_text(status_label_, text.c_str());
  lv_label_set_text(home_status_, text.c_str());
}

void ez::AutonSelector::set_panel(bool skills) {
  if (!selection_allowed()) return;
  const int count = skills ? auton_count - match_count_ : match_count_;
  if (count <= 0) return;
  select_index((skills ? match_count_ : 0) +
               clamp_index(skills ? skills_selected_ : match_selected_, count));
  refresh_ui();
}
void ez::AutonSelector::refresh_timer_cb(lv_timer_t*) {
  auto* self = g_selector_refresh_owner;
  if (!self) return;
  // Keep widgets stable until the current touch is released or cancelled.
  if (self->touch_.pressed()) return;
  if (self->ui_dirty_.exchange(false)) self->refresh_ui();
  if (++self->motor_tick_ >= 5) {
    self->motor_tick_ = 0;
    if (self->view_ == View::Motors) self->refresh_motors();
  }
  if (self->view_ == View::Motors) self->refresh_roller();  // 10 Hz so colour changes show promptly
  if (self->view_ == View::Sensors && self->motor_tick_ % 2 == 0) self->refresh_sensors();  // 5 Hz
}
void ez::AutonSelector::entry_button_cb(lv_event_t* event) {
  auto* self = static_cast<AutonSelector*>(lv_event_get_user_data(event));
  if (!self || !self->accept_touch(event)) return;
  auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  for (std::size_t i = 0; i < self->entry_buttons_.size(); ++i) {
    if (target == self->entry_buttons_[i]) {
      self->select_index((self->skills_panel_ ? self->match_count_ : 0) + static_cast<int>(i));
      return;
    }
  }
}
void ez::AutonSelector::tab_button_cb(lv_event_t* event) {
  auto* self = static_cast<AutonSelector*>(lv_event_get_user_data(event));
  if (self && self->accept_touch(event)) self->set_panel(lv_event_get_target(event) == self->skills_tab_);
}
void ez::AutonSelector::color_button_cb(lv_event_t* event) {
  auto* self = static_cast<AutonSelector*>(lv_event_get_user_data(event));
  if (!self || !self->accept_touch(event) || !self->selection_allowed()) return;
  roller::target_set(roller::target_get() == roller::Color::Red ? roller::Color::Blue : roller::Color::Red);
  self->ui_dirty_.store(true);
}

void ez::AutonSelector::dashboard_configure(const void* logo,
    std::function<std::vector<MotorStatus>()> provider) {
  if (ui_requested_.load()) return;
  logo_ = logo;
  motor_provider_ = std::move(provider);
}

void ez::AutonSelector::build_dashboard(int width, int height) {
  const int content_width = width - 56;
  const char* names[] = {LV_SYMBOL_HOME, "A", "M", "S"};
  for (int i = 0; i < 4; ++i) {
    nav_[i] = make_button(screen_);
    lv_obj_set_pos(nav_[i], 4, 6 + i * 58);
    lv_obj_set_size(nav_[i], 46, 52);
    lv_obj_set_style_pad_all(nav_[i], 0, 0);
    auto* label = lv_label_create(nav_[i]);
    lv_label_set_text(label, names[i]);
    lv_obj_center(label);
    lv_obj_add_event_cb(nav_[i], touch_event_cb, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(nav_[i], nav_button_cb, LV_EVENT_CLICKED, this);
  }
  home_panel_ = panel(screen_, 56, 0, content_width, height);
  routes_panel_ = panel(screen_, 56, 0, content_width, height);
  motors_panel_ = panel(screen_, 56, 0, content_width, height);
  sensors_panel_ = panel(screen_, 56, 0, content_width, height);
  for (auto* page : {home_panel_, routes_panel_, motors_panel_, sensors_panel_}) {
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(page, lv_color_hex(theme::SURFACE), 0);
  }
  if (logo_) {
#if LVGL_VERSION_MAJOR >= 9
    auto* image = lv_image_create(home_panel_);
    lv_image_set_src(image, logo_);
#else
    auto* image = lv_img_create(home_panel_);
    lv_img_set_src(image, logo_);
#endif
    lv_obj_align(image, LV_ALIGN_TOP_MID, 0, 4);
  } else {
    auto* label = lv_label_create(home_panel_);
    lv_label_set_text(label, team_theme::TEAM_NAME);
    lv_obj_set_style_text_color(label, lv_color_hex(theme::ACCENT), 0);
    lv_obj_center(label);
  }
  home_status_ = lv_label_create(home_panel_);
  lv_obj_set_pos(home_status_, 10, height - 28);
  lv_obj_set_size(home_status_, content_width - 20, 20);
  lv_obj_set_style_text_color(home_status_, lv_color_hex(theme::ACCENT), 0);
  lv_obj_set_style_text_align(home_status_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(home_status_, LV_LABEL_LONG_DOT);

  auto* title = lv_label_create(motors_panel_);
  lv_label_set_text(title, "MOTORS | CONNECTION + TEMP");
  lv_obj_set_pos(title, 8, 9);
  auto* legend = lv_label_create(motors_panel_);
  lv_label_set_text(legend, "Green: motor detected   Red: check port");
  lv_obj_set_pos(legend, 8, 31);
  lv_obj_set_width(legend, content_width - 16);
  lv_label_set_long_mode(legend, LV_LABEL_LONG_DOT);
  roller_label_ = legend;  // replaced by the live optical readout when a roller sensor is configured
  const int cell_w = (content_width - 28) / 3;
  const int cell_h = (height - 100) / 2;
  for (int i = 0; i < 6; ++i) {
    auto* box = panel(motors_panel_, 8 + (i % 3) * (cell_w + 6),
                      55 + (i / 3) * (cell_h + 5), cell_w, cell_h);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(theme::CARD), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 5, 0);
    auto* icon = panel(box, 8, 34, 15, 19);
    lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(icon, 3, 0);
    auto* shaft = panel(icon, 13, 7, 4, 5);
    lv_obj_set_style_bg_opa(shaft, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(shaft, lv_color_hex(theme::SURFACE), 0);
    auto* name = lv_label_create(box);
    lv_obj_set_pos(name, 6, 5);
    lv_obj_set_width(name, cell_w - 12);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    auto* status = lv_label_create(box);
    lv_obj_set_pos(status, 28, 27);
    lv_obj_set_width(status, cell_w - 32);
    lv_label_set_long_mode(status, LV_LABEL_LONG_DOT);
    auto* temp = lv_label_create(box);
    lv_obj_set_pos(temp, 28, 47);
    lv_obj_set_width(temp, cell_w - 32);
    motor_cards_.push_back({box, icon, name, status, temp});
  }
  motor_prev_ = make_button(motors_panel_);
  motor_next_ = make_button(motors_panel_);
  for (auto* button : {motor_prev_, motor_next_}) {
    lv_obj_set_size(button, 55, 36);
    style_button(button, false);
    lv_obj_add_event_cb(button, touch_event_cb, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(button, motor_page_cb, LV_EVENT_CLICKED, this);
  }
  lv_obj_set_pos(motor_prev_, 8, height - 38);
  lv_obj_set_pos(motor_next_, content_width - 63, height - 38);
  auto* previous = lv_label_create(motor_prev_);
  lv_label_set_text(previous, LV_SYMBOL_LEFT);
  lv_obj_center(previous);
  auto* next = lv_label_create(motor_next_);
  lv_label_set_text(next, LV_SYMBOL_RIGHT);
  lv_obj_center(next);
  motor_page_label_ = lv_label_create(motors_panel_);
  lv_obj_set_pos(motor_page_label_, 68, height - 26);
  lv_obj_set_width(motor_page_label_, content_width - 136);
  lv_obj_set_style_text_align(motor_page_label_, LV_TEXT_ALIGN_CENTER, 0);

  build_sensors_page(content_width, height);
}

// ---- Sensors page (VMP) -------------------------------------------------------------------------------
void ez::AutonSelector::build_sensors_page(int content_width, int height) {
  auto* title = lv_label_create(sensors_panel_);
  lv_label_set_text(title, "SENSORS | IMU MOTION");
  lv_obj_set_pos(title, 8, 9);

  // Like the Motors page, only IMUs that have a port in robot_hardware.hpp get a card, and the cards
  // share the width: one IMU = one wide card, two or three = side by side.
  const auto ports = RobotHardware::vmp_ports();
  sensor_count_ = 0;
  for (int slot = 0; slot < 3; ++slot)
    if (ports[slot] != 0) sensor_slots_[sensor_count_++] = slot;

  if (sensor_count_ == 0) {
    auto* none = lv_label_create(sensors_panel_);
    lv_obj_set_pos(none, 8, 40);
    lv_obj_set_width(none, content_width - 16);
    lv_label_set_long_mode(none, LV_LABEL_LONG_WRAP);
    lv_label_set_text(none, "No IMUs set up.\nAdd IMU_1_PORT (and optionally IMU_2_PORT, LIFT_IMU_PORT)\nin robot_hardware.hpp.");
  }

  const int gap = 6;
  const int cell_w = sensor_count_ ? (content_width - 16 - gap * (sensor_count_ - 1)) / sensor_count_ : 0;
  const int cell_h = height - 100;
  for (int i = 0; i < sensor_count_; ++i) {
    auto* box = panel(sensors_panel_, 8 + i * (cell_w + gap), 32, cell_w, cell_h);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(theme::CARD), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(theme::BORDER), 0);
    lv_obj_set_style_radius(box, 5, 0);
    auto* name = lv_label_create(box);
    lv_obj_set_pos(name, 6, 4);
    lv_obj_set_width(name, cell_w - 12);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    auto* body = lv_label_create(box);
    lv_obj_set_pos(body, 6, 24);
    lv_obj_set_width(body, cell_w - 10);
    lv_obj_set_style_text_line_space(body, -1, 0);
    sensor_cards_[i] = {box, name, body};
  }

  sensors_warning_ = lv_label_create(sensors_panel_);
  lv_obj_set_pos(sensors_warning_, 8, height - 58);
  lv_obj_set_width(sensors_warning_, content_width - 16);
  lv_label_set_long_mode(sensors_warning_, LV_LABEL_LONG_DOT);

  tilt_zero_button_ = make_button(sensors_panel_);
  lv_obj_set_size(tilt_zero_button_, 110, 32);
  lv_obj_set_pos(tilt_zero_button_, content_width - 118, height - 36);
  style_button(tilt_zero_button_, false);
  lv_obj_add_event_cb(tilt_zero_button_, touch_event_cb, LV_EVENT_ALL, this);
  lv_obj_add_event_cb(tilt_zero_button_, tilt_zero_cb, LV_EVENT_CLICKED, this);
  auto* label = lv_label_create(tilt_zero_button_);
  lv_label_set_text(label, "Zero tilt");
  lv_obj_center(label);
}

void ez::AutonSelector::refresh_sensors() {
  if (!sensors_panel_) return;
  static const char* const slot_names[] = {"IMU 1", "IMU 2", "Lift IMU"};
  const auto ports = RobotHardware::vmp_ports();
  const auto rows = vmp::snapshot();
  char text[160];
  for (int i = 0; i < sensor_count_; ++i) {
    const int slot = sensor_slots_[i];
    const auto& card = sensor_cards_[i];
    const auto& r = rows[slot];
    std::snprintf(text, sizeof(text), "%s P%d", slot_names[slot], ports[slot]);
    lv_label_set_text(card.title, text);

    uint32_t color = theme::WAITING;  // warming
    if (r.state == vmp::State::quiet) color = theme::OK;
    else if (r.state == vmp::State::active) color = theme::WARN;
    else if (r.state == vmp::State::invalid || r.state == vmp::State::stale) color = theme::BAD;
    lv_obj_set_style_border_color(card.box, lv_color_hex(color), 0);
    if (r.state == vmp::State::invalid) {  // unplugged, wrong device, or still calibrating
      lv_label_set_text(card.body, "no reading\ncheck the port\n(normal while\ncalibrating)");
      continue;
    }

    char tilt[24];
    if (r.tilt_reference && std::isfinite(r.tilt_deg)) std::snprintf(tilt, sizeof(tilt), "%.1f deg", r.tilt_deg);
    else std::snprintf(tilt, sizeof(tilt), "--");
    std::snprintf(text, sizeof(text), "%s\nshake %.3f g\nrot %.1f dps\npeak %.2f g\nhits %lu\ntilt %s",
                  vmp::state_name(r.state), r.accel_rms, r.gyro_rms, r.accel_peak,
                  static_cast<unsigned long>(r.impacts), tilt);
    lv_label_set_text(card.body, text);
  }

  const auto w = vmp::warning();
  if (w.alert == vmp::Alert::tipping)
    std::snprintf(text, sizeof(text), "WARNING: TIPPING  %.0f deg", w.tilt_deg);
  else if (w.alert == vmp::Alert::sway)
    std::snprintf(text, sizeof(text), "WARNING: LIFT SWAY  %.0f dps", w.lift_sway_dps);
  else
    std::snprintf(text, sizeof(text), "No warnings");
  lv_label_set_text(sensors_warning_, text);
  lv_obj_set_style_text_color(sensors_warning_,
                              lv_color_hex(w.alert == vmp::Alert::none ? theme::ACCENT : theme::HOT), 0);
}

void ez::AutonSelector::tilt_zero_cb(lv_event_t* event) {
  auto* self = static_cast<AutonSelector*>(lv_event_get_user_data(event));
  if (!self || !self->accept_touch(event)) return;
  vmp::tilt_reference_reset();  // the next still moment becomes "flat"
}

// Motors page legend line: shows what the roller's optical sensor sees, so colour thresholds can be checked on the real robot.
void ez::AutonSelector::refresh_roller() {
  if (!roller_label_) return;
  if (RobotHardware::ROLLER_OPTICAL_PORT == 0) {
    lv_label_set_text(roller_label_, "Green: motor detected   Red: check port");
    return;
  }
  const auto s = roller::status();
  char text[96];
  if (!s.sensor_ok)
    std::snprintf(text, sizeof(text), "Optical P%d: no sensor", RobotHardware::ROLLER_OPTICAL_PORT);
  else
    std::snprintf(text, sizeof(text), "Optical: %s  h%.0f s%.2f p%d  [%s]", roller::color_name(s.seen), s.hue,
                  s.saturation, s.proximity, roller::mode_name(s.mode));
  lv_label_set_text(roller_label_, text);
}

void ez::AutonSelector::set_view(View view) {
  view_ = view;
  visible(home_panel_, view == View::Home);
  visible(routes_panel_, view == View::Routes);
  visible(motors_panel_, view == View::Motors);
  visible(sensors_panel_, view == View::Sensors);
  for (int i = 0; i < 4; ++i) style_nav_button(nav_[i], i == static_cast<int>(view));
  if (view == View::Sensors) refresh_sensors();
  if (view == View::Motors) refresh_motors();
  if (view == View::Motors) refresh_roller();
}

void ez::AutonSelector::refresh_motors() {
  const auto motors = motor_provider_ ? motor_provider_() : std::vector<MotorStatus>{};
  motor_pages_ = std::max(1, (static_cast<int>(motors.size()) + 5) / 6);
  motor_page_ = clamp_index(motor_page_, motor_pages_);
  for (int i = 0; i < 6; ++i) {
    const int index = motor_page_ * 6 + i;
    auto& card = motor_cards_[i];
    visible(card.box, index < static_cast<int>(motors.size()));
    if (index >= static_cast<int>(motors.size())) continue;
    const auto& motor = motors[index];
    const char* state = "WAIT";
    uint32_t color = theme::WAITING;
    switch (motor.state) {
      case MotorConnection::Connected: state = "OK"; color = theme::OK; break;
      case MotorConnection::Disconnected: state = "OFF"; color = theme::BAD; break;
      case MotorConnection::WrongDevice: state = "TYPE"; color = theme::BAD; break;
      case MotorConnection::ReadError: state = "READ"; color = theme::WARN; break;
      case MotorConnection::InvalidPort: state = "PORT"; color = theme::BAD; break;
      case MotorConnection::Conflict: state = "USED"; color = theme::BAD; break;
      case MotorConnection::Stale: state = "STALE"; break;
      default: break;
    }
    lv_label_set_text(card.name, motor.name.c_str());
    char text[48];
    std::snprintf(text, sizeof(text), "P%d %s", motor.port, state);
    lv_label_set_text(card.status, text);
    lv_obj_set_style_bg_color(card.icon, lv_color_hex(color), 0);
    lv_obj_set_style_border_color(card.box, lv_color_hex(color), 0);
    if (motor.state == MotorConnection::Connected) {
      std::snprintf(text, sizeof(text), "%.0f C%s", motor.temperature,
                    motor.temperature >= 55 ? " HOT" : motor.temperature >= 50 ? " WARM" : "");
    } else std::snprintf(text, sizeof(text), "-- C");
    lv_label_set_text(card.temperature, text);
    lv_obj_set_style_text_color(card.temperature, lv_color_hex(
        motor.state == MotorConnection::Connected && motor.temperature >= 55 ? theme::HOT :
        motor.state == MotorConnection::Connected && motor.temperature >= 50 ? theme::WARN : theme::ACCENT), 0);
  }
  char text[64];
  if (motors.empty()) std::snprintf(text, sizeof(text), "No configured motors");
  else std::snprintf(text, sizeof(text), "%d motors | %d / %d", static_cast<int>(motors.size()), motor_page_ + 1, motor_pages_);
  lv_label_set_text(motor_page_label_, text);
  visible(motor_prev_, motor_pages_ > 1);
  visible(motor_next_, motor_pages_ > 1);
}
void ez::AutonSelector::nav_button_cb(lv_event_t* event) {
  auto* self = static_cast<AutonSelector*>(lv_event_get_user_data(event));
  if (!self || !self->accept_touch(event)) return;
  for (int i = 0; i < 4; ++i)
    if (lv_event_get_target(event) == self->nav_[i]) self->set_view(static_cast<View>(i));
}
void ez::AutonSelector::motor_page_cb(lv_event_t* event) {
  auto* self = static_cast<AutonSelector*>(lv_event_get_user_data(event));
  if (!self || !self->accept_touch(event)) return;
  const int step = lv_event_get_target(event) == self->motor_next_ ? 1 : -1;
  self->motor_page_ = (self->motor_page_ + step + self->motor_pages_) % self->motor_pages_;
  self->refresh_motors();
}

// All state below is owned by the LVGL task, just like its widget callbacks.
void ez::AutonSelector::touch_event_cb(lv_event_t* event) {
  auto* self = static_cast<AutonSelector*>(lv_event_get_user_data(event));
  if (!self) return;
  auto* target = lv_event_get_target(event);
  switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED: self->touch_.press(target, lv_tick_get()); break;
    case LV_EVENT_RELEASED: self->touch_.release(target); break;
    case LV_EVENT_PRESS_LOST: self->touch_.cancel(target); break;
    default: break;
  }
}

bool ez::AutonSelector::accept_touch(lv_event_t* event) {
  return touch_.click(lv_event_get_target(event), lv_tick_get());
}
