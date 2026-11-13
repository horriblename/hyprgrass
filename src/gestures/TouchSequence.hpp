#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string_view>

// Exact, case-sensitive namespace names, separated by commas. Empty entries are ignored.
inline bool matchesLayerNamespace(std::string_view names, std::string_view name) {
    if (name.empty())
        return false;

    while (!names.empty()) {
        const auto comma = names.find(',');
        auto entry       = names.substr(0, comma);
        const auto first = entry.find_first_not_of(" \t\r\n");
        if (first != std::string_view::npos) {
            entry = entry.substr(first, entry.find_last_not_of(" \t\r\n") - first + 1);
            if (entry == name)
                return true;
        }
        if (comma == std::string_view::npos)
            break;
        names.remove_prefix(comma + 1);
    }
    return false;
}

// Hyprland touch IDs are libinput seat slots, shared across devices. Device tokens
// are non-owning and only used to remove contacts on hardware cancel/unplug.
// An excluded sequence owns every subsequent contact until the last one ends;
// splitting it would allow a gesture to cancel touches already delivered to a client.
class TouchSequenceBypass {
  public:
    bool active() const {
        return !contacts.empty();
    }

    bool down(int32_t id, const void* device, bool first, bool excluded) {
        if (!active() && !(first && excluded))
            return false;
        contacts.insert_or_assign(id, device);
        return true;
    }

    bool up(int32_t id) {
        const bool bypass = active();
        contacts.erase(id);
        return bypass;
    }

    void cancel(int32_t id, const void* device) {
        const auto it = contacts.find(id);
        if (it != contacts.end() && it->second == device)
            contacts.erase(it);
    }

    void removeDevice(const void* device) {
        std::erase_if(contacts, [device](const auto& contact) { return contact.second == device; });
    }

  private:
    std::map<int32_t, const void*> contacts;
};
