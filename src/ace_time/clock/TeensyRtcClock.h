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
 * An implementation of Clock that uses an Teensy RTC chip using the
 * [STM32RTC](https://github.com/stm32duino/STM32RTC) library. The STM32RTC
 * singleton object should be configured using `STM32RTC::getInstance()` in the
 * global `setup()` function.
 *
 * AceTime v2 allows the epoch year of the library to be adjustable by the
 * client application, from the year 2000 until the year 10000. Unfortunately,
 * the STM32 hardware supported by this class (through the dependent STM32RTC
 * library) uses a 2-digit year offset from the year 2000. This will break in
 * the year 2100 unless a software fix can be created to work around that
 * limitation.
 *
 * Ironically, the STM32F1 chip has a more limited RTC functionality in the
 * hardware but that limitation actually works to its advantage so that the F1
 * can be fully compatible with AceTime to its limit of the year 10000 if
 * Stm32F1Clock class is used instead of this class. (See notes in
 * Stm32F1Clock.h).
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
