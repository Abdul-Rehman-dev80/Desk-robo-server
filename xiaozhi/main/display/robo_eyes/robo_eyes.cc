#include "robo_eyes.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

float Approach(float cur, float target, float dt_ms, float tau_ms) {
    float a = 1.0f - std::exp(-dt_ms / tau_ms);
    float v = cur + (target - cur) * a;
    if (std::fabs(target - v) < 0.05f) {
        v = target;
    }
    return v;
}

}  // namespace

RoboEyes::RoboEyes(int width, int height)
    : width_(width), height_(height), stride_((width + 7) / 8), view_h_(height) {
    fb_.assign(stride_ * height_, 0);
    prev_fb_.assign(stride_ * height_, 0xFF);

    // Proportions of the RoboEyes defaults (36x36 eyes, r=8, gap=10 on 128x64).
    eye_w_ = width * 36 / 128;
    eye_h_ = height * 36 / 64;
    radius_ = std::max(2, eye_h_ * 8 / 36);
    space_ = width * 10 / 128;

    left_.w = left_.tw = eye_w_;
    right_.w = right_.tw = eye_w_;
    // Start closed so the first thing the robot does is open its eyes.
    left_.h = right_.h = 1;
    left_.th = right_.th = eye_h_;
    SetPosition(kCenter);
    left_.x = left_.tx;
    left_.y = left_.ty;
    right_.x = right_.tx;
    right_.y = right_.ty;
}

