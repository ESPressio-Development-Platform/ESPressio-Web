#pragma once

#define ESPRESSIO_WEB_VERSION_MAJOR 0
#define ESPRESSIO_WEB_VERSION_MINOR 1
#define ESPRESSIO_WEB_VERSION_PATCH 0

namespace ESPressio::Web {

struct Version final {
    static constexpr int Major = ESPRESSIO_WEB_VERSION_MAJOR;
    static constexpr int Minor = ESPRESSIO_WEB_VERSION_MINOR;
    static constexpr int Patch = ESPRESSIO_WEB_VERSION_PATCH;
};

}
