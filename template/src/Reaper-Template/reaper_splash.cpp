// [Reaper] Scythe splash animation. See reaper_splash.hpp.

#include "Reaper-Template/reaper_splash.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <vector>

#include "liblvgl/lvgl.h"
#include "pros/rtos.hpp"
#include "Reaper-Template/reaper_splash_assets.hpp"

namespace {
namespace A = reaper_splash_assets;

// ---- timing / feel (tweak freely) -------------------------------------------
constexpr uint32_t TICK_MS = 20;          // animation timer period
constexpr float SWING_AMPLITUDE_DEG = 16; // pendulum swing while loading
constexpr float SWING_PERIOD_MS = 1000;   // one full back-and-forth; a hit lands every half period (0.5 s)
constexpr float WINDUP_DEG = -24;         // scythe pulls back before the strike
constexpr uint32_t WINDUP_MS = 160;
constexpr uint32_t SLAM_MS = 90;
constexpr uint32_t FLASH_MS = 140;        // white flash on impact
constexpr uint32_t SHAKE_MS = 170;
constexpr uint32_t LOOSEN_MS = 300;       // how long a freshly cracked shard takes to loosen
constexpr uint32_t FADE_FALLBACK_MS = 250;
constexpr int SHAKE_PX = 3;
constexpr int TRAIL_COUNT = 2;            // faint afterimages behind the scythe
constexpr int TRAIL_LAG_TICKS[TRAIL_COUNT] = {3, 6};
constexpr lv_opa_t TRAIL_OPA[TRAIL_COUNT] = {70, 32};
constexpr int HISTORY = 8;

// ---- per-swing cracking -------------------------------------------------------
// Every time the scythe passes through the bottom of its swing it hits the glass
// and the cracks spread further from the blade. Each hit adds less than the one
// before, so the pane never fully breaks until the final strike.
constexpr float HIT_CAP = 0.70f;          // share of the glass that can crack before the finale
constexpr float HIT_DECAY = 0.55f;        // each swing adds this much less than the last
constexpr uint32_t HIT_CRACK_MS = 260;    // how long the crack front takes to spread per hit
constexpr uint32_t HIT_SHAKE_MS = 130;
constexpr uint32_t FINALE_CRACK_MS = 350;
constexpr uint32_t BURST_MS = 1000;
constexpr uint32_t FINALE_MS = WINDUP_MS + SLAM_MS + FINALE_CRACK_MS + BURST_MS;

enum class Phase { Idle, Swing, Windup, Slam, Crack, Burst, Fallback, Done };

struct State {
  std::atomic<bool> requested{false};
  std::atomic<bool> ready{false};
  std::atomic<bool> finished{false};
  std::atomic<uint32_t> duration_ms{3000};  // entire animation, including falling shards

  Phase phase = Phase::Idle;
  bool full_res = true;  // false -> skip shards, simple fade
  lv_obj_t* root = nullptr;
  lv_obj_t* images[1 + TRAIL_COUNT] = {};  // [0] main, [1..] trails
  lv_obj_t* shard_layer = nullptr;
  lv_obj_t* flash = nullptr;
  lv_timer_t* timer = nullptr;

  uint32_t t_start = 0, t_phase = 0;
  float angle = 0, angle_from = 0;
  float history[HISTORY] = {};
  int shake_x = 0, shake_y = 0;

