#ifndef STEAM_CONTROLLER_HPP
#define STEAM_CONTROLLER_HPP

#include <dake/math/matrix.hpp>

extern "C" {
#include <libusb.h>
}

#include <thread>


class SteamController {
    private:
        struct InputData {
            uint8_t always_one;     // 0x00
            uint8_t unknown_0;
            uint8_t validity;       // 0x02
            uint8_t unknown_1;
            uint8_t seqnum;         // 0x04
            uint8_t unknown_2[3];
            uint8_t buttons_0;      // 0x08
            uint16_t buttons_1;     // 0x09
            uint8_t lshoulder;      // 0x0b
            uint8_t rshoulder;      // 0x0c
            uint8_t unknown_3[3];   // 0x0d
            int16_t lpad_x;         // 0x10
            int16_t lpad_y;         // 0x12
            int16_t rpad_x;         // 0x14
            int16_t rpad_y;         // 0x16
            uint8_t unknown_4[40];
        } __attribute__((packed));

        const libusb_endpoint_descriptor *
            set_interface(const libusb_config_descriptor *cfg_desc);

        static void usb_update(SteamController *self);

        void send_rumble(uint8_t index, uint16_t intensity);

        libusb_device *dev = nullptr;
        libusb_device_handle *handle = nullptr;
        int interface = -1, ep = -1;
        bool wireless = false;

        InputData raw_state;

        uint32_t raw_button_state = 0;

        dake::math::vec2 lpad_status = dake::math::vec2::zero();
        dake::math::vec2 rpad_status = dake::math::vec2::zero();
        dake::math::vec2 analog_status = dake::math::vec2::zero();
        float lshoulder_status = 0.f, rshoulder_status = 0.f;

        std::thread *usb_thread = nullptr;
        bool usb_thread_quit = false;

        uint16_t left_rumble = 0, right_rumble = 0;
        int rumble_index = 0;


    public:
        enum Button {
            BOT_RSHOULDER,
            BOT_LSHOULDER,
            TOP_RSHOULDER,
            TOP_LSHOULDER,

            Y,
            B,
            X,
            A,

            UP,
            RIGHT,
            LEFT,
            DOWN,

            PREVIOUS,
            ACTION,
            NEXT,

            LGRIP,
            RGRIP,

            LEFT_PAD,
            RIGHT_PAD,

            LEFT_PAD_TOUCH,
            RIGHT_PAD_TOUCH,

            ANALOG_STICK,

            NONE = -1,
        };

        SteamController(void);
        ~SteamController(void);

        bool lpad_valid(void) const;
        bool rpad_valid(void) const;
        bool analog_valid(void) const;

        const dake::math::vec2 &lpad(void) const { return lpad_status; }
        const dake::math::vec2 &rpad(void) const { return rpad_status; }
        const dake::math::vec2 &analog(void) const { return analog_status; }
        float lshoulder(void) const { return lshoulder_status; }
        float rshoulder(void) const { return rshoulder_status; }

        uint32_t buttons(void) const { return raw_button_state; }

        bool button_state(Button b) const { return buttons() & (1 << b); }

        void set_left_rumble(float intensity)
        { left_rumble = static_cast<uint16_t>(intensity * 65535.f); }

        void set_right_rumble(float intensity)
        { right_rumble = static_cast<uint16_t>(intensity * 65535.f); }
};

#endif
