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
            // Apparently always one (maybe the status report ID?)
            uint8_t always_one;     // 0x00
            uint8_t unknown_0;

            // Status of the device
            uint16_t status;

            // A sequence number
            uint16_t seqnum;        // 0x04

            uint8_t unknown_2[2];

            // Bitmask of all buttons
            uint16_t buttons_0;     // 0x08
            uint8_t buttons_1;      // 0x0a

            // Analog state of the shoulder triggers
            uint8_t lshoulder;      // 0x0b
            uint8_t rshoulder;      // 0x0c

            uint8_t unknown_3[3];   // 0x0d

            // Position of a finger on the left touch pad OR
            // position of the analog stick
            int16_t lpad_x;         // 0x10
            int16_t lpad_y;         // 0x12

            // Position of a finger on the right touch pad
            int16_t rpad_x;         // 0x14
            int16_t rpad_y;         // 0x16

            uint8_t unknown_4[4];

            // Linear acceleration
            int16_t acceleration_x; // 0x1c
            int16_t acceleration_z; // 0x1e
            int16_t acceleration_y; // 0x20

            // Rotational velocity
            int16_t rotation_x;     // 0x22
            int16_t rotation_z;     // 0x24
            int16_t rotation_y;     // 0x26

            // Orientation in space
            int16_t orientation_ya; // 0x28
            int16_t orientation_x;  // 0x2a
            int16_t orientation_z;  // 0x2c
            int16_t orientation_yb; // 0x2e

            uint8_t unknown_5[16];
        } __attribute__((packed));

        // Use git blame to read the exciting story behind this!
        // (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52991)
        static_assert(sizeof(InputData) == 64, "InputData has invalid size");

        enum DeviceStatus {
            STATUS_SLEEPING = 0x0b04,
            STATUS_VALID    = 0x3c01,
        };

        struct HIDFeatureReport {
            uint8_t report_id;
        } __attribute__((packed));

        struct ConfigureInputReport {
            HIDFeatureReport hfr;
            uint8_t command;        // 0x00
            uint8_t unknown_0[4];
            uint8_t preprocess;     // 0x05
            uint8_t unknown_1[12];
            uint8_t input_mask;     // 0x12
            uint8_t unknown_2[3];

            uint8_t padding[42];

            operator const uint8_t *(void) const
            { return reinterpret_cast<const uint8_t *>(this); }
        } __attribute__((packed));

        static_assert(sizeof(ConfigureInputReport) == 1 + 64,
                      "ConfigureInputReport has invalid size");

        enum CIRPreprocess {
            PREPROCESS_BASE = 0x08,

            PREPROCESS_RAW  = 0x10, // Do not smoothen the touchpad values
        };

        enum CIRInputMask {
            INPUT_MASK_ORIENTATION  = 0x04,
            INPUT_MASK_ACCELERATION = 0x08,
            INPUT_MASK_ROTATION     = 0x10,
        };


        bool enumerate(void);

        static void raw_update(SteamController *self);

        void send_rumble(uint8_t index, uint16_t intensity, uint16_t period = 0,
                         uint16_t count = 1);

        hid_device *dev = nullptr;
        int interface = -1, ep = -1;
        bool wireless = false;

        InputData raw_state;

        uint32_t raw_button_state = 0;

        dake::math::fvec2 lpad_status = dake::math::fvec2::zero();
        dake::math::fvec2 rpad_status = dake::math::fvec2::zero();
        dake::math::fvec2 analog_status = dake::math::fvec2::zero();
        float lshoulder_status = 0.f, rshoulder_status = 0.f;

        dake::math::fvec3 acceleration_data = dake::math::fvec3::zero();
        dake::math::fvec3 rotation_data     = dake::math::fvec3::zero();
        dake::math::fvec3 orientation_data  = dake::math::fvec3::zero();

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

        // Unit: g (== 9.81 m/s²)
        const dake::math::fvec3 &acceleration(void) const
        { return acceleration_data; }
        // Unit: I have no idea
        const dake::math::fvec3 &rotation(void) const { return rotation_data; }
        // Unit: radian
        const dake::math::fvec3 &orientation(void) const
        { return orientation_data; }

        bool button_state(Button b) const { return buttons() & (1 << b); }

        void set_left_rumble(float intensity, bool autoclear = false)
        { left_rumble = static_cast<uint16_t>(intensity * 65535.f);
          left_rumble_autoclear = autoclear; }

        void set_right_rumble(float intensity, bool autoclear = false)
        { right_rumble = static_cast<uint16_t>(intensity * 65535.f);
          right_rumble_autoclear = autoclear; }
};

#endif
