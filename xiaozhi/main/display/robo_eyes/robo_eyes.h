// RoboEyes-style animated eyes for monochrome displays.
//
// Look and behaviour are modelled on the FluxGarage "RoboEyes" Arduino library
// (rounded-rectangle eyes, moods drawn as eyelids, blink/idle/laugh/confused
// animations), re-implemented from scratch on a plain 1-bit framebuffer so it
// has no Adafruit_GFX / Arduino dependency and can run inside LVGL.
//
// The engine is platform independent: call Update(now_ms) once per frame and
// read the framebuffer (1 bit per pixel, MSB first, row stride (w+7)/8).
#pragma once

#include <cstdint>
#include <vector>

class RoboEyes {
public:
    enum Mood {
        kMoodDefault = 0,
        kMoodHappy,
        kMoodSad,
        kMoodAngry,
        kMoodTired,
        kMoodSleepy,
        kMoodSurprised,
        kMoodLove,
        kMoodSquint,   // cool / smug: flat half-closed lids
        kMoodSkeptic,  // one lid lower than the other
    };

    enum Position {
        kCenter = 0,
        kN, kNE, kE, kSE, kS, kSW, kW, kNW,
    };

    RoboEyes(int width, int height);

    // Advance the animation and redraw. Returns true if the frame changed.
    bool Update(uint32_t now_ms);

    const uint8_t* framebuffer() const { return fb_.data(); }
    int stride() const { return stride_; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Area the eyes are drawn in (lets a caption strip take the bottom rows).
    void SetViewportHeight(int h);

    // Shape
    void SetEyeSize(int w, int h);
    void SetBorderRadius(int r);
    void SetSpaceBetween(int s);

    // Expression
    void SetMood(Mood mood);
    Mood mood() const { return mood_; }
    void SetPosition(Position pos);
    void SetCurious(bool on) { curious_ = on; }
    void SetTalking(bool on) { talking_ = on; }
    void SetSweat(bool on) { sweat_ = on; }
    void SetTears(bool on) { tears_ = on; }

    // Behaviour
    void SetAutoBlink(bool on, int interval_s = 3, int variation_s = 2);
    void SetIdleMode(bool on, int interval_s = 2, int variation_s = 3);

    // One-shot animations
    void Blink();
    void Wink(bool left);
    void Close();
    void Open();
    void AnimLaugh(uint32_t duration_ms = 1000);
    void AnimConfused(uint32_t duration_ms = 1000);
    void AnimNod(uint32_t duration_ms = 700);

private:
    struct Eye {
        float x, y, w, h;          // current (tweened)
        float tx, ty, tw, th;      // targets
        bool open = true;          // false while a blink is closing this eye
    };

    // Eyelid state, tweened towards targets every frame (all 0..1).
    struct Lids {
        float tired = 0, angry = 0, happy = 0, sad = 0, squint = 0, love = 0;
    };

    void ClearFb();
    void SetPixel(int x, int y, bool on);
    void FillRect(int x, int y, int w, int h, bool on);
    void FillRoundRect(int x, int y, int w, int h, int r, bool on);
    void FillCircle(int cx, int cy, int r, bool on);
    void FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool on);
    void DrawEye(const Eye& e, bool is_left);
    void DrawHeart(int cx, int cy, int size);
    void DrawDrops(uint32_t now_ms);

    uint32_t Rand();
    int RandRange(int lo, int hi);  // inclusive

    int width_, height_, stride_;
    int view_h_;
    std::vector<uint8_t> fb_;
    std::vector<uint8_t> prev_fb_;

    int eye_w_, eye_h_, radius_, space_;
    Eye left_{}, right_{};
    Lids lids_{}, lids_target_{};
    Mood mood_ = kMoodDefault;
    Position pos_ = kCenter;
    float pos_fx_ = 0.5f, pos_fy_ = 0.5f;  // 0..1 across the free area
    bool curious_ = false;
    bool talking_ = false;
    bool sweat_ = false;
    bool tears_ = false;
    bool closed_ = false;

    bool autoblink_ = true;
    int blink_interval_ = 3, blink_variation_ = 2;
    uint32_t next_blink_ = 0;
    uint32_t blink_reopen_at_ = 0;

    bool idle_ = true;
    int idle_interval_ = 2, idle_variation_ = 3;
    uint32_t next_idle_ = 0;

    uint32_t laugh_until_ = 0, confused_until_ = 0, nod_until_ = 0;
    uint32_t anim_start_ = 0;

    uint32_t rng_ = 0x1234567u;
    uint32_t last_ms_ = 0;
    bool first_frame_ = true;
};
