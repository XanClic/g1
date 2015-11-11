extern "C" {
#include <hidapi.h>
}

#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

#include "steam_controller.hpp"


SteamController::SteamController(void)
{
    if (!enumerate()) {
        wireless = true;
        if (!enumerate()) {
            throw std::runtime_error("No controller found");
        }
    }


    // No error handling here, because, well, sometimes errors do occur here,
    // but ignoring them is apparently completely fine (??!??!)
    static uint8_t init_0[65] = { 0, 0x83 };
    hid_send_feature_report(dev, init_0, sizeof(init_0));

    update_thread = new std::thread(&SteamController::raw_update, this);

    uint8_t received[65];
    hid_get_feature_report(dev, received, sizeof(received));

    received[0] = 0;
    received[1] = 0xae;
    received[2] = 0x15;
    received[3] = 0x01;
    memset(received + 24, 0, 65 - 24);
    hid_send_feature_report(dev, received, sizeof(received));

    hid_get_feature_report(dev, received, sizeof(received));

    static uint8_t init_1[65] = { 0, 0x81 };
    hid_send_feature_report(dev, init_1, sizeof(init_1));

    static uint8_t init_2[65] = {
        0,
        0x87, 0x15, 0x32, 0x84, 0x03, 0x18, 0x00, 0x00,
        0x31, 0x02, 0x00, 0x08, 0x07, 0x00, 0x07, 0x07,
        0x00, 0x30, 0x00, 0x00, 0x2f, 0x01, 0x00, 0x00
    };
    hid_send_feature_report(dev, init_2, sizeof(init_2));
}


SteamController::~SteamController(void)
{
    update_thread_quit = true;
    update_thread->join();
    delete update_thread;

    hid_close(dev);
}


bool SteamController::enumerate(void)
{
    hid_device_info *info, *found;

    info = hid_enumerate(0x28de, wireless ? 0x1142 : 0x1102);
    for (found = info; found; found = found->next) {
        // For wireless mode, I suppose any of the interface 1 through 4
        // signifies one of the paired controllers; we don't care about more
        // than one, so just use 1.
        if (found->interface_number == (wireless ? 1 : 2)) {
            break;
        }
    }

    if (found) {
        dev = hid_open_path(found->path);
    }

    hid_free_enumeration(info);

    return dev;
}


void SteamController::send_rumble(uint8_t index, uint16_t intensity)
{
    uint8_t cmd[65] = {
        0,
        0x8f, 0x07, index,
        static_cast<uint8_t>(intensity & 0xff),
        static_cast<uint8_t>(intensity >> 8),
        0x00, 0x00, 0x01
    };

    hid_send_feature_report(dev, cmd, sizeof(cmd));
}


void SteamController::raw_update(SteamController *self)
{
    while (!self->update_thread_quit) {
        unsigned char *data =
            reinterpret_cast<unsigned char *>(&self->raw_state);

        int ret = hid_read_timeout(self->dev, data, 64, 1000);
        if (ret < 64 && ret >= 0) {
            continue;
        } else if (ret < 0) {
            break;
        }

        if (self->raw_state.validity != 0x01) {
            continue;
        }

        self->raw_button_state =  self->raw_state.buttons_0
                               | (self->raw_state.buttons_1 << 8);

        if (self->analog_valid() && self->button_state(LEFT_PAD)) {
            self->raw_button_state &= ~(1 << LEFT_PAD);
            self->raw_button_state |= 1 << ANALOG_STICK;
        }

        if (self->lpad_valid()) {
            self->lpad_status.x() = self->raw_state.lpad_x / 32767.f;
            self->lpad_status.y() = self->raw_state.lpad_y / 32767.f;
        } else {
            self->lpad_status.x() = 0.f;
            self->lpad_status.y() = 0.f;
        }

        if (self->rpad_valid()) {
            self->rpad_status.x() = self->raw_state.rpad_x / 32767.f;
            self->rpad_status.y() = self->raw_state.rpad_y / 32767.f;
        } else {
            self->rpad_status.x() = 0.f;
            self->rpad_status.y() = 0.f;
        }

        if (self->analog_valid()) {
            self->analog_status.x() = self->raw_state.lpad_x / 32767.f;
            self->analog_status.y() = self->raw_state.lpad_y / 32767.f;
        } else {
            self->analog_status.x() = 0.f;
            self->analog_status.y() = 0.f;
        }

        self->lshoulder_status = self->raw_state.lshoulder / 255.f;
        self->rshoulder_status = self->raw_state.rshoulder / 255.f;

        if (self->left_rumble && self->right_rumble) {
            if (self->rumble_index) {
                self->send_rumble(1, self->left_rumble);
            } else {
                self->send_rumble(0, self->right_rumble);
            }
            self->rumble_index ^= 1;
        } else if (self->right_rumble) {
            self->send_rumble(0, self->right_rumble);
        } else if (self->left_rumble) {
            self->send_rumble(1, self->left_rumble);
        }

        if (self->left_rumble_autoclear) {
            self->left_rumble = 0;
            self->left_rumble_autoclear = false;
        }
        if (self->right_rumble_autoclear) {
            self->right_rumble = 0;
            self->right_rumble_autoclear = false;
        }
    }
}


bool SteamController::lpad_valid(void) const
{
    return raw_button_state & (1 << LEFT_PAD_TOUCH);
}


bool SteamController::rpad_valid(void) const
{
    return raw_button_state & (1 << RIGHT_PAD_TOUCH);
}


bool SteamController::analog_valid(void) const
{
    return !lpad_valid();
}
