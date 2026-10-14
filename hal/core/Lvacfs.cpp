/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AHAL_Lvacfs_QTI"

#include <dlfcn.h>
#include <log/log.h>
#include <qti-audio-core/Lvacfs.h>
#include <qti-audio-core/StreamInPrimary.h>
#include <system/audio.h>
#include <unistd.h>

#include <cstdlib>

#ifdef __LP64__
#define VENDOR_LIBS "/vendor/lib64/"
#define ODM_LIBS "/odm/lib64/"
#else
#define VENDOR_LIBS "/vendor/lib/"
#define ODM_LIBS "/odm/lib/"
#endif
#define LVACFS_WRAPPER_LIB_NAME "liblvacfs_wrapper.so"
#define VENDOR_LIB_PATH VENDOR_LIBS LVACFS_WRAPPER_LIB_NAME
#define ODM_LIB_PATH ODM_LIBS LVACFS_WRAPPER_LIB_NAME

#define ODM_PARAMS_DIR_PATH "/odm/etc/lvacfs_params"
#define VENDOR_PARAMS_DIR_PATH "/vendor/etc/lvacfs_params"

#define LOAD_SYMBOL(handle, symbol_ptr, symbol_name)                           \
    do {                                                                       \
        (symbol_ptr) = (decltype(symbol_ptr))dlsym((handle), (symbol_name));   \
        if (!(symbol_ptr)) {                                                   \
            ALOGE("Failed to load symbol '%s': %s", (symbol_name), dlerror()); \
            deinit();                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

using ::aidl::android::media::audio::common::AudioFormatDescription;
using ::aidl::android::media::audio::common::AudioFormatType;
using ::aidl::android::media::audio::common::PcmType;

namespace qti::audio::core {

bool Lvacfs::isLvacfsEnabled() {
    bool isEnabled = false;
    bool* payload_lvacfs = &isEnabled;
    size_t param_size = 0;

    int ret = pal_get_param(PAL_PARAM_ID_LVACFS, (void**)&payload_lvacfs, &param_size, nullptr);

    return (!ret && isEnabled);
}

void Lvacfs::init() {
    if (access(ODM_PARAMS_DIR_PATH, F_OK) == 0) {
        params_file_path_ = ODM_PARAMS_DIR_PATH;
    } else if (access(VENDOR_PARAMS_DIR_PATH, F_OK) == 0) {
        params_file_path_ = VENDOR_PARAMS_DIR_PATH;
    } else {
        ALOGE("No params directory found");
        return;
    }

    wrapper_ops_ = std::make_unique<struct lvacfs_wrapper_ops>();
    if (!wrapper_ops_) {
        ALOGE("Failed to allocate lvacfs_wrapper_ops");
        return;
    }

    wrapper_ops_->lib_handle = dlopen(ODM_LIB_PATH, RTLD_NOW);
    if (!wrapper_ops_->lib_handle) {
        wrapper_ops_->lib_handle = dlopen(VENDOR_LIB_PATH, RTLD_NOW);
        if (!wrapper_ops_->lib_handle) {
            ALOGE("dlopen failed for %s and %s: %s", ODM_LIB_PATH, VENDOR_LIB_PATH, dlerror());
            return;
        }
    }

    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->create_instance,
                "lvacfs_wrapper_CreateLibraryInstance");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->destroy_instance,
                "lvacfs_wrapper_DestroyLibraryInstance");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->process, "lvacfs_wrapper_Process");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->update_zoom_info,
                "lvacfs_wrapper_UpdateZoomInfo");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->update_angle_info,
                "lvacfs_wrapper_UpdateAngleInfo");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_params_file_path,
                "lvacfs_SetParamsFilePath");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_profile, "lvacfs_wrapper_SetProfile");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_audio_direction,
                "lvacfs_wrapper_SetAudioDirection");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_device_orientation,
                "lvacfs_wrapper_SetDeviceOrientation");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->get_versions, "lvacfs_wrapper_GetVersions");

    ALOGI("LVACFS initialized successfully");
}

void Lvacfs::deinit() {
    if (wrapper_ops_ && wrapper_ops_->lib_handle) {
        dlclose(wrapper_ops_->lib_handle);
    }
    wrapper_ops_.reset();
    ALOGI("LVACFS deinitialized");
}

// Values match tinyalsa's pcm_format enum in tinyalsa/asoundlib.h.
static int lvacfs_format_from_audio_format(const AudioFormatDescription& format) {
    if (format.type != AudioFormatType::PCM) return -1;
    switch (format.pcm) {
        case PcmType::INT_16_BIT: return 0;    // PCM_FORMAT_S16_LE
        case PcmType::INT_32_BIT: return 1;    // PCM_FORMAT_S32_LE
        case PcmType::FIXED_Q_8_24: return 3;  // PCM_FORMAT_S24_LE
        case PcmType::INT_24_BIT: return 4;    // PCM_FORMAT_S24_3LE
        default: return -1;                    // PCM_FORMAT_INVALID
    }
}

void Lvacfs::startInputStream(StreamInPrimary* in) {
    const auto& audio_format = in->mMixPortConfig.format.value();
    int format = lvacfs_format_from_audio_format(audio_format);
    if (format < 0) {
        ALOGE("Unsupported audio format: %s", audio_format.toString().c_str());
        return;
    }
    in->mLvacfsHandle = (void**)calloc(1, sizeof(void*));
    if (!in->mLvacfsHandle) {
        ALOGE("Failed to allocate lvacfs_handle");
        return;
    }
    wrapper_ops_->set_params_file_path(params_file_path_);
    int channel_count = getChannelCount(in->mMixPortConfig.channelMask.value());
    uint32_t channels = ((channel_count & 0xFFFF) << 16) | (channel_count & 0xFFFF);
    uint64_t sample_rate_and_format =
            ((uint64_t)format << 32) | in->mMixPortConfig.sampleRate.value().value;
    const auto source = getAudioSource(in->mMixPortConfig);
    int ret = wrapper_ops_->create_instance(
            in->mLvacfsHandle, source ? static_cast<uint32_t>(source.value()) : AUDIO_SOURCE_DEFAULT,
            sample_rate_and_format, channels);
    if (ret < 0) {
        ALOGE("create instance failed: %d", ret);
        stopInputStream(in);
    }
}

void Lvacfs::processInputStream(StreamInPrimary* in, void* buffer, size_t bytes) {
    std::lock_guard<std::mutex> lock(in->mLvacfsLock);
    uint32_t num_frames = bytes / in->mFrameSizeBytes;
    uint8_t status_buffer[0x160] = {0};
    int ret = wrapper_ops_->process(in->mLvacfsHandle, buffer, buffer, num_frames, status_buffer);
    if (ret < 0) {
        ALOGE("process failed: %d", ret);
        stopInputStream(in);
    }
}

void Lvacfs::stopInputStream(StreamInPrimary* in) {
    int ret = wrapper_ops_->destroy_instance(in->mLvacfsHandle);
    if (ret < 0) {
        ALOGE("destroy instance failed: %d", ret);
    }
    free(in->mLvacfsHandle);
    in->mLvacfsHandle = nullptr;
}

} // namespace qti::audio::core
