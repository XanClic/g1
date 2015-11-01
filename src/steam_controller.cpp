extern "C" {
#include <libusb.h>
}

#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

#include "steam_controller.hpp"


static libusb_context *libusb_ctx;


static void destroy_libusb_context(void)
{
    if (libusb_ctx) {
        libusb_exit(libusb_ctx);
    }
}


SteamController::SteamController(void)
{
    if (!libusb_ctx) {
        int ret = libusb_init(&libusb_ctx);
        if (ret < 0) {
            throw std::runtime_error("Failed to initialize libusb: " +
                                     std::string(libusb_error_name(ret)));
        }

        atexit(destroy_libusb_context);
    }

    // libusb tells us to never use this is a serious program. Good thing this
    // isn't a serious program.
    handle = libusb_open_device_with_vid_pid(libusb_ctx, 0x28de, 0x1102);
    if (!handle) {
        handle = libusb_open_device_with_vid_pid(libusb_ctx, 0x28de, 0x1142);
        wireless = true;
    }

    if (!handle) {
        throw std::runtime_error("Did not find any controller");
    }

    dev = libusb_get_device(handle);


    try {
        libusb_device_descriptor dev_desc;
        libusb_get_device_descriptor(dev, &dev_desc);

        if (dev_desc.bNumConfigurations != 1) {
            throw std::runtime_error("Unexpected controller configuration");
        }

        libusb_config_descriptor *cfg_desc;
        int ret = libusb_get_config_descriptor(dev, 0, &cfg_desc);
        if (ret < 0) {
            throw std::runtime_error("Failed to read controller configuration: "
                                     + std::string(libusb_error_name(ret)));
        }

        try {
            int curcfg;
            ret = libusb_get_configuration(handle, &curcfg);
            if (ret < 0) {
                throw std::runtime_error("Failed to get current controller "
                                         "configuration: " +
                                         std::string(libusb_error_name(ret)));
            }

            // Just skip the HID descriptors. They'll tell us nothing but "you
            // get 64 bytes of opaque data per request!" anyway.
            const libusb_endpoint_descriptor *ep_desc = set_interface(cfg_desc);

            if (ep_desc->wMaxPacketSize != 64 ||
                ep_desc->bEndpointAddress <= 0x80 ||
                ep_desc->bmAttributes != LIBUSB_TRANSFER_TYPE_INTERRUPT)
            {
                throw std::runtime_error("Unexpected USB endpoint "
                                         "configuration");
            }

            ep = ep_desc->bEndpointAddress;
        } catch (...) {
            libusb_free_config_descriptor(cfg_desc);
            throw;
        }

        libusb_free_config_descriptor(cfg_desc);
    } catch (...) {
        libusb_close(handle);
        throw;
    }


    usb_thread = new std::thread(&SteamController::usb_update, this);


    // No error handling here, because, well, sometimes errors do occur here,
    // but ignoring them is apparently completely fine (??!??!)

    static uint8_t init_0[64] = { 0x83 };
    libusb_control_transfer(handle, 0x21, 0x09, 0x0300, wireless ? 1 : 2,
                            init_0, 64, 0);

    uint8_t received[64];
    libusb_control_transfer(handle, 0xa1, 0x01, 0x0300, wireless ? 1 : 2,
                            received, 64, 0);

    received[0] = 0xae;
    received[1] = 0x15;
    received[2] = 0x01;
    memset(received + 23, 0, 64 - 23);
    libusb_control_transfer(handle, 0x21, 0x09, 0x0300, wireless ? 1 : 2,
                            received, 64, 0);

    libusb_control_transfer(handle, 0xa1, 0x01, 0x0300, wireless ? 1 : 2,
                            received, 64, 0);

    static uint8_t init_1[64] = { 0x81 };
    libusb_control_transfer(handle, 0x21, 0x09, 0x0300, wireless ? 1 : 2,
                            init_1, 64, 0);

    static uint8_t init_2[64] = {
        0x87, 0x15, 0x32, 0x84, 0x03, 0x18, 0x00, 0x00,
        0x31, 0x02, 0x00, 0x08, 0x07, 0x00, 0x07, 0x07,
        0x00, 0x30, 0x00, 0x00, 0x2f, 0x01, 0x00, 0x00
    };
    libusb_control_transfer(handle, 0x21, 0x09, 0x0300, wireless ? 1 : 2,
                            init_2, 64, 0);
}


