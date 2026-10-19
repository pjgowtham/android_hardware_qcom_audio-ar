# Device profile selection

LVACFS profile IDs are vendor configuration keys, not Android audio sources.
The same ID can name different tuning on different devices. Vendor configuration
files and libraries do not need to be modified to provide a device selector.

## Device header

Provide `LvacfsDeviceProfiles.h` in a vendor `cc_library_headers` module. The
header includes `LvacfsProfileConfig.h` and defines:

```cpp
namespace lvacfs {
inline int selectProfile(const Capture& capture) {
    // Select a vendor profile using the device's stock HAL behavior.
    // Return -1 for capture configurations that should bypass LVACFS.
}
}
```

`Capture` contains the Android source and input device, sample rate, channel
count, and recording scene. Selection rules belong to the device header because
some vendor HALs select by source while others select by rate and channel count.

Select the module in the device's `BoardConfig.mk`:

```make
$(call soong_config_set,qtiaudio,lvacfs_headers,//device/vendor/device:liblvacfs_device_headers)
```

The module exports the directory containing `LvacfsDeviceProfiles.h` and any
generated constants. The HAL's local include directory supplies
`LvacfsProfileConfig.h`; do not copy it into the device tree.

Without `lvacfs_headers`, the default selector preserves the original source-ID
interface. This is a compatibility default, not a claim that source IDs match
every vendor's tuning table. Devices that need a different mapping must opt in.

## Generate constants from vendor files

From the Android source root, run:

```sh
python3 hardware/qcom-caf/sm8450/audio/primary-hal/tools/generate_lvacfs_profiles.py \
    vendor/vendor/device/proprietary/vendor/etc/lvacfs_params \
    device/vendor/device/audio/lvacfs/LvacfsProfiles.h
```

The output directory must already exist. The generator reads the supplied
configuration files without changing them. Check the generated header into the
device tree and regenerate it when updating vendor tuning.

Each configuration directory gets a namespace, such as
`lvacfs::profiles::config_2mic`. Constants use names derived from parameter-file
basenames, but their values come from the configuration table. A file named
`ID5_MIC_Normal.txt` can therefore produce `MIC_NORMAL = 1` when its lookup ID
has been changed. Duplicate references to a tuning file get an ID suffix.

The generator emits constants only. It cannot infer the stock HAL's selection
rules from filenames. Review the device selector against the stock caller,
including route, rate, channel count, and scene changes.

Run the generator tests with:

```sh
python3 hardware/qcom-caf/sm8450/audio/primary-hal/tools/test_generate_lvacfs_profiles.py
```