uint32_t RoboEyes::Rand() {
    // xorshift32
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

int RoboEyes::RandRange(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + static_cast<int>(Rand() % static_cast<uint32_t>(hi - lo + 1));
}

void RoboEyes::SetViewportHeight(int h) {
    view_h_ = std::clamp(h, 16, height_);
}

void RoboEyes::SetEyeSize(int w, int h) {
    eye_w_ = w;
    eye_h_ = h;
}

void RoboEyes::SetBorderRadius(int r) { radius_ = r; }
void RoboEyes::SetSpaceBetween(int s) { space_ = s; }

void RoboEyes::SetMood(Mood mood) {
    mood_ = mood;
    lids_target_ = Lids{};
    switch (mood) {
        case kMoodHappy: lids_target_.happy = 1; break;
        case kMoodSad: lids_target_.sad = 1; break;
        case kMoodAngry: lids_target_.angry = 1; break;
        case kMoodTired: lids_target_.tired = 1; break;
        case kMoodSleepy: lids_target_.tired = 0.6f; lids_target_.squint = 1; break;
        case kMoodSquint: lids_target_.squint = 0.8f; break;
        case kMoodSkeptic: lids_target_.squint = 0.5f; break;
        case kMoodLove: lids_target_.love = 1; break;
        default: break;
    }
}

void RoboEyes::SetPosition(Position pos) {
    pos_ = pos;
    switch (pos) {
        case kN: pos_fx_ = 0.5f; pos_fy_ = 0.0f; break;
        case kNE: pos_fx_ = 1.0f; pos_fy_ = 0.0f; break;
        case kE: pos_fx_ = 1.0f; pos_fy_ = 0.5f; break;
        case kSE: pos_fx_ = 1.0f; pos_fy_ = 1.0f; break;
        case kS: pos_fx_ = 0.5f; pos_fy_ = 1.0f; break;
        case kSW: pos_fx_ = 0.0f; pos_fy_ = 1.0f; break;
        case kW: pos_fx_ = 0.0f; pos_fy_ = 0.5f; break;
        case kNW: pos_fx_ = 0.0f; pos_fy_ = 0.0f; break;
        default: pos_fx_ = 0.5f; pos_fy_ = 0.5f; break;
    }
}

void RoboEyes::SetAutoBlink(bool on, int interval_s, int variation_s) {
    autoblink_ = on;
    blink_interval_ = interval_s;
    blink_variation_ = variation_s;
}

void RoboEyes::SetIdleMode(bool on, int interval_s, int variation_s) {
    idle_ = on;
    idle_interval_ = interval_s;
    idle_variation_ = variation_s;
}

void RoboEyes::Blink() {
    left_.open = right_.open = false;
    blink_reopen_at_ = last_ms_ + 110;
}

void RoboEyes::Wink(bool left) {
    (left ? left_ : right_).open = false;
    blink_reopen_at_ = last_ms_ + 260;
}

void RoboEyes::Close() {
    closed_ = true;
}

void RoboEyes::Open() {
    closed_ = false;
    left_.open = right_.open = true;
}

void RoboEyes::AnimLaugh(uint32_t duration_ms) {
    anim_start_ = last_ms_;
    laugh_until_ = last_ms_ + duration_ms;
}

void RoboEyes::AnimConfused(uint32_t duration_ms) {
    anim_start_ = last_ms_;
    confused_until_ = last_ms_ + duration_ms;
}

void RoboEyes::AnimNod(uint32_t duration_ms) {
    anim_start_ = last_ms_;
    nod_until_ = last_ms_ + duration_ms;
}

// ---------------------------------------------------------------------------
// Drawing primitives on the 1-bit framebuffer (1 = lit pixel)

void RoboEyes::ClearFb() { std::memset(fb_.data(), 0, fb_.size()); }

void RoboEyes::SetPixel(int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    uint8_t& b = fb_[y * stride_ + (x >> 3)];
    uint8_t mask = 0x80 >> (x & 7);
    if (on) b |= mask; else b &= ~mask;
}

void RoboEyes::FillRect(int x, int y, int w, int h, bool on) {
    int x0 = std::max(x, 0), x1 = std::min(x + w, width_);
    int y0 = std::max(y, 0), y1 = std::min(y + h, height_);
    for (int yy = y0; yy < y1; ++yy) {
        for (int xx = x0; xx < x1; ++xx) SetPixel(xx, yy, on);
    }
}

void RoboEyes::FillRoundRect(int x, int y, int w, int h, int r, bool on) {
    if (w <= 0 || h <= 0) return;
    r = std::min(r, std::min(w, h) / 2);
    for (int yy = 0; yy < h; ++yy) {
        int inset = 0;
        int dy = -1;
        if (yy < r) dy = r - yy;
        else if (yy >= h - r) dy = yy - (h - r - 1);
        if (dy > 0) {
            float fx = r - std::sqrt(std::max(0.0f, float(r * r) - float((dy - 0.5f) * (dy - 0.5f))));
            inset = static_cast<int>(std::lround(fx));
        }
        FillRect(x + inset, y + yy, w - 2 * inset, 1, on);
    }
}

void RoboEyes::FillCircle(int cx, int cy, int r, bool on) {
    for (int dy = -r; dy <= r; ++dy) {
        int dx = static_cast<int>(std::sqrt(float(r * r - dy * dy)) + 0.5f);
        FillRect(cx - dx, cy + dy, 2 * dx + 1, 1, on);
    }
}

void RoboEyes::FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool on) {
    int minx = std::min({x0, x1, x2}), maxx = std::max({x0, x1, x2});
    int miny = std::min({y0, y1, y2}), maxy = std::max({y0, y1, y2});
    auto edge = [](int ax, int ay, int bx, int by, int px, int py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    };
    int area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;
    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            int w0 = edge(x1, y1, x2, y2, x, y);
            int w1 = edge(x2, y2, x0, y0, x, y);
            int w2 = edge(x0, y0, x1, y1, x, y);
            if (area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                SetPixel(x, y, on);
            }
        }
    }
}

void RoboEyes::DrawHeart(int cx, int cy, int size) {
    // Implicit heart curve: (x^2 + y^2 - 1)^3 - x^2 * y^3 <= 0
    float s = size / 2.4f;
    int half = size;
    for (int py = -half; py <= half; ++py) {
        for (int px = -half; px <= half; ++px) {
            float x = px / s;
            float y = -(py / s) + 0.25f;
            float a = x * x + y * y - 1.0f;
            if (a * a * a - x * x * y * y * y <= 0.0f) {
                SetPixel(cx + px, cy + py, true);
            }
        }
    }
}

