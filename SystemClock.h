/*
 * MIT License
 * Copyright (c) 2018 Brian T. Park
 */

#ifndef ACE_TIME_SYSTEM_CLOCK_H
#define ACE_TIME_SYSTEM_CLOCK_H

#include <stdint.h>
#include "Clock.h"

extern "C" unsigned long millis();
class SystemClockLoopTest;
class SystemClockLoopTest_syncNow;
class SystemClockCoroutineTest;

namespace ace_time {
namespace clock {

/**
 * A Clock that uses the Arduino millis() function to advance the time
 * returned to the user. It has 2 major features:
 *
 *    1) The built-in millis() is not accurate, so this class allows a periodic
 *    sync using the (presumably) more accurate referenceClock.
 *    2) The current time can be periodically backed up into the backupClock
 *    which is expected to be an RTC chip that continues to keep time during
 *    power loss. Upon (re)start, SystemClock::setup() reads back the time from
 *    the backupClock if it exists.
 *
 * There are 2 maintenance tasks which this class must perform peridicallly:
 *
 *    1) The value of the previous system time millis() is stored internally as
 *    a uint16_t. That has 2 advantages: 1) it saves memory, 2) the upper bound
 *    of the execution time of getNow() limited to 65 iterations. The
 *    disadvantage is the that internal counter will rollover within 65.535
 *    milliseconds. To prevent that, keepAlive() must be called more frequently
 *    than every 65.536 seconds.
 *    2) The current time can be synchronized to the referenceClock peridically.
 *    Some reference clocks can take hundreds or thousands of milliseconds to
 *    return, so it's important that the non-block methods of Clock are
 *    used to synchronize to the reference clock.
 *
 * Two subclasses of SystemClock expose 2 different ways of performing these
 * maintenance tasks.
 *
 *    1) Call SystemClockCoroutine::runCoroutine using the framework of
 *    the AceRoutine library in the global loop() function.
 *    2) Call the SystemClockLoop::loop() method from the global loop()
 *    function.
 */
class SystemClock: public Clock {
  public:

    /** Attempt to retrieve the time from the backupClock if it exists. */
    void setup() {
      if (mBackupClock != nullptr) {
        setNow(mBackupClock->getNow());
      }
    }

    acetime_t getNow() const override {
      if (!mIsInit) return kInvalidSeconds;

      // Update mEpochSeconds by the number of seconds elapsed according to the
      // millis(). This method is expected to be called multiple times a second,
      // so the while() loop below will normally execute 0 times, until the
      // millis() clock goes past the mPrevMillis by 1 second.
      //
      // There are 2 reasons why this method will be called multiple times a
      // second:
      //
      // 1) A physical clock with an external display will want to refresh
      // its display 5-10 times a second, so that it can capture the transition
      // from one second to the next without too much jitter. So it will call
      // this method multiple times a second to check if one second has passed.
      //
      // 2) If the SystemClockCoroutine or SystemClockLoop classes is used,
      // then the keepAlive() method will be called perhaps 100's times per
      // second, as fast as the iteration speed of the global loop() function.
      while ((uint16_t) ((uint16_t) clockMillis() - mPrevMillis) >= 1000) {
        mPrevMillis += 1000;
        mEpochSeconds += 1;
      }

      return mEpochSeconds;
    }

    void setNow(acetime_t epochSeconds) override {
      syncNow(epochSeconds);

      // Also set the reference clock if possible.
      if (mReferenceClock != nullptr) {
        mReferenceClock->setNow(epochSeconds);
      }
    }

    /**
     * Manually force a sync with the referenceClock if it exists. Intended to
     * be mostly for diagnostic or debugging.
     *
     * This calls the synchronous Clock::getNow() method on the reference clock,
     * which can block the program from continuing if the reference clock takes
     * a long time.
     *
     * Normally syncing with the reference clock happens through the
     * SystemClockCoroutine::runCoroutine() or SystemClockLoop::loop(), both of
     * which use the non-blocking calls (Clock::sendRequest(),
     * Clock::isResponseReady(), Clock::readResponse()) on the reference clock.
     */
    void forceSync() {
      if (mReferenceClock) {
        acetime_t nowSeconds = mReferenceClock->getNow();
        syncNow(nowSeconds);
      }
    }

    /**
     * Return the time (seconds since Epoch) of the last valid sync() call.
     * Returns kInvalidSeconds if never synced.
     */
    acetime_t getLastSyncTime() const {
      return mLastSyncTime;
    }

    /**
     * Difference between this clock compared to reference at last sync. A
     * negative value means that the SystemClock was slower than the
     * referenceClock, and a positive value means that the SystemClock was
     * faster than the referenceClock.
     *
     * The clock skew is expected to be very small, a few seconds, so we use
     * `int16_t` type to save memory. The maximum clock skew that can be stored
     * is 32767 seconds, or just over 9 hours.
     */
    int16_t getClockSkew() const {
      return mClockSkew;
    }

