#include "binding.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>

namespace mediacap::python {
namespace {
[[noreturn]] void discovery_failed(const char* operation) {
    // The native global latest error can belong to an unrelated concurrent call.
    throw std::runtime_error(std::string(operation) + " failed; inspect MediaCap diagnostics");
}

template <class T> void release_entry(T&) {}
void release_entry(MediaCap_CameraInfo& item) { MediaCap_FreeCameraInfoContents(&item); }

template <class T> struct Inventory {
    std::vector<T> entries;
    size_t count = 0;
    ~Inventory() { for (auto& entry : entries) release_entry(entry); }
};

template <class T, class F>
void fill_inventory(Inventory<T>& inventory, F enumerate, const char* operation) {
    py::gil_scoped_release release;
    int capacity = 0;
    if (enumerate(nullptr, &capacity) || capacity < 0) discovery_failed(operation);

    for (int attempt = 0; capacity > 0; ++attempt) {
        for (auto& entry : inventory.entries) release_entry(entry);
        // FreeCameraInfoContents clears pointers but does not reset counts.
        inventory.entries.assign(static_cast<size_t>(capacity), T{});
        int count = capacity;
        if (enumerate(inventory.entries.data(), &count) || count < 0 || count > capacity)
            discovery_failed(operation);
        inventory.count = static_cast<size_t>(count);
        if (count < capacity || attempt == 2) return;

        int latest = 0;
        if (enumerate(nullptr, &latest) || latest < 0) discovery_failed(operation);
        if (latest <= capacity) return;
        capacity = latest;
    }
}

template <class T, class F>
auto classic_inventory(F enumerate, const char* operation) {
    Inventory<T> inventory;
    fill_inventory(inventory, enumerate, operation);
    return std::vector<T>(inventory.entries.begin(), inventory.entries.begin() + inventory.count);
}

template <class T>
std::vector<T> copy_array(const T* data, size_t count) {
    return count ? std::vector<T>(data, data + count) : std::vector<T>{};
}

template <class T, void (*Free)(T*, size_t)>
struct AllocatedInventory {
    T* entries = nullptr;
    size_t count = 0;
    ~AllocatedInventory() { Free(entries, count); }
};

py::str display_text_ptr(const char* text) {
    return text ? decode(text, std::strlen(text)) : py::str("");
}
}

void bind_discovery(py::module_& module) {
    py::class_<MediaCap_FeatureRequirements>(module, "FeatureRequirements")
        .def_readonly("platform", &MediaCap_FeatureRequirements::platform)
        .def_property_readonly("included_in_build", [](const MediaCap_FeatureRequirements& value) {
            return value.included_in_build != 0;
        })
        .def_readonly("prerequisites", &MediaCap_FeatureRequirements::prerequisites)
        .def_readonly("minimum_os_major", &MediaCap_FeatureRequirements::minimum_os_major)
        .def_readonly("minimum_os_minor", &MediaCap_FeatureRequirements::minimum_os_minor)
        .def_readonly("minimum_os_build", &MediaCap_FeatureRequirements::minimum_os_build)
        .def_property_readonly("backend", [](const MediaCap_FeatureRequirements& value) { return display_text(value.backend); });
    module.def("get_feature_requirements", [](MediaCap_Feature feature) {
        MediaCap_FeatureRequirements result{};
        if (!MediaCap_GetFeatureRequirements(feature, &result)) throw py::value_error("Unknown capture feature");
        return result;
    }, py::arg("feature"));
    module.def("get_monitors", [] { return classic_inventory<MediaCap_Monitor>(MediaCap_GetMonitors, "Monitor discovery"); });
    module.def("get_windows", [] { return classic_inventory<MediaCap_Window>(MediaCap_GetWindows, "Window discovery"); });
    module.def("get_camera_devices", [] { return classic_inventory<MediaCap_Camera>(MediaCap_GetCameraDevices, "Camera discovery"); });
    module.def("get_audio_devices", [] { return classic_inventory<MediaCap_AudioDevice>(MediaCap_GetAudioDevices, "Audio discovery"); });
    module.def("get_cameras", [] {
        Inventory<MediaCap_CameraInfo> inventory;
        fill_inventory(inventory, MediaCap_GetCameras, "Camera capability discovery");
        std::vector<CameraDetail> cameras;
        cameras.reserve(inventory.count);
        for (size_t index = 0; index < inventory.count; ++index) {
            const auto& entry = inventory.entries[index];
            cameras.push_back({entry.camera,
                copy_array(entry.supported_resolutions, entry.num_supported_resolutions),
                copy_array(entry.supported_pixel_formats, entry.num_supported_pixel_formats)});
        }
        return cameras;
    });
    module.def("get_audio_capabilities", [] {
        MediaCap_AudioCapabilities out{};
        py::gil_scoped_release release;
        if (!MediaCap_GetAudioCapabilities(&out)) discovery_failed("Audio capability query");
        return out;
    });
    module.def("get_audio_capture_devices", [] {
        AllocatedInventory<MediaCap_AudioCaptureDevice, MediaCap_FreeAudioCaptureDevices> raw;
        {
            py::gil_scoped_release release;
            if (!MediaCap_GetAudioCaptureDevices(&raw.entries, &raw.count)) {
                MediaCap_AudioCapabilities capabilities{};
                if (MediaCap_GetAudioCapabilities(&capabilities) && !capabilities.backend_enabled)
                    return std::vector<AudioCaptureDevice>{};
                discovery_failed("FlexAudio device discovery");
            }
        }
        std::vector<AudioCaptureDevice> devices;
        devices.reserve(raw.count);
        for (size_t index = 0; index < raw.count; ++index) {
            const auto& entry = raw.entries[index];
            devices.push_back({decode(entry.id, std::strlen(entry.id), "strict"),
                display_text_ptr(entry.name), entry.kind, entry.sample_rate, entry.channels, entry.is_default != 0});
        }
        return devices;
    });
    module.def("get_audio_processes", [] {
        AllocatedInventory<MediaCap_AudioProcess, MediaCap_FreeAudioProcesses> raw;
        {
            py::gil_scoped_release release;
            if (!MediaCap_GetAudioProcesses(&raw.entries, &raw.count)) {
                MediaCap_AudioCapabilities capabilities{};
                if (MediaCap_GetAudioCapabilities(&capabilities) && !capabilities.backend_enabled)
                    return std::vector<AudioProcess>{};
                discovery_failed("FlexAudio process discovery");
            }
        }
        std::vector<AudioProcess> processes;
        processes.reserve(raw.count);
        for (size_t index = 0; index < raw.count; ++index) {
            const auto& entry = raw.entries[index];
            py::object executable = py::none();
            if (entry.executable) executable = display_text_ptr(entry.executable);
            processes.push_back({entry.pid, display_text_ptr(entry.name), std::move(executable), entry.output_active});
        }
        return processes;
    });
}
}
