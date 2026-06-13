#pragma once

#include "implant/session.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class ImplantManager {
public:
    using ImplantPtr = std::shared_ptr<ImplantSession>;

    ImplantManager() = default;
    ~ImplantManager() = default;

    bool Add(const ImplantPtr& impl) {
        if (!impl) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        implants_[impl->implantId] = impl;
        return true;
    }

    bool Remove(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return implants_.erase(id) > 0;
    }

    ImplantPtr Get(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = implants_.find(id);
        return it != implants_.end() ? it->second : nullptr;
    }

    // Find by prefix (partial id match)
    ImplantPtr FindByPrefix(const std::string& prefix) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& p : implants_) {
            if (p.first.rfind(prefix, 0) == 0) return p.second;
        }
        return nullptr;
    }

    std::vector<ImplantPtr> List() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ImplantPtr> out;
        out.reserve(implants_.size());
        for (auto& p : implants_) out.push_back(p.second);
        return out;
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return implants_.size();
    }

    template<typename Fn>
    void ForEach(Fn&& fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& p : implants_) fn(p.second);
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, ImplantPtr> implants_;
};
