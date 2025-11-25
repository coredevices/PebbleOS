/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef _JERRYSCRIPT_MBED_EVENT_LOOP_BOUND_CALLBACK_H
#define _JERRYSCRIPT_MBED_EVENT_LOOP_BOUND_CALLBACK_H

#include "Callback.h"

namespace mbed {
namespace js {

template<typename T>
class BoundCallback;

template<typename R, typename A0>
class BoundCallback<R(A0)> {
 public:
    BoundCallback(Callback<R(A0)> cb, A0 a0) : a0(a0), cb(cb) { }

    void call() {
        cb(a0);
    }

    operator Callback<void()>() {
        Callback<void()> cb(this, &BoundCallback::call);
        return cb;
    }

 private:
    A0 a0;
    Callback<R(A0)> cb;
};

template<typename R, typename A0, typename A1>
class BoundCallback<R(A0, A1)> {
 public:
    BoundCallback(Callback<R(A0, A1)> cb, A0 a0, A1 a1) : a0(a0), a1(a1), cb(cb) { }

    void call() {
        cb(a0, a1);
    }

    operator Callback<void()>() {
        Callback<void()> cb(this, &BoundCallback::call);
        return cb;
    }

 private:
    A0 a0;
    A0 a1;

    Callback<R(A0, A1)> cb;
};

template<typename R, typename A0, typename A1, typename A2>
class BoundCallback<R(A0, A1, A2)> {
 public:
    BoundCallback(Callback<R(A0, A1, A2)> cb, A0 a0, A1 a1, A2 a2) : a0(a0), a1(a1), a2(a2), cb(cb) { }

    void call() {
        cb(a0, a1, a2);
    }

    operator Callback<void()>() {
        Callback<void()> cb(this, &BoundCallback::call);
        return cb;
    }

 private:
    A0 a0;
    A1 a1;
    A2 a2;

    Callback<R(A0, A1, A2)> cb;
};

template<typename R, typename A0, typename A1, typename A2, typename A3>
class BoundCallback<R(A0, A1, A2, A3)> {
 public:
    BoundCallback(Callback<R(A0, A1, A2, A3)> cb, A0 a0, A1 a1, A2 a2, A3 a3) : a0(a0), a1(a1), a2(a2), a3(a3), cb(cb) { }

    void call() {
        cb(a0, a1, a2, a3);
    }

    operator Callback<void()>() {
        Callback<void()> cb(this, &BoundCallback::call);
        return cb;
    }

 private:
    A0 a0;
    A1 a1;
    A2 a2;
    A3 a3;

    Callback<R(A0, A1, A2, A3)> cb;
};

}  // namespace js
}  // namespace mbed

#endif  // _JERRYSCRIPT_MBED_EVENT_LOOP_BOUND_CALLBACK_H
