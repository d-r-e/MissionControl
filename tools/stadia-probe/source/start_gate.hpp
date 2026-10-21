/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
namespace missioncontrol::probe {
    // A held while launching hbmenu must not start a hardware experiment.
    class StartGate {
        bool released = false;
        bool consumed = false;
        public:
            constexpr bool Update(bool held, bool pressed) {
                if (!held) released = true;
                if (consumed || !released || !held || !pressed) return false;
                consumed = true;
                return true;
            }
    };
}