    /** Return true if initialized by setNow() or syncNow(). */
    bool isInit() const { return mIsInit; }

  protected:
    friend class ::SystemClockLoopTest;
    friend class ::SystemClockCoroutineTest;
    friend class ::SystemClockLoopTest_syncNow;

    // disable copy constructor and assignment operator
    SystemClock(const SystemClock&) = delete;
    SystemClock& operator=(const SystemClock&) = delete;

    /**
     * Constructor.
     *
     * @param referenceClock The authoritative source of the time. If this is
     *    null, this object relies just on clockMillis() to keep time, and the
     *    user is expected to set the proper time using setNow().
     *
     * @param backupClock An RTC chip which continues to keep time even when
     *    power is lost. If this clock exists, then the time is retrieved from
     *    the backupClock during setup() and used to set the referenceClock
     *    which is assumed to have lost its info. If the reference clock
     *    continues to keep time during power loss, then the backupClock does
     *    not need to be given. One should never need to give the same clock
     *    instance as both the referenceClock and the backupClock, but the code
     *    tries to detect this case and attempts to do the right thing. This
     *    parameter can be null.
     */
    explicit SystemClock(
        Clock* referenceClock /* nullable */,
        Clock* backupClock /* nullable */
    ) :
        mReferenceClock(referenceClock),
        mBackupClock(backupClock) {}

    /**
     * Empty constructor primarily for tests. The init() must be called before
     * using the object.
     */
    explicit SystemClock() {}

    /** Same as constructor but allows delayed initialization, e.g. in tests. */
    void initSystemClock(
        Clock* referenceClock /* nullable */,
        Clock* backupClock /* nullable */
    ) {
      mReferenceClock = referenceClock;
      mBackupClock = backupClock;

      mEpochSeconds = kInvalidSeconds;
      mLastSyncTime = kInvalidSeconds;
      mPrevMillis = 0;
      mIsInit = false;
    }

    /**
     * Return the Arduino millis(). Override for unit testing. Named
     * 'clockMillis()' to avoid conflict with Coroutine::millis().
     */
    virtual unsigned long clockMillis() const { return ::millis(); }

    /**
     * Call this (or getNow() every 65.535 seconds or faster to keep the
     * internal counter in sync with millis(). This will normally happen through
     * the SystemClockCoroutine::runCoroutine() or SystemClockLoop::loop()
     * methods.
     */
    void keepAlive() {
      getNow();
    }

    /**
     * Write the nowSeconds to the backupClock (which can be an RTC that has
     * non-volatile memory). If the referenceClock already preserves its date
     * and time during power loss, then we don't need a backupClock and this
     * method does not need to be called.
     */
    void backupNow(acetime_t nowSeconds) {
      if (mBackupClock != nullptr) {
        mBackupClock->setNow(nowSeconds);
      }
    }

    /**
     * Set the current mEpochSeconds to the given epochSeconds. This method is
     * intended to be used by the SystemClockCoroutine or SystemClockLoop
     * classes to update the current mEpochSeconds using the epochSeconds
     * retrieved from the referenceClock, using the asynchronous methods of the
     * referenceClock to avoid blocking.
     *
     * This method exists because the implementation details of synchronizing
     * the referenceClock to the systemClock is decoupled from this parent class
     * and moved into the subclasses (currently one of SystemClockCoroutine and
     * SystemClockLoop). This method is the hook that allows the subclasses to
     * perform the synchronization.
     *
     * This method is the same as setNow() (in fact, setNow() just calls this
     * method), except that we don't set the referenceClock, since that was the
     * original source of the epochSeconds. If we saved it back to its source,
     * we would probably see drifting of the referenceClock due to the 1-second
     * granularity of many RTC clocks.
     *
     * TODO: Implement a more graceful syncNow() algorithm which shifts only a
     * few milliseconds per iteration, and which guarantees that the clock
     * never goes backwards in time.
     */
    void syncNow(acetime_t epochSeconds) {
      if (epochSeconds == kInvalidSeconds) return;

      mLastSyncTime = epochSeconds;
      acetime_t skew = mEpochSeconds - epochSeconds;
      mClockSkew = skew;
      if (skew == 0) return;

      mEpochSeconds = epochSeconds;
      mPrevMillis = clockMillis();
      mIsInit = true;

      if (mBackupClock != mReferenceClock) {
        backupNow(epochSeconds);
      }
    }

  protected:
    Clock* mReferenceClock;
    Clock* mBackupClock;

    mutable acetime_t mEpochSeconds = kInvalidSeconds;
    acetime_t mLastSyncTime = kInvalidSeconds; // time when last synced
    mutable uint16_t mPrevMillis = 0; // lower 16-bits of clockMillis()
    int16_t mClockSkew = 0; // diff between reference and this clock
    bool mIsInit = false; // true if setNow() or syncNow() was successful
};

}
}

#endif