void RoboEyes::DrawEye(const Eye& e, bool is_left) {
    int x = static_cast<int>(std::lround(e.x));
    int y = static_cast<int>(std::lround(e.y));
    int w = std::max(1, static_cast<int>(std::lround(e.w)));
    int h = std::max(1, static_cast<int>(std::lround(e.h)));

    if (lids_.love > 0.5f && h > 6) {
        DrawHeart(x + w / 2, y + h / 2, std::min(w, h) * 8 / 10);
        return;
    }

    FillRoundRect(x, y, w, h, static_cast<int>(radius_), true);
    if (h <= 3) return;

    // Top eyelid, drawn per column so moods blend smoothly into each other.
    // s runs 0 at the outer corner to 1 at the inner corner (towards the nose).
    for (int cx = 0; cx < w; ++cx) {
        float t = (w > 1) ? float(cx) / float(w - 1) : 0.5f;
        float s = is_left ? t : 1.0f - t;
        float depth = 0;
        depth = std::max(depth, lids_.tired * h * 0.5f * (1.0f - s));
        depth = std::max(depth, lids_.sad * h * 0.6f * (1.0f - s));
        depth = std::max(depth, lids_.angry * h * 0.55f * s);
        float flat = lids_.squint * h * 0.45f;
        if (mood_ == kMoodSkeptic && !is_left) flat *= 0.2f;  // one eyebrow raised
        depth = std::max(depth, flat);
        int d = static_cast<int>(std::lround(depth));
        if (d > 0) FillRect(x + cx, y - 1, 1, d + 1, false);

        // Bottom lid for happy eyes: arched cut, deepest in the middle.
        if (lids_.happy > 0.01f) {
            float u = 2.0f * t - 1.0f;
            float cut = lids_.happy * h * (0.3f + 0.3f * (1.0f - u * u));
            int c = static_cast<int>(std::lround(cut));
            if (c > 0) FillRect(x + cx, y + h - c, 1, c + 1, false);
        }
    }
}

void RoboEyes::DrawDrops(uint32_t now_ms) {
    if (tears_) {
        // Tears fall from the inner-bottom corner of each eye.
        const Eye* eyes[2] = {&left_, &right_};
        for (int i = 0; i < 2; ++i) {
            const Eye& e = *eyes[i];
            float phase = float((now_ms + i * 450) % 900) / 900.0f;
            int dx = (i == 0) ? static_cast<int>(e.x + e.w * 0.75f) : static_cast<int>(e.x + e.w * 0.25f);
            int top = static_cast<int>(e.y + e.h);
            int dy = top + static_cast<int>(phase * (height_ - top));
            FillCircle(dx, dy, 2, true);
            FillTriangle(dx - 2, dy, dx + 2, dy, dx, dy - 4, true);
        }
    }
    if (sweat_) {
        float phase = float(now_ms % 1400) / 1400.0f;
        int sx = static_cast<int>(right_.x + right_.w) + 3;
        if (sx > width_ - 4) sx = width_ - 4;
        int sy = 2 + static_cast<int>(phase * 14);
        FillCircle(sx, sy + 3, 2, true);
        FillTriangle(sx - 2, sy + 3, sx + 2, sy + 3, sx, sy - 2, true);
    }
}

// ---------------------------------------------------------------------------

