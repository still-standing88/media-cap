#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>


namespace {
    class UniqueTextProperty {
    public:
        UniqueTextProperty()
        {
            p.value = nullptr;
        }

        UniqueTextProperty(const UniqueTextProperty&) = delete;
        UniqueTextProperty& operator=(const UniqueTextProperty&) = delete;

        UniqueTextProperty(UniqueTextProperty&& other):
            p{other.p}
        {
            other.p = XTextProperty{};
        }

        UniqueTextProperty& operator=(UniqueTextProperty&& other)
        {
            swap(*this, other);
            return *this;
        }

        friend void swap(UniqueTextProperty& lhs, UniqueTextProperty& rhs) {
            using std::swap;
            swap(lhs.p, rhs.p);
        }

        ~UniqueTextProperty()
        {
            if (p.value) {
                XFree(p.value);
            }
        }

        auto& get() {
            return p;
        }

    private:
        XTextProperty p;
    };

    auto GetWMName(Display* display, Window window)
    {
        auto x = UniqueTextProperty{};
        XGetWMName(display, window, &x.get());
        return x;
    }

    auto TextPropertyToStrings(
        Display* dpy,
        const XTextProperty& prop
    )
    {
        char **list = nullptr;
        auto n_strings = 0;
        auto result = std::vector<std::string>{};

        auto status = Xutf8TextPropertyToTextList(
            dpy,
            &prop,
            &list,
            &n_strings
        );

        if (status < Success or not n_strings or not list or not *list) {
            if (list) XFreeStringList(list);
            return result;
        }

        for (auto i = 0; i < n_strings; ++i) {
            result.emplace_back(list[i]);
        }

        XFreeStringList(list);

        return result;
    }
}

namespace SL
{
namespace Screen_Capture
{

    void AddWindow(Display* display, XID& window, std::vector<Window>& wnd)
    {
        using namespace std::string_literals;

        Window w = {};
        w.Handle = static_cast<size_t>( (size_t) window);

        XWindowAttributes wndattr{};
        if (!XGetWindowAttributes(display, window, &wndattr)) return;

        w.Position = Point{ wndattr.x, wndattr.y };
        w.Size = Point{ wndattr.width, wndattr.height };
		
        UniqueTextProperty net_name;
        const auto name_atom = XInternAtom(display, "_NET_WM_NAME", True);
        const auto utf8_atom = XInternAtom(display, "UTF8_STRING", True);
        auto& prop = net_name.get();
        std::string name;
        if (name_atom != None && utf8_atom != None &&
            XGetTextProperty(display, window, &prop, name_atom) &&
            prop.value && prop.format == 8 && prop.encoding == utf8_atom) {
            name.assign(reinterpret_cast<const char*>(prop.value), prop.nitems);
        } else {
            auto wm_name = GetWMName(display, window);
            if (wm_name.get().value) {
                auto candidates = TextPropertyToStrings(display, wm_name.get());
                if (!candidates.empty()) name = std::move(candidates.front());
            }
        }
        auto bytes = std::min(name.size(), sizeof(w.Name) - 1);
        while (bytes < name.size() && bytes && (static_cast<unsigned char>(name[bytes]) & 0xc0) == 0x80)
            --bytes;
        std::copy_n(name.begin(), bytes, w.Name);
        wnd.push_back(w);
    }

    std::vector<Window> GetWindows()
    {
        auto* display = XOpenDisplay(NULL);
        if (!display) return {};
        Atom a = XInternAtom(display, "_NET_CLIENT_LIST", true);
        if (a == None) {
            XCloseDisplay(display);
            return {};
        }
        Atom actualType;
        int format;
        unsigned long numItems, bytesAfter;
        unsigned char* data = 0;
        int status = XGetWindowProperty(display,
                                        XDefaultRootWindow(display),
                                        a,
                                        0L,
                                        (~0L),
                                        false,
                                        AnyPropertyType,
                                        &actualType,
                                        &format,
                                        &numItems,
                                        &bytesAfter,
                                        &data);
        std::vector<Window> ret;
        if(status == Success && format == 32 && data) {
            auto array = (XID*)data;
            for(decltype(numItems) k = 0; k < numItems; k++) {
                auto w = array[k];
                AddWindow(display, w, ret);
            }
        }
        if (data) XFree(data);
        XCloseDisplay(display);
        return ret;
    }
}
}
