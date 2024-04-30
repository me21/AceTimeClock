/*
 * MIT License
 * Copyright (c) 2024 Brian T. Park, Anatoli Arkhipenko, Alexander Zarubkin
 * Based on StmRtcClock
 *
 */

#ifndef ACE_TIME_TEENSY_RTC_CLOCK_H
#define ACE_TIME_TEENSY_RTC_CLOCK_H

#if defined(TEENSYDUINO)

#include <stdint.h>
#include <AceTime.h> // LocalDateTime
#include "../hw/HardwareDateTime.h"
#include "Clock.h"

namespace ace_time {
namespace clock {

/**
 * An implementation of Clock that uses an Teensy RTC chip.
 * It uses Teensy3Clock class to access the built-in RTC.
 * Teensy firmware updater sets RTC to the current datetime of the computer it is running on.
 * In hardware, it's implemented as 64-bit counter of 32 KHz ticks.
 * Theoretically, it allows to use 64 - 15 == 49 bits as seconds counter.
 * But Teensy3Clock.get() returns regular 32-bit Unix time.
 * It's possible to write our own implementation of that class to use the full 49-bit range. Or maybe it's already patched in newer Teensyduino versions.
 */
class TeensyRtcClock: public Clock {
  public:
    /** Constructor. */
    explicit TeensyRtcClock() = default;

    /** Setup does nothing. Defined for API consistency with other Clocks. */
    void setup() {}

    acetime_t getNow() const override {
      time_t unixSeconds = Teensy3Clock.get(); // returns seconds since 1970-01-01 (Unix epoch)
      // detect if RTC has not been set with correct time (e.g. starting without battery or with bad battery)
      //TODO: will stop working after 2038
      if(unixSeconds < 1714521600) // May 1 2024 00:00:00 UTC
        return kInvalidSeconds;
      return unixSeconds - Epoch::secondsToCurrentEpochFromUnixEpoch64();
    }

    void setNow(acetime_t epochSeconds) override {
      if (epochSeconds == kInvalidSeconds) return;

      time_t unixSeconds = epochSeconds + Epoch::secondsToCurrentEpochFromUnixEpoch64();
      Teensy3Clock.set(unixSeconds);
    }
};

} // hw
} // ace_time

#endif // #if defined(ARDUINO_ARCH_STM32) || defined(EPOXY_DUINO)
#endif // #ifndef ACE_TIME_STM_RTC_CLOCK_H
