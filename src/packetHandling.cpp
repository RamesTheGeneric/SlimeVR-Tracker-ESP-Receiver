#include "packetHandling.h"

PacketHandling &PacketHandling::getInstance() {
    return instance;
}

void PacketHandling::insert(
        const uint8_t data[ESPNowCommunication::packetSizeBytes]) {
    Packet packet;
    memcpy(packet.data, data, sizeof(packet.data));
    buffer.push(packet);
}

void PacketHandling::tick(HIDDevice &hidDevice) {
    if (buffer.isEmpty() || !hidDevice.ready()) {
        return;
    }

    const auto packetsToHandle =
            std::min(static_cast<size_t>(buffer.size()), maxPacketsPerTick);

    // Always emit a full-size HID report. Windows' HID stack delivers input
    // reports padded/clamped to the descriptor's declared length and drops or
    // mangles short reports, so variable-length reports never reach the SlimeVR
    // server on Windows even though they pass through on Linux/Android. Pad any
    // unused slots with a sentinel the server's parser skips: packetType != 0xff
    // (not treated as a device-register packet) and device id 0xff (an
    // unregistered id, so DesktopHIDManager bails before processing the slot).
    Packet sendArray[maxPacketsPerTick];
    memset(sendArray, 0, sizeof(sendArray));
    for (auto i = 0u; i < packetsToHandle; i++) {
        sendArray[i] = buffer.shift();
    }
    for (auto i = packetsToHandle; i < maxPacketsPerTick; i++) {
        sendArray[i].data[0] = 0xfe;
        sendArray[i].data[1] = 0xff;
    }

    hidDevice.send(reinterpret_cast<uint8_t *>(sendArray), sizeof(sendArray));
}

PacketHandling PacketHandling::instance;
