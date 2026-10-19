/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <LvacfsProfileConfig.h>

namespace lvacfs {

inline int selectProfile(const Capture& capture) {
    // Preserve the legacy interface for devices without a profile selector.
    return capture.source;
}

}  // namespace lvacfs
