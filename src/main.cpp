#include "HID.h"
#include "button.h"
#include "configuration.h"
#include "error_codes.h"
#include "espnow/espnow.h"
#include "led.h"
#include "packetHandling.h"
#include "logging/Logger.h"

#include <Arduino.h>
#include <USB.h>
#include <USBCDC.h>
#include <map>


/*
 * Todo:
 * [X] Connect to the PC through HID
 * [X] Enter pairing mode when long pressing button
 * - [ ] In pairing mode listen for pair requests and send out mac address
 * - [ ] Count the number of paired devices
 * [ ] Listen for incoming data packets and store them in a circular buffer
 * [ ] Send out data periodically to the PC
 */

HIDDevice hidDevice;
Button &button = Button::getInstance();
ESPNowCommunication &espnow = ESPNowCommunication::getInstance();
LED led;
SlimeVR::Logging::Logger logger("Main");

[[noreturn]] [[noreturn]] void fail(ErrorCodes errorCode) {
    led.displayError(errorCode);
}

void debugPacket(const uint8_t packet[ESPNowCommunication::packetSizeBytes]) {
    Serial.print("New packet: ");
    for(int i = 0; i < ESPNowCommunication::packetSizeBytes; ++i) {
        Serial.printf("%02x ", packet[i]);
    }
    Serial.println();
}

void setup() { 
    Serial.begin(115200);
    Serial.println("Starting up " USB_PRODUCT "...");
    Configuration::getInstance().setup();
    hidDevice.begin();
    USB.begin();

    button.begin();

    button.onLongPress([]() {
        if (!espnow.isInPairingMode()) {
            Serial.println("Pairing mode enabled");
            espnow.enterPairingMode();
            led.sendContinuousBlinks(0.1f, 0.5f);
        } else {
            Serial.println("Pairing mode disabled");
            espnow.exitPairingMode();
            led.stopBlinking();
        }
    });
    button.onMultiPress([](size_t pressCount) {
        if (pressCount == 5) {
            Serial.println("Trackers reset");
            Configuration::getInstance().setSavedTrackerCount(0);
            led.sendBlinks(5, 0.2f, 0.1f);
            return;
        }
    });

    led.begin();
    led.setState(false);

    ErrorCodes result = espnow.begin();
    if (result != ErrorCodes::NO_ERROR) {
        fail(result);
    }

    espnow.onTrackerPaired([&]() { 
        Serial.println("New tracker paired");
        led.sendBlinks(3, 0.1f);
        });
    espnow.onTrackerConnected(
        [&](uint8_t trackerId, const uint8_t *trackerMacAddress) {
            Serial.println("New tracker connected");
            led.sendBlinks(2, 0.1f);
            uint8_t packet[16];
            packet[0] = 0xff;
            packet[1] = trackerId << 2;
            memcpy(&packet[2], trackerMacAddress, sizeof(uint8_t) * 6);
            memset(&packet[8], 0, sizeof(uint8_t) * 8);
            PacketHandling::getInstance().insert(packet);
        });

    espnow.onPacketReceived(
            [&](const uint8_t packet[ESPNowCommunication::packetSizeBytes]) {
                // Debug: Show packet info
                uint8_t packetType = packet[0];
                uint8_t sensorId = packet[1];
                uint8_t trackerId = sensorId >> 2;
                uint8_t sensorIndex = sensorId & 0x03;
                const char* typeStr = (packetType == 0) ? "DeviceInfo" : 
                                     (packetType == 1) ? "FullSizeFusion" : "Unknown";
                Serial.printf("Packet: type=%s(%d), sensorId=%d, trackerId=%d, sensorIdx=%d\n",
                    typeStr, packetType, sensorId, trackerId, sensorIndex);
                PacketHandling::getInstance().insert(packet);
            });

    // New callback with MAC for extension registration
    static std::map<uint8_t, uint8_t> registeredExtensions;
    espnow.onPacketReceivedWithMac(
            [&](const uint8_t packet[ESPNowCommunication::packetSizeBytes], const uint8_t *mac) {
                uint8_t sensorId = packet[1];
                uint8_t trackerId = sensorId >> 2;
                uint8_t sensorIndex = sensorId & 0x03;
                
                // Send registration packet for extension (sensorIndex=1) when we see DeviceInfo (packetType=0)
                if (sensorIndex == 1 && packet[0] == 0) {
                    // Check if we already registered this extension
                    uint8_t key = (trackerId << 1) | sensorIndex;
                    if (registeredExtensions.find(key) == registeredExtensions.end()) {
                        registeredExtensions[key] = 1;
                        Serial.printf("Registering extension for tracker %d, sensorIdx %d\n", trackerId, sensorIndex);
                        uint8_t regPacket[16];
                        regPacket[0] = 0xff;
                        regPacket[1] = sensorId;  // Use sensorId directly so server knows it's extension
                        memcpy(&regPacket[2], mac, 6);
                        memset(&regPacket[8], 0, 8);
                        PacketHandling::getInstance().insert(regPacket);
                    }
                }
                PacketHandling::getInstance().insert(packet);
            });
    Serial.println("Boot complete");
}

void loop() {
    button.update();
    led.update();

    PacketHandling::getInstance().tick(hidDevice);
}
