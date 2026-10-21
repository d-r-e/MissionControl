/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <ostream>
namespace stadia_host {
int RunNativeHid(unsigned seconds, std::ostream &out, std::ostream &error);
}