  // per-swing cracking
  int hits = 0;                  // swings that have hit the glass so far
  float front = 0;               // crack front, in shard crack_ms units
  float front_from = 0, front_to = 0, front_at_impact = 0;
  uint32_t t_hit = 0;
  bool hit_active = false;
  float ring_r = 0;
  lv_opa_t ring_opa = 0;
};
State g;

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

int32_t rot10(float deg) {  // LVGL rotation: 0.1 degree units, 0..3599
  int32_t r = static_cast<int32_t>(std::lround(deg * 10.0f)) % 3600;
  return r < 0 ? r + 3600 : r;
}

void no_scroll(lv_obj_t* o) {
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t* plain_obj(lv_obj_t* parent, int w, int h) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(o, 0, 0);
  no_scroll(o);
  return o;
}

void set_scythe_angle(float deg) {
  g.angle = deg;
  for (int i = HISTORY - 1; i > 0; --i) g.history[i] = g.history[i - 1];
  g.history[0] = deg;
  lv_image_set_rotation(g.images[0], rot10(deg));
  for (int i = 0; i < TRAIL_COUNT; ++i)
    lv_image_set_rotation(g.images[1 + i], rot10(g.history[TRAIL_LAG_TICKS[i]]));
}

void show(lv_obj_t* o, bool on) {
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// ---- glass shards: drawn by hand each frame ----------------------------------
float smooth01(float u) { u = clamp01(u); return u * u * (3.0f - 2.0f * u); }

struct Xform { bool cracked; float dx, dy, ang_deg, fade, k; };

// Where one shard is right now. Crack phase: shards crack when the crack front
// (a ring spreading from the impact point) reaches them, then loosen a little.
// During the swings t_ms is the crack front, which only ever moves forward.
// Burst phase: shards let go centre-out, fall under gravity, and fade.
Xform shard_xform(const A::Shard& s, bool bursting, float t_ms) {
  float rx = s.cx - A::IMPACT_X, ry = s.cy - A::IMPACT_Y;
  const float n = std::sqrt(rx * rx + ry * ry);
  if (n > 0.001f) { rx /= n; ry /= n; }

  Xform x{};
  float k, ts = 0.0f;
  x.fade = 1.0f;
  if (!bursting) {
    x.cracked = t_ms >= s.crack_ms;
    k = x.cracked ? smooth01((t_ms - s.crack_ms) / static_cast<float>(LOOSEN_MS)) : 0.0f;
  } else {
    x.cracked = true;
    k = 1.0f;
    const float fly_ms = std::max(0.0f, t_ms - s.delay_ms);
    ts = fly_ms / 1000.0f;
    x.fade = clamp01(1.0f - (fly_ms - A::FADE_AFTER_MS) / static_cast<float>(A::FADE_MS));
  }
  x.dx = rx * A::LOOSEN_PX * k + s.vx * ts;
  x.dy = ry * A::LOOSEN_PX * k + s.vy * ts + 0.5f * A::GRAVITY * ts * ts;
  x.ang_deg = (s.spin >= 0 ? 1.5f : -1.5f) * k + s.spin * ts;
  x.k = k;
  return x;
}

void shard_draw_cb(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  if (!layer) return;
  const bool bursting = g.phase == Phase::Burst;
  const bool finale = g.phase == Phase::Crack || bursting;
  const bool swinging = g.phase == Phase::Swing || g.phase == Phase::Windup || g.phase == Phase::Slam;
  if (!finale && !(swinging && g.hits > 0)) return;  // pristine glass: nothing to draw yet
  const float t_ms = bursting ? static_cast<float>(lv_tick_elaps(g.t_phase)) * A::BURST_MS / BURST_MS : g.front;

  const int bg_r = (A::BG_HEX >> 16) & 0xFF, bg_g = (A::BG_HEX >> 8) & 0xFF, bg_b = A::BG_HEX & 0xFF;

  const lv_area_t clip = layer->_clip_area;

  for (int i = 0; i < A::shard_count; ++i) {
    const A::Shard& s = A::shards[i];
    if (!bursting) {  // while still on the pane, skip shards outside the area being redrawn
      int x1 = s.vf[0][0], x2 = x1, y1 = s.vf[0][1], y2 = y1;
      for (int k = 1; k < 3; ++k) {
        x1 = std::min<int>(x1, s.vf[k][0]); x2 = std::max<int>(x2, s.vf[k][0]);
        y1 = std::min<int>(y1, s.vf[k][1]); y2 = std::max<int>(y2, s.vf[k][1]);
      }
      constexpr int M = 14;  // loosen + shake + tilt
      if (x2 + M < clip.x1 || x1 - M > clip.x2 || y2 + M < clip.y1 || y1 - M > clip.y2) continue;
    }
    const Xform x = shard_xform(s, bursting, t_ms);
    if (x.fade <= 0) continue;
    const lv_opa_t opa = static_cast<lv_opa_t>(x.fade * 255.0f);
    const float dx = x.dx + g.shake_x, dy = x.dy + g.shake_y;
    const float ang = x.ang_deg * 3.14159265f / 180.0f;
    const float c = std::cos(ang), sn = std::sin(ang);

    // Glass tint fades in as a shard cracks, so unbroken glass is invisible.
    lv_draw_triangle_dsc_t tri;
    lv_draw_triangle_dsc_init(&tri);
    tri.bg_color = lv_color_make(
        static_cast<uint8_t>(std::clamp(bg_r + static_cast<int>(std::lround(s.tint[0] * x.k)), 0, 255)),
        static_cast<uint8_t>(std::clamp(bg_g + static_cast<int>(std::lround(s.tint[1] * x.k)), 0, 255)),
        static_cast<uint8_t>(std::clamp(bg_b + static_cast<int>(std::lround(s.tint[2] * x.k)), 0, 255)));
    tri.bg_opa = opa;
    for (int k = 0; k < 3; ++k) {
      const int16_t vx = x.cracked ? s.v[k][0] : s.vf[k][0];
      const int16_t vy = x.cracked ? s.v[k][1] : s.vf[k][1];
      const float rx = vx - s.cx, ry = vy - s.cy;
      tri.p[k].x = static_cast<lv_value_precise_t>(std::lround(s.cx + dx + rx * c - ry * sn));
      tri.p[k].y = static_cast<lv_value_precise_t>(std::lround(s.cy + dy + rx * sn + ry * c));
    }
    lv_draw_triangle(layer, &tri);

    // The logo pieces only appear for the finale; before that the live scythe is on top.
    if (finale && s.piece) {
      lv_draw_image_dsc_t img;
      lv_draw_image_dsc_init(&img);
      img.src = s.piece;
      img.rotation = rot10(x.ang_deg);
      img.pivot.x = static_cast<int32_t>(std::lround(s.cx)) - s.piece_x;
      img.pivot.y = static_cast<int32_t>(std::lround(s.cy)) - s.piece_y;
      img.recolor = lv_color_hex(A::ART_HEX);
      img.recolor_opa = LV_OPA_COVER;
      img.opa = opa;
      img.antialias = 1;
      lv_area_t area;
      area.x1 = s.piece_x + static_cast<int32_t>(std::lround(dx));
      area.y1 = s.piece_y + static_cast<int32_t>(std::lround(dy));
      area.x2 = area.x1 + s.piece->header.w - 1;
      area.y2 = area.y1 + s.piece->header.h - 1;
      lv_draw_image(layer, &img, &area);
    }
  }

  if (!bursting && g.ring_opa > 0 && g.ring_r > 2.0f) {  // shock-wave ring riding the crack front
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = lv_color_hex(A::FLASH_HEX);
    arc.width = 3;
    arc.opa = g.ring_opa;
    arc.center.x = static_cast<int32_t>(A::IMPACT_X);
    arc.center.y = static_cast<int32_t>(A::IMPACT_Y);
    arc.radius = static_cast<uint16_t>(g.ring_r);
    arc.start_angle = 0;
    arc.end_angle = 360;
    lv_draw_arc(layer, &arc);
  }
}

// ---- state machine -----------------------------------------------------------
void begin_phase(Phase p) {
  static const char* const names[] = {"idle", "swing", "windup", "slam", "crack", "burst", "fallback", "done"};
  printf("[Reaper splash] phase -> %s\n", names[static_cast<int>(p)]);
  g.phase = p;
  g.t_phase = lv_tick_get();
}

// The scythe just hit the glass on a swing: spread the cracks a bit further.
void hit_register(int n) {
  g.hits = n;
  g.front_from = g.front;
  // Crack the nearest share of shards (by crack order), so every swing visibly
  // breaks a few more instead of the later ones adding almost nothing.
  const float share = HIT_CAP * (1.0f - std::pow(HIT_DECAY, static_cast<float>(n)));
  std::vector<float> order;
  order.reserve(A::shard_count);
  for (int i = 0; i < A::shard_count; ++i) order.push_back(static_cast<float>(A::shards[i].crack_ms));
  std::sort(order.begin(), order.end());
  const int upto = std::clamp(static_cast<int>(std::ceil(share * order.size())), 1,
                              static_cast<int>(order.size()));
  g.front_to = std::max(g.front, order[upto - 1] + 1.0f);
  g.t_hit = lv_tick_get();
  g.hit_active = true;
  if (n == 1) {
    lv_obj_set_style_bg_color(g.root, lv_color_hex(A::CRACK_HEX), 0);  // shows through the gaps
    show(g.shard_layer, true);
  }
  printf("[Reaper splash] swing %d hit the glass\n", n);
}

// Advances the crack front, shake, flash and ring for the latest hit.
void hit_update() {
  if (!g.hit_active) return;
  const float dt = static_cast<float>(lv_tick_elaps(g.t_hit));
  const float u = clamp01(dt / static_cast<float>(HIT_CRACK_MS));
  const float e = 1.0f - (1.0f - u) * (1.0f - u);  // ease-out: fast crack, slow settle
  g.front = g.front_from + (g.front_to - g.front_from) * e;

  const float shake_amp = std::min(3.0f, 1.0f + 0.5f * g.hits) *
                          (1.0f - clamp01(dt / static_cast<float>(HIT_SHAKE_MS)));
  g.shake_x = static_cast<int>(std::lround(shake_amp * std::sin(dt * 0.9f)));
  g.shake_y = static_cast<int>(std::lround(shake_amp * std::cos(dt * 1.3f)));

  g.ring_r = g.front / static_cast<float>(A::CRACK_MS) * A::MAX_RADIUS;
  g.ring_opa = static_cast<lv_opa_t>(110.0f * (1.0f - u));

  if (u >= 1.0f) {
    g.hit_active = false;
    g.shake_x = g.shake_y = 0;
    g.ring_opa = 0;
  }
  lv_obj_invalidate(g.shard_layer);
}

void impact() {
  // Swap the live scythe for the pre-cut shards (same pose, same pixels). Any
  // cracks from earlier swings are already on the glass; the crack front
  // carries on from where it stopped.
  for (auto* img : g.images) show(img, false);
  lv_obj_set_style_bg_color(g.root, lv_color_hex(A::CRACK_HEX), 0);  // shows through the gaps
  lv_obj_set_style_bg_opa(g.root, LV_OPA_COVER, 0);
  g.hit_active = false;
  g.shake_x = g.shake_y = 0;
  g.ring_opa = 0;
  g.front_at_impact = g.front;
  show(g.shard_layer, true);
  lv_obj_set_style_bg_opa(g.flash, 170, 0);
  show(g.flash, true);
  lv_obj_invalidate(g.shard_layer);
  begin_phase(Phase::Crack);
}

void cleanup() {
  if (g.timer) { lv_timer_delete(g.timer); g.timer = nullptr; }
  if (g.root) { lv_obj_delete(g.root); g.root = nullptr; }
  g.phase = Phase::Done;
  g.finished.store(true);
}

void tick_cb(lv_timer_t*) {
  if (!g.root) return;
  const uint32_t elapsed = lv_tick_elaps(g.t_start);
  const uint32_t duration = g.duration_ms.load();
  // Absolute deadline: late/missing finish() and dropped frames cannot leave
  // an overlay covering the GUI. No extra finale is appended after init.
  if (elapsed >= duration) { cleanup(); return; }
  const uint32_t windup_at = duration - FINALE_MS;
  const uint32_t slam_at = windup_at + WINDUP_MS;
  const uint32_t crack_at = slam_at + SLAM_MS;
  const uint32_t burst_at = crack_at + FINALE_CRACK_MS;
  // Advance through overdue phases in one callback, retaining the original
  // deadlines rather than accumulating one timer/render delay per phase.
  if (!g.full_res) {
    if (elapsed >= duration - FADE_FALLBACK_MS && g.phase == Phase::Swing) {
      begin_phase(Phase::Fallback);
      g.t_phase = g.t_start + duration - FADE_FALLBACK_MS;
    }
  } else {
    if (elapsed >= windup_at && g.phase == Phase::Swing) {
      g.angle_from = g.angle;
      begin_phase(Phase::Windup);
      g.t_phase = g.t_start + windup_at;
    }
    if (elapsed >= slam_at && g.phase == Phase::Windup) {
      begin_phase(Phase::Slam);
      g.t_phase = g.t_start + slam_at;
    }
    if (elapsed >= crack_at && g.phase == Phase::Slam) {
      impact();
      g.t_phase = g.t_start + crack_at;
    }
    if (elapsed >= burst_at && g.phase == Phase::Crack) {
      g.front = A::CRACK_MS;
      g.shake_x = g.shake_y = 0;
      g.ring_opa = 0;
      show(g.flash, false);
      lv_obj_set_style_bg_opa(g.root, LV_OPA_TRANSP, 0);
      begin_phase(Phase::Burst);
      g.t_phase = g.t_start + burst_at;
    }
  }
  const uint32_t dt = lv_tick_elaps(g.t_phase);

  if (g.phase == Phase::Swing || g.phase == Phase::Windup || g.phase == Phase::Slam) hit_update();

  switch (g.phase) {
    case Phase::Swing: {
      set_scythe_angle(SWING_AMPLITUDE_DEG *
                       std::sin(2.0f * 3.14159265f * elapsed / SWING_PERIOD_MS));
      // One hit per pass through the bottom of the swing (every half period).
      const int passes = static_cast<int>(2.0f * elapsed / SWING_PERIOD_MS);
      if (g.full_res && passes > g.hits) hit_register(passes);
      break;
    }
    case Phase::Windup: {  // ease-out pull-back
      const float u = clamp01(dt / static_cast<float>(WINDUP_MS));
      const float e = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
      set_scythe_angle(g.angle_from + (WINDUP_DEG - g.angle_from) * e);
      break;
    }
    case Phase::Slam: {  // ease-in strike down to the rest pose (0 deg)
      const float u = clamp01(dt / static_cast<float>(SLAM_MS));
      set_scythe_angle(WINDUP_DEG * (1.0f - u * u));
      break;
    }
    case Phase::Crack: {
      // Finish spreading the cracks from wherever the swings left them.
      const float cr = static_cast<float>(A::CRACK_MS);
      const float dur = static_cast<float>(FINALE_CRACK_MS);
      const float u = clamp01(dt / dur);
      g.front = g.front_at_impact + (cr - g.front_at_impact) * u;
      g.ring_r = g.front / cr * A::MAX_RADIUS;
      g.ring_opa = static_cast<lv_opa_t>(140.0f * (1.0f - u));

      // flash and shake fade out fast; the crack front keeps spreading
      const float flash = 1.0f - clamp01(dt / static_cast<float>(FLASH_MS));
      const float decay = 1.0f - clamp01(dt / static_cast<float>(SHAKE_MS));
      g.shake_x = static_cast<int>(std::lround(SHAKE_PX * decay * std::sin(dt * 0.9f)));
      g.shake_y = static_cast<int>(std::lround(SHAKE_PX * decay * std::cos(dt * 1.3f)));
      if (flash > 0) lv_obj_set_style_bg_opa(g.flash, static_cast<lv_opa_t>(170.0f * flash), 0);
      else show(g.flash, false);
      lv_obj_invalidate(g.shard_layer);
      break;
    }
    case Phase::Burst: {
      lv_obj_invalidate(g.shard_layer);
      break;
    }
    case Phase::Fallback: {  // unexpected display size: plain fade, no shards
      const float u = clamp01(dt / static_cast<float>(FADE_FALLBACK_MS));
      lv_obj_set_style_opa(g.root, static_cast<lv_opa_t>(255.0f * (1.0f - u)), 0);
      if (u >= 1.0f) cleanup();
      break;
    }
    default:
      break;
  }
}

void build_async(void*) {
  const int w = lv_display_get_horizontal_resolution(nullptr);
  const int h = lv_display_get_vertical_resolution(nullptr);
  g.full_res = (w == A::SCREEN_W && h == A::SCREEN_H);

  g.root = plain_obj(lv_layer_top(), w, h);
  lv_obj_remove_flag(g.root, LV_OBJ_FLAG_CLICKABLE);  // never block the selector underneath
  lv_obj_set_style_bg_color(g.root, lv_color_hex(A::BG_HEX), 0);
  lv_obj_set_style_bg_opa(g.root, LV_OPA_COVER, 0);

  // Glass first so the scythe (and its trails) swing in front of it.
  g.shard_layer = plain_obj(g.root, w, h);
  lv_obj_remove_flag(g.shard_layer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g.shard_layer, shard_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
  show(g.shard_layer, false);

  // Trails first so the main scythe draws on top.
  for (int i = TRAIL_COUNT; i >= 0; --i) {
    lv_obj_t* img = lv_image_create(g.root);
    lv_image_set_src(img, &A::scythe_full);
    lv_obj_set_pos(img, A::ART_X, A::ART_Y);
    lv_obj_set_size(img, A::scythe_full.header.w, A::scythe_full.header.h);
    lv_image_set_pivot(img, A::PIVOT_X, A::PIVOT_Y);
    lv_image_set_antialias(img, true);
    lv_obj_set_style_image_recolor(img, lv_color_hex(A::ART_HEX), 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_image_opa(img, i == 0 ? static_cast<lv_opa_t>(LV_OPA_COVER) : TRAIL_OPA[i - 1], 0);
    g.images[i] = img;
  }

  g.flash = plain_obj(g.root, w, h);
  lv_obj_set_style_bg_color(g.flash, lv_color_hex(A::FLASH_HEX), 0);
  lv_obj_remove_flag(g.flash, LV_OBJ_FLAG_CLICKABLE);
  show(g.flash, false);

  g.t_start = lv_tick_get();
  g.phase = Phase::Swing;
  g.t_phase = g.t_start;
  g.timer = lv_timer_create(tick_cb, TICK_MS, nullptr);
  g.ready.store(true);
}

}  // namespace

namespace reaper_splash {

void min_duration_set(uint32_t ms) {
  if (!g.requested.load()) g.duration_ms.store(std::max(ms, FINALE_MS + TICK_MS));
}

void start() {
  if (g.requested.exchange(true)) return;
  if (lv_async_call(build_async, nullptr) != LV_RESULT_OK) {
    g.finished.store(true);
    return;  // allocation failure must not hold up robot initialization
  }
  // Same pattern as the dashboard: give LVGL a moment so the first frame is up
  // before the heavy init work starts.
  for (int i = 0; i < 100 && !g.ready.load(); ++i) pros::delay(5);
}

void finish() {
  // Kept for existing callers. Completion follows the start-time deadline,
  // independently of calibration success or when this notification arrives.
}

bool active() { return g.requested.load() && !g.finished.load(); }

}  // namespace reaper_splash
