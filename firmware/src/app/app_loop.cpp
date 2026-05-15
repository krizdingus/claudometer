#include "app/app_loop.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>
#include <algorithm>
#include <lvgl.h>

#include "app/app_config.h"
#include "app/long_press.h"
#include "app/plan_names.h"
#include "app/state_machine.h"
#include "hw/display.h"
#include "hw/nvs.h"
#include "hw/touch.h"
#include "net/mdns_discover.h"
#include "net/stats_client.h"
#include "net/usb_provisioner.h"
#include "ui/chrome.h"
#include "ui/discover_screen.h"
#include "ui/lvgl_glue.h"
#include "ui/provision_screen.h"
#include "ui/screen_budgets.h"
#include "ui/screen_home.h"
#include "ui/screen_models.h"
#include "ui/screen_routines.h"
#include "ui/screen_session.h"
#include "ui/screen_settings.h"
#include "ui/theme.h"
#include "ui/tileview.h"

namespace cyd {

namespace {

State current_state = State::BOOT;
Context ctx_;
Nvs *nvs_ = nullptr;
Chrome *chrome_ = nullptr;
Tileview *tileview_ = nullptr;
ScreenHome *scr_home_ = nullptr;
ScreenSession *scr_session_ = nullptr;
ScreenModels *scr_models_ = nullptr;
ScreenRoutines *scr_routines_ = nullptr;
ScreenBudgets *scr_budgets_ = nullptr;
ScreenSettings *scr_settings_ = nullptr;
ProvisionScreen *prov_ = nullptr;
DiscoverScreen *disc_ = nullptr;
MdnsDiscover *mdns_ = nullptr;
StatsClient *stats_client_ = nullptr;
Stats last_stats_;
std::string daemon_base_url_;
std::string daemon_display_;
uint32_t next_poll_at_ = 0;
uint32_t backoff_ms_ = kBackoffStartMs;

lv_obj_t *root_ = nullptr;
lv_obj_t *pre_pairing_layer_ = nullptr;   // hosts prov + disc
lv_obj_t *main_layer_ = nullptr;          // hosts tileview + chrome

void show_layer(lv_obj_t *layer) {
  lv_obj_add_flag(pre_pairing_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(main_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(layer, LV_OBJ_FLAG_HIDDEN);
}

void render_state(State s) {
  if (s == State::PROVISION || s == State::DISCOVER) {
    show_layer(pre_pairing_layer_);
    // Pre-pairing children: 0=prov, 1=disc.
    if (s == State::PROVISION) {
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    show_layer(main_layer_);
  }
}

void apply_event(Event e) {
  State next = next_state(current_state, e, ctx_);
  if (next != current_state) {
    Serial.printf("state: %d -> %d\n", (int)current_state, (int)next);
    current_state = next;
    render_state(current_state);
  }
}

void on_screen_change(Screen s) {
  if (chrome_) chrome_->set_active_screen(s);
  next_poll_at_ = 0;
}

void update_all_screens(const Stats &s) {
  if (scr_home_) scr_home_->update(s);
  if (scr_session_) scr_session_->update(s);
  if (scr_models_) scr_models_->update(s);
  if (scr_routines_) scr_routines_->update(s);
  if (scr_budgets_) scr_budgets_->update(s);
  if (scr_settings_) {
    scr_settings_->update(s);
    // Refresh device info from current WiFi state — at boot the IP is
    // 0.0.0.0 since WiFi hasn't associated yet.
    String ip_str = WiFi.localIP().toString();
    String host_str = WiFi.getHostname();
#ifdef FIRMWARE_VERSION
    scr_settings_->set_device_info(host_str.c_str(), ip_str.c_str(), FIRMWARE_VERSION);
#else
    scr_settings_->set_device_info(host_str.c_str(), ip_str.c_str(), "?");
#endif
  }
  if (chrome_) {
    chrome_->set_health(s.stale ? 1 : 0);
    if (!s.local_time.empty()) chrome_->set_clock(s.local_time.c_str());
    chrome_->set_plan(pretty_plan_str(s.budgets.plan));
  }
}

void perform_provision_usb() {
  // Block here until a host sends a valid provisioning JSON.
  ProvisioningCreds creds;
  if (!run_usb_provisioning(creds)) return;  // shouldn't happen — it loops

  // Persist to NVS and reboot. On next boot the new state will short-circuit
  // BOOT → POLL_RENDER.
  if (nvs_) {
    nvs_->save_wifi(creds.wifi_ssid, creds.wifi_password);
    nvs_->save_server(creds.server_host, creds.server_port);
    nvs_->save_bearer_token(creds.bearer_token);
  }
  Serial.println("provisioned, restarting");
  delay(200);
  ESP.restart();
}

void perform_discover() {
  // mDNS fallback when a saved server_host stops responding.
  DaemonAddr addr;
  if (!mdns_ || !mdns_->find(addr)) {
    delay(kMdnsQueryMs);
    apply_event(Event::DAEMON_NOT_FOUND);
    return;
  }
  char hp[96];
  snprintf(hp, sizeof(hp), "%s:%u", addr.hostname.c_str(), addr.port);
  daemon_base_url_ = std::string("http://") + hp;
  daemon_display_ = addr.display;
  if (nvs_) nvs_->save_server(addr.hostname, addr.port);
  apply_event(Event::DAEMON_FOUND);
}

void perform_poll() {
  uint32_t now = millis();
  if (now < next_poll_at_) return;

  Stats fresh;
  std::string token = nvs_ ? nvs_->bearer_token() : "";
  uint8_t mask = tileview_ ? tileview_->neighbor_mask() : 0;
  bool ok = stats_client_ && stats_client_->fetch(daemon_base_url_, token, mask, fresh);
  Serial.printf("poll url=%s mask=0x%02X ok=%d total=%d sess=%d%%\n",
                daemon_base_url_.c_str(), mask, ok ? 1 : 0,
                fresh.models_today.total_tokens, fresh.session.pct_used);
  if (ok) {
    last_stats_ = fresh;
    last_stats_.stale = false;
    update_all_screens(last_stats_);
    backoff_ms_ = kBackoffStartMs;
    bool active_is_hot = (tileview_ &&
                          (tileview_->active() == SCR_SESSION ||
                           tileview_->active() == SCR_MODELS));
    next_poll_at_ = now + (active_is_hot ? kActivePollMs : kIdlePollMs);
    apply_event(Event::DAEMON_RECOVERED);
  } else {
    last_stats_.stale = true;
    update_all_screens(last_stats_);
    next_poll_at_ = now + backoff_ms_;
    backoff_ms_ = std::min<uint32_t>(backoff_ms_ * 2, kBackoffMaxMs);
    if (chrome_) chrome_->set_health(2);
    apply_event(Event::DAEMON_UNREACHABLE);
  }
}

bool try_connect_saved_wifi() {
  if (!nvs_ || !nvs_->has_wifi_creds()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(nvs_->wifi_ssid().c_str(), nvs_->wifi_psk().c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 12000) return false;
    delay(200);
  }
  return true;
}

static void on_screen_click(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_RELEASED) return;
  if (!tileview_ || tileview_->active() != SCR_SETTINGS) return;
  if (!scr_settings_) return;

  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);

  // Translate global screen coords to tile-local. The Settings tile starts
  // at y = kStatusBarHeight; x is 0-based in the tileview content area.
  int tile_x = p.x;
  int tile_y = p.y - kStatusBarHeight;

  if (tile_y < 0) return;  // tap was in chrome, ignore

  scr_settings_->on_tap(tile_x, tile_y);
}

} // namespace

void app_init() {
  display().init();
  display().setRotation(0);
  display().setBrightness(200);
  touch().probe_and_init();
  lvgl_init();

  nvs_ = new Nvs();
  nvs_->begin();

  ctx_.have_wifi_creds = nvs_->has_wifi_creds();
  ctx_.have_token      = nvs_->has_server() && nvs_->has_bearer_token();

  if (ctx_.have_token) {
    char hp[96];
    snprintf(hp, sizeof(hp), "%s:%u",
             nvs_->server_host().c_str(), nvs_->server_port());
    daemon_base_url_ = std::string("http://") + hp;
    daemon_display_ = hp;
  }

  // Apply stored theme before building UI
  int theme_mode = nvs_->has_theme() ? nvs_->theme_mode() : 0;
  theme::set_mode(theme_mode == 1 ? theme::Mode::Light : theme::Mode::Dark);

  root_ = lv_screen_active();
  lv_obj_set_style_bg_color(root_, theme::bg(), 0);

  chrome_ = new Chrome();
  chrome_->attach(root_);

  pre_pairing_layer_ = lv_obj_create(root_);
  lv_obj_set_size(pre_pairing_layer_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(pre_pairing_layer_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(pre_pairing_layer_, theme::bg(), 0);
  lv_obj_set_style_border_width(pre_pairing_layer_, 0, 0);
  lv_obj_set_style_radius(pre_pairing_layer_, 0, 0);
  lv_obj_set_style_pad_all(pre_pairing_layer_, 0, 0);
  lv_obj_clear_flag(pre_pairing_layer_, LV_OBJ_FLAG_SCROLLABLE);
  // Children: 0=prov, 1=disc (referenced by index in render_state).
  prov_ = new ProvisionScreen();
  disc_ = new DiscoverScreen();
  prov_->build(lv_obj_create(pre_pairing_layer_));
  disc_->build(lv_obj_create(pre_pairing_layer_));

  main_layer_ = lv_obj_create(root_);
  lv_obj_set_size(main_layer_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(main_layer_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(main_layer_, theme::bg(), 0);
  lv_obj_set_style_border_width(main_layer_, 0, 0);
  lv_obj_set_style_radius(main_layer_, 0, 0);
  lv_obj_set_style_pad_all(main_layer_, 0, 0);
  lv_obj_clear_flag(main_layer_, LV_OBJ_FLAG_SCROLLABLE);

  tileview_ = new Tileview();
  tileview_->attach(main_layer_);
  tileview_->on_change(&on_screen_change);
  scr_home_ = new ScreenHome();
  scr_session_ = new ScreenSession();
  scr_models_ = new ScreenModels();
  scr_routines_ = new ScreenRoutines();
  scr_budgets_ = new ScreenBudgets();
  scr_home_->build(tileview_->tile(SCR_HOME));
  scr_session_->build(tileview_->tile(SCR_SESSION));
  scr_models_->build(tileview_->tile(SCR_MODELS));
  scr_routines_->build(tileview_->tile(SCR_ROUTINES));
  scr_budgets_->build(tileview_->tile(SCR_BUDGETS));
  scr_settings_ = new ScreenSettings();
  scr_settings_->build(tileview_->tile(SCR_SETTINGS), nvs_);
  {
    String ip_str = WiFi.localIP().toString();
    String host_str = WiFi.getHostname();
#ifdef FIRMWARE_VERSION
    scr_settings_->set_device_info(host_str.c_str(), ip_str.c_str(), FIRMWARE_VERSION);
#else
    scr_settings_->set_device_info(host_str.c_str(), ip_str.c_str(), "?");
#endif
  }

  lv_obj_add_event_cb(root_, on_screen_click, LV_EVENT_RELEASED, nullptr);

  mdns_ = new MdnsDiscover();
  stats_client_ = new StatsClient();

  // Initial transition from BOOT — uses ctx_ filled above.
  apply_event(Event::TICK);

  // If we believe we have full credentials, try to connect now. On failure,
  // fall back to PROVISION (USB) and let the user re-run setup.
  if (current_state == State::POLL_RENDER) {
    if (!try_connect_saved_wifi()) {
      current_state = State::PROVISION;
      ctx_.have_wifi_creds = false;
      ctx_.have_token = false;
      render_state(current_state);
    }
  }
}

void app_tick() {
  lvgl_tick();
  static LongPress long_press;
  auto ev = touch().poll();
  if (long_press.update(ev.pressed, millis(), kLongPressMs)) {
    Serial.println("factory reset triggered");
    nvs_->factory_reset();
    ctx_ = Context{};
    apply_event(Event::FACTORY_RESET);
  }
  switch (current_state) {
    case State::PROVISION:  perform_provision_usb(); break;
    case State::DISCOVER:   perform_discover();      break;
    case State::POLL_RENDER:perform_poll();          break;
    default: break;
  }
}

} // namespace cyd

#endif  // UNIT_TEST
