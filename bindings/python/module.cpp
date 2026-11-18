#include "binding.hpp"

PYBIND11_MODULE(_native, module) {
    using namespace mediacap::python;
    module.doc() = "Native MediaCap capture bindings";
    bind_enums(module);
    bind_types(module);
    bind_discovery(module);
    bind_frames(module);
    bind_session(module);
}
