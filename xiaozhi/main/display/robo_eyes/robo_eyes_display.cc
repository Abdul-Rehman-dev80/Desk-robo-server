#include "robo_eyes_display.h"

#include "application.h"
#include "assets/lang_config.h"
#include "lvgl_font.h"
#include "lvgl_theme.h"
#include "settings.h"

#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_random.h>
#include <src/misc/cache/instance/lv_image_cache.h>

#include <algorithm>
#include <cstring>
#include <string>

#define TAG "RoboEyesDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_material_symbols_30_1);
LV_FONT_DECLARE(font_noto_emoji_30_1);

// esp_lvgl_port's monochrome conversion turns *dark* pixels on and bright
// pixels off (that is why the stock OLED UI draws black text on white).
#define PIXEL_ON lv_color_black()
#define PIXEL_OFF lv_color_white()

namespace {

constexpr uint32_t kFrameIntervalMs = 30;

struct EmotionStyle {
    const char* name;
    RoboEyes::Mood mood;
    enum Extra { kNone, kLaugh, kConfused, kWink, kTears, kSweat, kCurious, kBlink } extra;
    RoboEyes::Position look;  // kCenter = don't care
};

// Emotions the xiaozhi server sends (see docs/emotion) mapped to eye styles.
const EmotionStyle kEmotionStyles[] = {
    {"neutral", RoboEyes::kMoodDefault, EmotionStyle::kNone, RoboEyes::kCenter},
    {"happy", RoboEyes::kMoodHappy, EmotionStyle::kNone, RoboEyes::kCenter},
    {"laughing", RoboEyes::kMoodHappy, EmotionStyle::kLaugh, RoboEyes::kCenter},
    {"funny", RoboEyes::kMoodHappy, EmotionStyle::kLaugh, RoboEyes::kCenter},
    {"sad", RoboEyes::kMoodSad, EmotionStyle::kNone, RoboEyes::kS},
    {"crying", RoboEyes::kMoodSad, EmotionStyle::kTears, RoboEyes::kCenter},
    {"angry", RoboEyes::kMoodAngry, EmotionStyle::kNone, RoboEyes::kCenter},
    {"loving", RoboEyes::kMoodLove, EmotionStyle::kNone, RoboEyes::kCenter},
    {"kissy", RoboEyes::kMoodLove, EmotionStyle::kWink, RoboEyes::kCenter},
    {"embarrassed", RoboEyes::kMoodHappy, EmotionStyle::kSweat, RoboEyes::kS},
    {"surprised", RoboEyes::kMoodSurprised, EmotionStyle::kNone, RoboEyes::kCenter},
    {"shocked", RoboEyes::kMoodSurprised, EmotionStyle::kBlink, RoboEyes::kCenter},
    {"thinking", RoboEyes::kMoodSkeptic, EmotionStyle::kCurious, RoboEyes::kNE},
    {"winking", RoboEyes::kMoodHappy, EmotionStyle::kWink, RoboEyes::kCenter},
    {"cool", RoboEyes::kMoodSquint, EmotionStyle::kNone, RoboEyes::kCenter},
    {"relaxed", RoboEyes::kMoodTired, EmotionStyle::kNone, RoboEyes::kCenter},
    {"delicious", RoboEyes::kMoodHappy, EmotionStyle::kLaugh, RoboEyes::kCenter},
    {"confident", RoboEyes::kMoodSquint, EmotionStyle::kNone, RoboEyes::kN},
    {"sleepy", RoboEyes::kMoodSleepy, EmotionStyle::kNone, RoboEyes::kS},
    {"silly", RoboEyes::kMoodHappy, EmotionStyle::kConfused, RoboEyes::kCenter},
    {"confused", RoboEyes::kMoodSkeptic, EmotionStyle::kConfused, RoboEyes::kCenter},
    {"tired", RoboEyes::kMoodTired, EmotionStyle::kNone, RoboEyes::kCenter},
    {"curious", RoboEyes::kMoodDefault, EmotionStyle::kCurious, RoboEyes::kE},
};

uint32_t NowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

}  // namespace