SteamController::~SteamController(void)
{
    usb_thread_quit = true;
    usb_thread->join();
    delete usb_thread;

    libusb_release_interface(handle, interface);
    libusb_close(handle);
}


const libusb_endpoint_descriptor *
    SteamController::set_interface(const libusb_config_descriptor *cfg_desc)
{
    const libusb_interface_descriptor *ifc = nullptr;
    // I'd really love to use the auto-detach feature, but for me it somehow has
    // the opposite effect. While we can indeed claim the interface with
    // auto-detach enabled, somehow the kernel driver gets active in that
    // instant and sends emulated mouse and keyboard input to g1. So just do it
    // the hard way.
    for (int i = 0; i < cfg_desc->bNumInterfaces; i++) {
        for (int j = 0; j < cfg_desc->interface[i].num_altsetting; j++) {
            ifc = &cfg_desc->interface[i].altsetting[j];
            if (libusb_kernel_driver_active(handle, ifc->bInterfaceNumber)) {
                int ret = libusb_detach_kernel_driver(handle,
                                                      ifc->bInterfaceNumber);
                if (ret) {
                    throw std::runtime_error("Cannot detach kernel driver from "
                                             "device");
                }
            }
        }
    }

    ifc = nullptr;
    for (int i = 0; i < cfg_desc->bNumInterfaces; i++) {
        for (int j = 0; j < cfg_desc->interface[i].num_altsetting; j++) {
            ifc = &cfg_desc->interface[i].altsetting[j];

            if (ifc->bInterfaceClass == 3 &&
                ifc->bInterfaceSubClass == 0 &&
                ifc->bInterfaceProtocol == 0)
            {
                break;
            }

            ifc = nullptr;
        }

        if (ifc) {
            break;
        }
    }

    if (!ifc) {
        throw std::runtime_error("Failed to find controller USB interface");
    }

    if (ifc->bNumEndpoints != 1) {
        throw std::runtime_error("Unexpected controller USB interface");
    }


    interface = ifc->bInterfaceNumber;

    int ret = libusb_claim_interface(handle, interface);
    if (ret < 0) {
        throw std::runtime_error("Failed to claim USB interface: " +
                                 std::string(libusb_error_name(ret)));
    }

    ret = libusb_set_interface_alt_setting(handle, interface,
                                           ifc->bAlternateSetting);
    if (ret < 0) {
        libusb_release_interface(handle, interface);
        throw std::runtime_error("Failed to set USB interface: " +
                                 std::string(libusb_error_name(ret)));
    }

    return ifc->endpoint;
}


void SteamController::send_rumble(uint8_t index, uint16_t intensity)
{
    uint8_t cmd[64] = {
        0x8f, 0x07, index,
        static_cast<uint8_t>(intensity & 0xff),
        static_cast<uint8_t>(intensity >> 8),
        0x00, 0x00, 0x01
    };

    libusb_control_transfer(handle, 0x21, 0x09, 0x0300, wireless ? 1 : 2,
                            cmd, 64, 0);
}


void SteamController::usb_update(SteamController *self)
{
    while (!self->usb_thread_quit) {
        int act_len;
        unsigned char *data =
            reinterpret_cast<unsigned char *>(&self->raw_state);

        int ret = libusb_interrupt_transfer(self->handle, self->ep, data, 64,
                                            &act_len, 1000);
        if (ret == LIBUSB_ERROR_TIMEOUT || act_len < 64) {
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
