#ifndef ROBO_EYES_DISPLAY_H
#define ROBO_EYES_DISPLAY_H

#include "lvgl_display.h"
#include "robo_eyes.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include <memory>

// Monochrome OLED display (SSD1306/SH1106, 128x64) that shows animated
// RoboEyes-style eyes full screen instead of the stock emoji + text layout.
//
// Emotions sent by the xiaozhi server are mapped to eye moods, the device
// state (listening/speaking/idle...) drives the eye behaviour, and chat text
// or notifications appear in a one-line caption strip at the bottom that
// only shows while there is something to read.
class RoboEyesDisplay : public LvglDisplay {
public:
    RoboEyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                    int height, bool mirror_x, bool mirror_y);
    ~RoboEyesDisplay();

    void SetupUI() override;
    void SetChatMessage(const char* role, const char* content) override;
    void SetEmotion(const char* emotion) override;
    void SetTheme(Theme* theme) override;
    bool IsMonochrome() const override { return true; }
    void SetPowerSaveMode(bool on) override;

    // Point the eyes somewhere, e.g. in the direction a servo is turning.
    // Idle glancing resumes after hold_ms.
    void LookAt(RoboEyes::Position pos, int hold_ms = 2500);

    // Show / hide chat text in the caption strip (notifications always show).
    void SetShowSubtitles(bool show);

private:
    bool Lock(int timeout_ms = 0) override;
    void Unlock() override;

    static void OnFrameTimer(lv_timer_t* timer);
    void Frame();
    void ApplyDeviceState(int state);

    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    std::unique_ptr<RoboEyes> eyes_;
    uint8_t* image_data_ = nullptr;  // 2-entry palette + 1bpp pixels
    lv_image_dsc_t image_dsc_ = {};
    lv_obj_t* eyes_image_ = nullptr;
    lv_obj_t* caption_bar_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* hidden_holder_ = nullptr;
    lv_timer_t* frame_timer_ = nullptr;

    bool has_chat_text_ = false;
    bool show_subtitles_ = true;
    bool caption_visible_ = false;
    bool sleeping_ = false;
    int last_state_ = -1;
    int caption_height_ = 16;
    uint32_t look_hold_until_ = 0;
};

#endif  // ROBO_EYES_DISPLAY_H