RoboEyesDisplay::RoboEyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                 int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_material_symbols_30_1);
    auto emoji_font = std::make_shared<LvglBuiltInFont>(&font_noto_emoji_30_1);

    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);
    dark_theme->set_emoji_font(emoji_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation =
            {
                .swap_xy = false,
                .mirror_x = mirror_x,
                .mirror_y = mirror_y,
            },
        .flags =
            {
                .buff_dma = 1,
                .buff_spiram = 0,
                .sw_rotate = 0,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    eyes_ = std::make_unique<RoboEyes>(width_, height_);
}

RoboEyesDisplay::~RoboEyesDisplay() {
    if (frame_timer_ != nullptr) {
        lv_timer_delete(frame_timer_);
    }
    if (caption_bar_ != nullptr) {
        status_label_ = nullptr;
        notification_label_ = nullptr;
        lv_obj_del(caption_bar_);
    }
    if (hidden_holder_ != nullptr) {
        network_label_ = nullptr;
        mute_label_ = nullptr;
        battery_label_ = nullptr;
        lv_obj_del(hidden_holder_);
    }
    if (eyes_image_ != nullptr) {
        lv_obj_del(eyes_image_);
    }
    if (image_data_ != nullptr) {
        heap_caps_free(image_data_);
    }
    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool RoboEyesDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void RoboEyesDisplay::Unlock() { lvgl_port_unlock(); }

void RoboEyesDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }
    Display::SetupUI();

    DisplayLockGuard lock(this);
    if (display_ == nullptr) {
        return;
    }

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    caption_height_ = std::clamp<int>(text_font->line_height + 1, 12, height_ / 3);

    auto screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, PIXEL_OFF, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, PIXEL_ON, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    /* Eyes: a full-screen 1bpp indexed image the RoboEyes engine draws into */
    const int stride = eyes_->stride();
    const size_t palette_size = 2 * sizeof(lv_color32_t);
    const size_t data_size = palette_size + stride * height_;
    image_data_ = static_cast<uint8_t*>(heap_caps_calloc(1, data_size, MALLOC_CAP_INTERNAL));
    lv_color32_t* palette = reinterpret_cast<lv_color32_t*>(image_data_);
    palette[0] = lv_color_to_32(PIXEL_OFF, LV_OPA_COVER);
    palette[1] = lv_color_to_32(PIXEL_ON, LV_OPA_COVER);

    image_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    image_dsc_.header.cf = LV_COLOR_FORMAT_I1;
    image_dsc_.header.w = width_;
    image_dsc_.header.h = height_;
    image_dsc_.header.stride = stride;
    image_dsc_.data_size = data_size;
    image_dsc_.data = image_data_;

    eyes_image_ = lv_image_create(screen);
    lv_image_set_src(eyes_image_, &image_dsc_);
    lv_obj_set_pos(eyes_image_, 0, 0);

    /* Labels LvglDisplay manages that this layout doesn't show (status text,
       network / battery / mute icons). They live in a hidden container so the
       base class can keep updating them. */
    hidden_holder_ = lv_obj_create(screen);
    lv_obj_add_flag(hidden_holder_, LV_OBJ_FLAG_HIDDEN);
    status_label_ = lv_label_create(hidden_holder_);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    network_label_ = lv_label_create(hidden_holder_);
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    mute_label_ = lv_label_create(hidden_holder_);
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    battery_label_ = lv_label_create(hidden_holder_);
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    /* Caption strip at the bottom for chat text and notifications */
    caption_bar_ = lv_obj_create(screen);
    lv_obj_set_size(caption_bar_, width_, caption_height_);
    lv_obj_align(caption_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(caption_bar_, 0, 0);
    lv_obj_set_style_bg_color(caption_bar_, PIXEL_OFF, 0);
    lv_obj_set_style_bg_opa(caption_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(caption_bar_, 0, 0);
    lv_obj_set_style_pad_all(caption_bar_, 0, 0);
    lv_obj_set_scrollbar_mode(caption_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(caption_bar_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(caption_bar_);
    lv_obj_set_width(chat_message_label_, width_);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_align(chat_message_label_, LV_ALIGN_CENTER, 0, 0);

    // Start scrolling long captions after a short pause
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 800);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(50, 300, 60000),
                                   LV_PART_MAIN);

    notification_label_ = lv_label_create(caption_bar_);
    lv_obj_set_width(notification_label_, width_);
    lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    eyes_->SetAutoBlink(true, 3, 3);
    eyes_->SetIdleMode(true, 2, 3);
    eyes_->SetCurious(true);

    frame_timer_ = lv_timer_create(OnFrameTimer, kFrameIntervalMs, this);
    Frame();
}

void RoboEyesDisplay::OnFrameTimer(lv_timer_t* timer) {
    auto self = static_cast<RoboEyesDisplay*>(lv_timer_get_user_data(timer));
    self->Frame();
}

// Runs in the LVGL task with the LVGL lock held.
void RoboEyesDisplay::Frame() {
    if (sleeping_) {
        return;
    }
    uint32_t now = NowMs();

    int state = Application::GetInstance().GetDeviceState();
    if (state != last_state_) {
        ApplyDeviceState(state);
        last_state_ = state;
    }
    if (look_hold_until_ != 0 && now >= look_hold_until_) {
        look_hold_until_ = 0;
        if (last_state_ == kDeviceStateIdle || last_state_ == kDeviceStateUnknown) {
            eyes_->SetIdleMode(true, 2, 3);
        } else {
            eyes_->SetPosition(RoboEyes::kCenter);
        }
    }

    // The caption strip shows while there is text; the eyes shrink above it.
    bool notification_visible = notification_label_ != nullptr &&
                                !lv_obj_has_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    bool want_caption = notification_visible || (show_subtitles_ && has_chat_text_);
    if (want_caption != caption_visible_) {
        caption_visible_ = want_caption;
        if (want_caption) {
            lv_obj_remove_flag(caption_bar_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(caption_bar_, LV_OBJ_FLAG_HIDDEN);
        }
        if (notification_visible) {
            lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (caption_visible_) {
        if (notification_visible) {
            lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    eyes_->SetViewportHeight(caption_visible_ ? height_ - caption_height_ : height_);

    if (eyes_->Update(now)) {
        std::memcpy(image_data_ + 2 * sizeof(lv_color32_t), eyes_->framebuffer(),
                    eyes_->stride() * height_);
        lv_image_cache_drop(&image_dsc_);
        lv_obj_invalidate(eyes_image_);
    }
}

void RoboEyesDisplay::ApplyDeviceState(int state) {
    switch (state) {
        case kDeviceStateStarting:
            eyes_->SetMood(RoboEyes::kMoodDefault);
            eyes_->SetIdleMode(false);
            eyes_->SetPosition(RoboEyes::kCenter);
            break;
        case kDeviceStateWifiConfiguring:
        case kDeviceStateActivating:
            // Looking around, a bit puzzled, while waiting for setup.
            eyes_->SetTalking(false);
            eyes_->SetIdleMode(true, 1, 2);
            eyes_->SetMood(RoboEyes::kMoodSkeptic);
            break;
        case kDeviceStateIdle:
        case kDeviceStateUnknown:
            eyes_->SetTalking(false);
            eyes_->SetCurious(true);
            eyes_->SetIdleMode(true, 2, 3);
            eyes_->SetAutoBlink(true, 3, 3);
            eyes_->SetTears(false);
            eyes_->SetSweat(false);
            break;
        case kDeviceStateConnecting:
            eyes_->SetTalking(false);
            eyes_->SetIdleMode(true, 1, 1);
            break;
        case kDeviceStateListening:
            // Attentive: look straight at the person, blink a little less.
            eyes_->SetTalking(false);
            eyes_->SetIdleMode(false);
            eyes_->SetPosition(RoboEyes::kCenter);
            eyes_->SetAutoBlink(true, 4, 3);
            eyes_->SetTears(false);
            eyes_->SetSweat(false);
            break;
        case kDeviceStateSpeaking:
        case kDeviceStateNotifying:
            eyes_->SetIdleMode(false);
            eyes_->SetPosition(RoboEyes::kCenter);
            eyes_->SetTalking(true);
            eyes_->SetAutoBlink(true, 3, 2);
            break;
        case kDeviceStateUpgrading:
            eyes_->SetTalking(false);
            eyes_->SetIdleMode(false);
            eyes_->SetMood(RoboEyes::kMoodSquint);
            eyes_->SetPosition(RoboEyes::kS);
            break;
        case kDeviceStateFatalError:
            eyes_->SetTalking(false);
            eyes_->SetIdleMode(false);
            eyes_->SetMood(RoboEyes::kMoodSad);
            eyes_->SetPosition(RoboEyes::kS);
            break;
        default:
            break;
    }
}

void RoboEyesDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    if (!eyes_ || eyes_image_ == nullptr) {
        return;
    }

    const EmotionStyle* style = nullptr;
    for (const auto& s : kEmotionStyles) {
        if (strcmp(s.name, emotion) == 0) {
            style = &s;
            break;
        }
    }
    if (style == nullptr) {
        // Unknown names (e.g. "robot_2", "microchip_ai", "download") -> neutral eyes
        style = &kEmotionStyles[0];
    }

    eyes_->SetMood(style->mood);
    eyes_->SetTears(style->extra == EmotionStyle::kTears);
    eyes_->SetSweat(style->extra == EmotionStyle::kSweat);
    eyes_->SetCurious(style->extra == EmotionStyle::kCurious || last_state_ == kDeviceStateIdle);
    switch (style->extra) {
        case EmotionStyle::kLaugh: eyes_->AnimLaugh(); break;
        case EmotionStyle::kConfused: eyes_->AnimConfused(); break;
        case EmotionStyle::kWink: eyes_->Wink(esp_random() & 1); break;
        case EmotionStyle::kBlink: eyes_->Blink(); break;
        default: break;
    }
    if (style->look != RoboEyes::kCenter) {
        eyes_->SetIdleMode(false);
        eyes_->SetPosition(style->look);
        look_hold_until_ = NowMs() + 3000;
    } else if (last_state_ != kDeviceStateIdle) {
        eyes_->SetPosition(RoboEyes::kCenter);
    }
}

void RoboEyesDisplay::LookAt(RoboEyes::Position pos, int hold_ms) {
    DisplayLockGuard lock(this);
    if (!eyes_) {
        return;
    }
    eyes_->SetIdleMode(false);
    eyes_->SetPosition(pos);
    look_hold_until_ = NowMs() + std::max(hold_ms, 1);
}

void RoboEyesDisplay::SetShowSubtitles(bool show) {
    DisplayLockGuard lock(this);
    show_subtitles_ = show;
}

void RoboEyesDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    std::string text = content != nullptr ? content : "";
    std::replace(text.begin(), text.end(), '\n', ' ');
    has_chat_text_ = !text.empty();
    lv_anim_delete(chat_message_label_, nullptr);
    lv_label_set_text(chat_message_label_, text.c_str());
}

void RoboEyesDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);
    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();
    lv_obj_set_style_text_font(lv_screen_active(), text_font, 0);
}

void RoboEyesDisplay::SetPowerSaveMode(bool on) {
    {
        DisplayLockGuard lock(this);
        if (eyes_) {
            // Close the eyes when the robot dozes off, open them on wake-up.
            if (on) {
                eyes_->SetMood(RoboEyes::kMoodSleepy);
                eyes_->Close();
            } else {
                eyes_->Open();
                eyes_->SetMood(RoboEyes::kMoodDefault);
            }
        }
    }
    if (panel_) {
        Settings settings("wifi", false);
        if (settings.GetBool("power_save_display_off", false)) {
            esp_lcd_panel_disp_on_off(panel_, !on);
        }
    }
    LvglDisplay::SetPowerSaveMode(on);
}