bool RoboEyes::Update(uint32_t now_ms) {
    float dt = first_frame_ ? 16.0f : float(now_ms - last_ms_);
    dt = std::clamp(dt, 1.0f, 200.0f);
    last_ms_ = now_ms;
    if (first_frame_) {
        first_frame_ = false;
        next_blink_ = now_ms + 1500;
        next_idle_ = now_ms + 2500;
        rng_ ^= now_ms * 2654435761u;
    }

    // ---- behaviour -------------------------------------------------------
    if (autoblink_ && !closed_ && now_ms >= next_blink_) {
        Blink();
        next_blink_ = now_ms + 1000u * RandRange(blink_interval_, blink_interval_ + blink_variation_);
        // Occasionally double-blink, like the real thing.
        if ((Rand() & 7) == 0) next_blink_ = now_ms + 350;
    }
    if (blink_reopen_at_ && now_ms >= blink_reopen_at_ && !closed_) {
        left_.open = right_.open = true;
        blink_reopen_at_ = 0;
    }
    if (idle_ && now_ms >= next_idle_) {
        // Glance somewhere random, biased to looking roughly forward.
        pos_fx_ = RandRange(0, 100) / 100.0f;
        pos_fy_ = RandRange(15, 85) / 100.0f;
        if ((Rand() % 3) == 0) {
            pos_fx_ = 0.5f;
            pos_fy_ = 0.5f;
        }
        next_idle_ = now_ms + 1000u * RandRange(idle_interval_, idle_interval_ + idle_variation_);
    }

    // ---- targets ---------------------------------------------------------
    float base_w = eye_w_, base_h = eye_h_;
    if (mood_ == kMoodSurprised) {
        base_w = eye_w_ * 1.1f;
        base_h = eye_h_ * 1.25f;
    }
    // Keep eyes inside the viewport (e.g. when the caption strip is visible).
    float max_h = view_h_ - 4.0f;
    if (base_h > max_h) {
        base_w *= max_h / base_h * 0.15f + 0.85f;
        base_h = max_h;
    }

    float total_w = 2 * base_w + space_;
    float free_x = width_ - total_w;
    float free_y = view_h_ - base_h;
    float lx = free_x * pos_fx_;
    float ly = free_y * pos_fy_;

    float lh = base_h, rh = base_h;
    float lw = base_w, rw = base_w;
    if (curious_) {
        // The eye on the side we're looking towards grows, like RoboEyes' curious mode.
        if (pos_fx_ < 0.25f) lh += 8, lw += 2;
        if (pos_fx_ > 0.75f) rh += 8, rw += 2;
    }
    if (talking_) {
        float t = now_ms / 1000.0f;
        float wob = 0.5f + 0.5f * std::sin(t * 13.0f) * std::sin(t * 4.7f + 1.0f);
        lh *= 0.86f + 0.14f * wob;
        rh *= 0.86f + 0.14f * wob;
    }

    float oy = 0, ox = 0;
    if (now_ms < laugh_until_) {
        oy = ((now_ms / 60) & 1) ? -3.0f : 3.0f;
    }
    if (now_ms < confused_until_) {
        ox = ((now_ms / 80) & 1) ? -5.0f : 5.0f;
    }
    if (now_ms < nod_until_) {
        oy = 5.0f * std::sin((now_ms - anim_start_) / 110.0f);
    }

    left_.tx = lx + ox;
    right_.tx = lx + base_w + space_ + ox;
    left_.tw = lw;
    right_.tw = rw;
    left_.th = (closed_ || !left_.open) ? 1 : lh;
    right_.th = (closed_ || !right_.open) ? 1 : rh;
    // Keep the vertical centre of each eye fixed while its height changes.
    left_.ty = ly + (base_h - left_.th) / 2 + oy;
    right_.ty = ly + (base_h - right_.th) / 2 + oy;

    // ---- tween -----------------------------------------------------------
    const float tau_size = 28.0f, tau_pos = 55.0f, tau_lid = 70.0f;
    for (Eye* e : {&left_, &right_}) {
        // Tween the vertical centre separately from the height so blinks
        // close towards the centre line instead of sliding up.
        float centre_now = e->y + e->h / 2;
        float centre_target = e->ty + e->th / 2;
        e->h = Approach(e->h, e->th, dt, tau_size);
        e->w = Approach(e->w, e->tw, dt, tau_size);
        e->x = Approach(e->x, e->tx, dt, tau_pos);
        float c = Approach(centre_now, centre_target, dt, tau_pos);
        e->y = c - e->h / 2;
    }
    lids_.tired = Approach(lids_.tired, lids_target_.tired, dt, tau_lid);
    lids_.angry = Approach(lids_.angry, lids_target_.angry, dt, tau_lid);
    lids_.happy = Approach(lids_.happy, lids_target_.happy, dt, tau_lid);
    lids_.sad = Approach(lids_.sad, lids_target_.sad, dt, tau_lid);
    lids_.squint = Approach(lids_.squint, lids_target_.squint, dt, tau_lid);
    lids_.love = lids_target_.love;

    // ---- draw ------------------------------------------------------------
    ClearFb();
    DrawEye(left_, true);
    DrawEye(right_, false);
    DrawDrops(now_ms);
    // Anything drawn below the viewport (tears) is fine, but clear eye pixels
    // that would sit under a caption strip.
    if (view_h_ < height_ && !tears_) {
        std::memset(fb_.data() + view_h_ * stride_, 0, (height_ - view_h_) * stride_);
    }

    bool changed = std::memcmp(fb_.data(), prev_fb_.data(), fb_.size()) != 0;
    if (changed) {
        std::memcpy(prev_fb_.data(), fb_.data(), fb_.size());
    }
    return changed;
}
