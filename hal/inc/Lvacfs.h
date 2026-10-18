/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mutex>

#include "LvacfsProfileConfig.h"
#include "PalApi.h"
#include "PalDefs.h"

class StreamInPrimary;
struct str_parms;

struct lvacfs_wrapper_ops {
    void* lib_handle;
    int (*create_instance)(void**, uint32_t, uint64_t, uint32_t);
    int (*destroy_instance)(void**);
    int (*process)(void**, void*, void*, size_t, void*);
    int (*update_zoom_info)(void**, float, int32_t);
    int (*update_angle_info)(void**, int32_t);
    void (*set_params_file_path)(const char*);
    int (*set_profile)(void**, uint32_t);
    int (*set_audio_direction)(void**, uint32_t);
    int (*set_device_orientation)(void**, uint32_t);
    void (*get_versions)(char*);
};

class Lvacfs {
  public:
    static Lvacfs& getInstance() {
        static Lvacfs instance;
        return instance;
    }

    static bool isLvacfsEnabled();
    void init();
    void deinit();
    void setParameters(struct str_parms* parms);
    void startInputStream(StreamInPrimary* in);
    void processInputStream(StreamInPrimary* in, void* buffer, size_t bytes);
    void stopInputStream(StreamInPrimary* in);

    const struct lvacfs_wrapper_ops* getWrapperOps() const { return wrapper_ops_.get(); }

  private:
    Lvacfs() : wrapper_ops_(nullptr), params_file_path_(nullptr) {}

    Lvacfs(const Lvacfs&) = delete;
    Lvacfs& operator=(const Lvacfs&) = delete;

    int getProfile(StreamInPrimary* in) const;
    void destroyInstance(StreamInPrimary* in);

    std::unique_ptr<struct lvacfs_wrapper_ops> wrapper_ops_;
    const char* params_file_path_;
    // The wrapper shares its params path between instances.
    std::mutex lock_;
    lvacfs::Scene scene_ = lvacfs::Scene::DEFAULT;
};
