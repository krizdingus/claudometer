#include "app/app_loop.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>
#include <algorithm>
#include <lvgl.h>

#include "app/app_config.h"
#include "app/long_press.h"
#include "app/state_machine.h"
#include "hw/display.h"
#include "hw/nvs.h"
#include "hw/touch.h"
#include "net/mdns_discover.h"
#include "net/pairing_client.h"
#include "net/stats_client.h"
#include "net/wifi_onboarding.h"
#include "ui/chrome.h"
#include "ui/discover_screen.h"
#include "ui/lvgl_glue.h"
#include "ui/pair_screen.h"
#include "ui/provision_screen.h"
#include "ui/screen_budgets.h"
#include "ui/screen_chat.h"
#include "ui/screen_models.h"
#include "ui/screen_routines.h"
#include "ui/screen_session.h"
#include "ui/screen_sonnet.h"
#include "ui/theme.h"
#include "ui/tileview.h"

namespace cyd {

namespace {

State current_state = State::BOOT;
Context ctx_;
Nvs *nvs_ = nullptr;
Chrome *chrome_ = nullptr;
Tileview *tileview_ = nullptr;
ScreenSession *scr_session_ = nullptr;
ScreenModels *scr_models_ = nullptr;
ScreenSonnet *scr_sonnet_ = nullptr;
ScreenChat *scr_chat_ = nullptr;
ScreenRoutines *scr_routines_ = nullptr;
ScreenBudgets *scr_budgets_ = nullptr;
ProvisionScreen *prov_ = nullptr;
DiscoverScreen *disc_ = nullptr;
PairScreen *pair_ = nullptr;
WifiOnboarding *wifi_ = nullptr;
MdnsDiscover *mdns_ = nullptr;
PairingClient *pairing_ = nullptr;
StatsClient *stats_client_ = nullptr;
Stats last_stats_;
std::string daemon_base_url_;
std::string daemon_display_;
std::string pending_code_;
uint32_t next_poll_at_ = 0;
uint32_t backoff_ms_ = kBackoffStartMs;
bool confirm_pressed_ = false;

lv_obj_t *root_ = nullptr;
lv_obj_t *pre_pairing_layer_ = nullptr;   // hosts prov/disc/pair
lv_obj_t *main_layer_ = nullptr;          // hosts tileview + chrome

void show_layer(lv_obj_t *layer) {
  lv_obj_add_flag(pre_pairing_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(main_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(layer, LV_OBJ_FLAG_HIDDEN);
}

void render_state(State s) {
  if (s == State::PROVISION || s == State::DISCOVER || s == State::PAIR) {
    show_layer(pre_pairing_layer_);
    if (s == State::PROVISION) {
      if (prov_) prov_->set_ap_ssid(wifi_->ap_ssid().c_str());
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 2), LV_OBJ_FLAG_HIDDEN);
    } else if (s == State::DISCOVER) {
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 2), LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 2), LV_OBJ_FLAG_HIDDEN);
      if (pair_) {
        pair_->set_host(daemon_display_.c_str());
        pair_->set_code(pending_code_.c_str());
      }
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
  next_poll_at_ = 0;   // refetch with new neighbor mask immediately
}

void on_pair_confirm() { confirm_pressed_ = true; }

void update_all_screens(const Stats &s) {
  if (scr_session_) scr_session_->update(s);
  if (scr_models_) scr_models_->update(s);
  if (scr_sonnet_) scr_sonnet_->update(s);
  if (scr_chat_) scr_chat_->update(s);
  if (scr_routines_) scr_routines_->update(s);
  if (scr_budgets_) scr_budgets_->update(s);
  if (chrome_) chrome_->set_health(s.stale ? 1 : 0);
}

void perform_provision() {
  std::string ssid, psk;
  if (wifi_ && wifi_->run_portal(ssid, psk)) {
    if (nvs_) nvs_->save_wifi(ssid, psk);
    ctx_.have_wifi_creds = true;
    apply_event(Event::WIFI_OK);
  }
}

void perform_discover() {
  // Try a saved hostname first.
  std::string host = nvs_ ? nvs_->daemon_host() : "";
  if (!host.empty()) {
    daemon_base_url_ = "http://" + host;
    daemon_display_ = host;
    apply_event(Event::DAEMON_FOUND);
    return;
  }
  DaemonAddr addr;
  if (!mdns_ || !mdns_->find(addr)) {
    delay(kMdnsQueryMs);
    return;
  }
  char hp[96];
  snprintf(hp, sizeof(hp), "%s:%u", addr.hostname.c_str(), addr.port);
  daemon_base_url_ = std::string("http://") + hp;
  daemon_display_ = addr.display;
  if (nvs_) nvs_->save_daemon_host(hp);
  apply_event(Event::DAEMON_FOUND);
}

void perform_pair() {
  if (pending_code_.empty()) {
    if (!pairing_ || !pairing_->init(daemon_base_url_, device_cyd_id(), pending_code_)) {
      delay(2000);
      return;
    }
    render_state(current_state);  // refresh code on screen
  }
  if (!confirm_pressed_) return;
  confirm_pressed_ = false;
  std::string token;
  if (pairing_ && pairing_->verify(daemon_base_url_, device_cyd_id(),
                      pending_code_, device_name(), token)) {
    if (nvs_) nvs_->save_token(token);
    ctx_.have_token = true;
    pending_code_.clear();
    apply_event(Event::PAIR_CONFIRMED);
  } else {
    pending_code_.clear();
    apply_event(Event::PAIR_FAILED);
  }
}

void perform_poll() {
  uint32_t now = millis();
  if (now < next_poll_at_) return;

  Stats fresh;
  std::string token = nvs_ ? nvs_->token() : "";
  if (stats_client_ && stats_client_->fetch(daemon_base_url_, token,
                          tileview_ ? tileview_->neighbor_mask() : 0, fresh)) {
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
  ctx_.have_token      = nvs_->has_token();

  // Restore daemon URL from NVS so paired boots can poll immediately without
  // re-running discovery. perform_discover() also reads this; we keep both
  // paths populated.
  std::string saved_host = nvs_->daemon_host();
  if (!saved_host.empty()) {
    daemon_base_url_ = "http://" + saved_host;
    daemon_display_ = saved_host;
  }

  root_ = lv_screen_active();
  lv_obj_set_style_bg_color(root_, theme::c(theme::bg), 0);

  chrome_ = new Chrome();
  chrome_->attach(root_);

  pre_pairing_layer_ = lv_obj_create(root_);
  lv_obj_set_size(pre_pairing_layer_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(pre_pairing_layer_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(pre_pairing_layer_, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(pre_pairing_layer_, 0, 0);
  lv_obj_set_style_radius(pre_pairing_layer_, 0, 0);
  lv_obj_set_style_pad_all(pre_pairing_layer_, 0, 0);
  lv_obj_clear_flag(pre_pairing_layer_, LV_OBJ_FLAG_SCROLLABLE);
  // Children: 0=prov, 1=disc, 2=pair (referenced by index in render_state).
  prov_ = new ProvisionScreen();
  disc_ = new DiscoverScreen();
  pair_ = new PairScreen();
  prov_->build(lv_obj_create(pre_pairing_layer_));
  disc_->build(lv_obj_create(pre_pairing_layer_));
  pair_->build(lv_obj_create(pre_pairing_layer_));
  pair_->on_confirm(&on_pair_confirm);

  main_layer_ = lv_obj_create(root_);
  lv_obj_set_size(main_layer_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(main_layer_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(main_layer_, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(main_layer_, 0, 0);
  lv_obj_set_style_radius(main_layer_, 0, 0);
  lv_obj_set_style_pad_all(main_layer_, 0, 0);
  lv_obj_clear_flag(main_layer_, LV_OBJ_FLAG_SCROLLABLE);

  tileview_ = new Tileview();
  tileview_->attach(main_layer_);
  tileview_->on_change(&on_screen_change);
  scr_session_ = new ScreenSession();
  scr_models_ = new ScreenModels();
  scr_sonnet_ = new ScreenSonnet();
  scr_chat_ = new ScreenChat();
  scr_routines_ = new ScreenRoutines();
  scr_budgets_ = new ScreenBudgets();
  scr_session_->build(tileview_->tile(SCR_SESSION));
  scr_models_->build(tileview_->tile(SCR_MODELS));
  scr_sonnet_->build(tileview_->tile(SCR_SONNET));
  scr_chat_->build(tileview_->tile(SCR_CHAT));
  scr_routines_->build(tileview_->tile(SCR_ROUTINES));
  scr_budgets_->build(tileview_->tile(SCR_BUDGETS));

  wifi_ = new WifiOnboarding();
  mdns_ = new MdnsDiscover();
  pairing_ = new PairingClient();
  stats_client_ = new StatsClient();

  // Initial transition from BOOT.
  apply_event(Event::TICK);

  // Auto-reconnect with saved credentials.
  if (current_state == State::DISCOVER || current_state == State::POLL_RENDER) {
    if (!wifi_->try_saved(nvs_->wifi_ssid(), nvs_->wifi_psk())) {
      // Fall back to PROVISION if creds no longer valid.
      current_state = State::PROVISION;
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
    pending_code_.clear();
    apply_event(Event::FACTORY_RESET);
  }
  switch (current_state) {
    case State::PROVISION:  perform_provision(); break;
    case State::DISCOVER:   perform_discover();  break;
    case State::PAIR:       perform_pair();      break;
    case State::POLL_RENDER:perform_poll();      break;
    default: break;
  }
}

} // namespace cyd

#endif  // UNIT_TEST
