#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "display/robo_eyes/robo_eyes_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "esp32_camera.h"
#include "dual_servo.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "DigilogS3CamOledBoard"

class DigilogS3CamOledBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    RoboEyesDisplay* eyes_display_ = nullptr;
    Button boot_button_;
    Esp32Camera* camera_ = nullptr;
    DualServo* servos_ = nullptr;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)DISPLAY_I2C_PORT,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // The module can need a moment after power-up, so probe for it for up to ~1.2 s
        // and accept either common address (0x3C, or 0x3D when the module is strapped).
        vTaskDelay(pdMS_TO_TICKS(200));
        uint8_t oled_addr = 0;
        for (int attempt = 0; attempt < 10 && oled_addr == 0; attempt++) {
            if (i2c_master_probe(display_i2c_bus_, 0x3C, 50) == ESP_OK) {
                oled_addr = 0x3C;
            } else if (i2c_master_probe(display_i2c_bus_, 0x3D, 50) == ESP_OK) {
                oled_addr = 0x3D;
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        if (oled_addr == 0) {
            ESP_LOGE(TAG, "OLED not found on I2C (SDA %d, SCL %d), running without a display",
                     DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);
            display_ = new NoDisplay();
            return;
        }
        ESP_LOGI(TAG, "OLED found at 0x%02X", oled_addr);

        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = oled_addr,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "OLED did not accept init commands, running without a display");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        eyes_display_ = new RoboEyesDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = eyes_display_;
    }

    void InitializeCamera() {
        camera_config_t config = {};
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = CAMERA_PIN_SIOD;
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 0;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    // Glance the eyes the way the robot is about to turn, then move. Screen left/right is the
    // viewer's left/right, so the robot turning left (smaller yaw) looks to the viewer's right.
    void MoveServo(DualServo::Axis axis, float target_deg) {
        float delta = target_deg - servos_->GetTarget(axis);
        if (eyes_display_ != nullptr && delta != 0) {
            RoboEyes::Position pos = axis == DualServo::kYaw
                ? (delta < 0 ? RoboEyes::kE : RoboEyes::kW)
                : (delta > 0 ? RoboEyes::kN : RoboEyes::kS);
            eyes_display_->LookAt(pos, 1500);
        }
        servos_->SetTarget(axis, target_deg);
    }

    // Tool names and meanings match the previous Digilog firmware, so existing
    // voice prompts keep working. 90 is centered; for yaw, smaller turns left.
    void InitializeServoTools() {
        servos_ = new DualServo();
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.base.set_angle",
            "Rotate the desk robot camera/base (yaw) to an absolute angle. Use for look/turn commands: "
            "90 is center/forward, smaller angles turn left, larger turn right. Angle range 0-180; "
            "values outside the safe travel limits are clamped.",
            PropertyList({Property("angle", kPropertyTypeInteger, 90, 0, 180)}),
            [this](const PropertyList& properties) -> ToolResult {
                MoveServo(DualServo::kYaw, properties["angle"].value<int>());
                return true;
            });
        mcp.AddTool("self.base.turn_left",
            "Turn the desk robot camera/base left (decrease yaw angle). Call this when the user asks to look left or turn left.",
            PropertyList({Property("degrees", kPropertyTypeInteger, 20, 1, 180)}),
            [this](const PropertyList& properties) -> ToolResult {
                MoveServo(DualServo::kYaw, servos_->GetTarget(DualServo::kYaw) - properties["degrees"].value<int>());
                return true;
            });
        mcp.AddTool("self.base.turn_right",
            "Turn the desk robot camera/base right (increase yaw angle). Call this when the user asks to look right or turn right.",
            PropertyList({Property("degrees", kPropertyTypeInteger, 20, 1, 180)}),
            [this](const PropertyList& properties) -> ToolResult {
                MoveServo(DualServo::kYaw, servos_->GetTarget(DualServo::kYaw) + properties["degrees"].value<int>());
                return true;
            });
        mcp.AddTool("self.base.center",
            "Center the desk robot camera/base (yaw) to forward-facing 90 degrees. Use when the user asks to look forward, center, or reset pan. Does not change head pitch/tilt.",
            PropertyList(),
            [this](const PropertyList&) -> ToolResult {
                MoveServo(DualServo::kYaw, SERVO_CENTER_DEG);
                return true;
            });

        mcp.AddTool("self.head.set_angle",
            "Set the desk robot head pitch/tilt to an absolute angle. 90 is level/center; "
            "smaller angles look down, larger look up. Angle range 0-180; values outside the safe travel limits are clamped.",
            PropertyList({Property("angle", kPropertyTypeInteger, 90, 0, 180)}),
            [this](const PropertyList& properties) -> ToolResult {
                MoveServo(DualServo::kPitch, properties["angle"].value<int>());
                return true;
            });
        mcp.AddTool("self.head.look_up",
            "Tilt the desk robot head up (increase pitch angle). Call when the user asks to look up.",
            PropertyList({Property("degrees", kPropertyTypeInteger, 15, 1, 90)}),
            [this](const PropertyList& properties) -> ToolResult {
                MoveServo(DualServo::kPitch, servos_->GetTarget(DualServo::kPitch) + properties["degrees"].value<int>());
                return true;
            });
        mcp.AddTool("self.head.look_down",
            "Tilt the desk robot head down (decrease pitch angle). Call when the user asks to look down.",
            PropertyList({Property("degrees", kPropertyTypeInteger, 15, 1, 90)}),
            [this](const PropertyList& properties) -> ToolResult {
                MoveServo(DualServo::kPitch, servos_->GetTarget(DualServo::kPitch) - properties["degrees"].value<int>());
                return true;
            });
        mcp.AddTool("self.head.center",
            "Center the desk robot head pitch/tilt to level 90 degrees. Use when the user asks to level the head, look straight, or reset tilt.",
            PropertyList(),
            [this](const PropertyList&) -> ToolResult {
                MoveServo(DualServo::kPitch, SERVO_CENTER_DEG);
                return true;
            });
    }

public:
    DigilogS3CamOledBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeCamera();
        InitializeServoTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(DigilogS3CamOledBoard);
