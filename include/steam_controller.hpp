#ifndef STEAM_CONTROLLER_HPP
#define STEAM_CONTROLLER_HPP

#include <dake/math/fmatrix.hpp>

extern "C" {
#include <hidapi.h>
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
            uint16_t buttons_0;     // 0x08
            uint8_t buttons_1;      // 0x0a
            uint8_t lshoulder;      // 0x0b
            uint8_t rshoulder;      // 0x0c
            uint8_t unknown_3[3];   // 0x0d
            int16_t lpad_x;         // 0x10
            int16_t lpad_y;         // 0x12
            int16_t rpad_x;         // 0x14
            int16_t rpad_y;         // 0x16
            uint8_t unknown_4[10];
            int16_t gyro_x;         // 0x22
            int16_t gyro_z;         // 0x24
            int16_t gyro_y;         // 0x26
            uint8_t unknown_5[24];
        } __attribute__((packed));

        // Use git blame to read the exciting story behind this!
        // (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52991)
        static_assert(sizeof(InputData) == 64, "InputData has invalid size");

        bool enumerate(void);

        static void raw_update(SteamController *self);

        void send_rumble(uint8_t index, uint16_t intensity);

        hid_device *dev = nullptr;
        int interface = -1, ep = -1;
        bool wireless = false;

        InputData raw_state;

        uint32_t raw_button_state = 0;

        dake::math::fvec2 lpad_status = dake::math::fvec2::zero();
        dake::math::fvec2 rpad_status = dake::math::fvec2::zero();
        dake::math::fvec2 analog_status = dake::math::fvec2::zero();
        dake::math::fvec3 gyro_status = dake::math::vec3::zero();
        float lshoulder_status = 0.f, rshoulder_status = 0.f;

        std::thread *update_thread = nullptr;
        bool update_thread_quit = false;

        uint16_t left_rumble = 0, right_rumble = 0;
        bool left_rumble_autoclear = false, right_rumble_autoclear = false;
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

        const dake::math::fvec2 &lpad(void) const { return lpad_status; }
        const dake::math::fvec2 &rpad(void) const { return rpad_status; }
        const dake::math::fvec2 &analog(void) const { return analog_status; }
        float lshoulder(void) const { return lshoulder_status; }
        float rshoulder(void) const { return rshoulder_status; }

        uint32_t buttons(void) const { return raw_button_state; }

        const dake::math::fvec3 &gyro(void) const { return gyro_status; }

        bool button_state(Button b) const { return buttons() & (1 << b); }

        void set_left_rumble(float intensity, bool autoclear = false)
        { left_rumble = static_cast<uint16_t>(intensity * 65535.f);
          left_rumble_autoclear = autoclear; }

        void set_right_rumble(float intensity, bool autoclear = false)
        { right_rumble = static_cast<uint16_t>(intensity * 65535.f);
          right_rumble_autoclear = autoclear; }
};

#endif
