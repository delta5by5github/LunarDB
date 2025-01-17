#include "cache.h"
#include <algorithm>

Cache::Cache(size_t max_size) : max_size(max_size) {}

void Cache::set(const std::string &key, const std::string &value, int ttl_seconds) {
    evict_if_needed();
    CacheEntry entry;
    entry.is_list = false;
    new (&entry.string_value) std::string(value);
    entry.has_expiry = (ttl_seconds > 0);
    entry.expiry = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds);
    data[key] = std::move(entry);
}

std::string Cache::get(const std::string &key) const {
    auto it = data.find(key);
    if (it != data.end()) {
        if (!it->second.has_expiry || it->second.expiry > std::chrono::steady_clock::now()) {
            return it->second.is_list ? "" : it->second.string_value;
        }
    }
    return "";
}

bool Cache::del(const std::string &key) {
    return data.erase(key) > 0;
}

void Cache::clear() {
    data.clear();
}

size_t Cache::size() const {
    return data.size();
}

void Cache::mset(const std::vector<std::pair<std::string, std::string>> &kvs) {
    for (const auto &kv : kvs) {
        set(kv.first, kv.second);
    }
}

std::vector<std::string> Cache::mget(const std::vector<std::string> &keys) const {
    std::vector<std::string> results;
    results.reserve(keys.size());
    for (const auto &key : keys) {
        results.push_back(get(key));
    }
    return results;
}

std::vector<std::string> Cache::keys() const {
    std::vector<std::string> result;
    result.reserve(data.size());
    for (const auto &entry : data) {
        result.push_back(entry.first);
    }
    return result;
}

void Cache::cleanup_expired() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = data.begin(); it != data.end();) {
        if (it->second.has_expiry && it->second.expiry <= now) {
            it = data.erase(it);
        } else {
            ++it;
        }
    }
}

void Cache::evict_if_needed() {
    if (data.size() >= max_size) {
        auto oldest = std::min_element(data.begin(), data.end(),
                                       [](const auto &a, const auto &b) {
                                           return a.second.expiry < b.second.expiry;
                                       });
        data.erase(oldest);
    }
}

void Cache::lpush(const std::string& key, const std::string& value) {
    auto it = data.find(key);
    if (it == data.end() || !it->second.is_list) {
        CacheEntry entry;
        entry.is_list = true;
        new (&entry.list_value) std::list<std::string>{value};
        entry.has_expiry = false;
        data[key] = std::move(entry);
    } else {
        it->second.list_value.push_front(value);
    }
}

std::string Cache::lpop(const std::string& key) {
    auto it = data.find(key);
    if (it != data.end() && it->second.is_list) {
        auto& list = it->second.list_value;
        if (!list.empty()) {
            std::string value = list.front();
            list.pop_front();
            if (list.empty()) {
                data.erase(it);
            }
            return value;
        }
    }
    return "";
}

void Cache::rpush(const std::string& key, const std::string& value) {
    auto it = data.find(key);
    if (it == data.end() || !it->second.is_list) {
        CacheEntry entry;
        entry.is_list = true;
        new (&entry.list_value) std::list<std::string>{value};
        entry.has_expiry = false;
        data[key] = std::move(entry);
    } else {
        it->second.list_value.push_back(value);
    }
}

std::string Cache::rpop(const std::string& key) {
    auto it = data.find(key);
    if (it != data.end() && it->second.is_list) {
        auto& list = it->second.list_value;
        if (!list.empty()) {
            std::string value = list.back();
            list.pop_back();
            if (list.empty()) {
                data.erase(it);
            }
            return value;
        }
    }
    return "";
}

std::vector<std::string> Cache::lrange(const std::string& key, int start, int stop) const {
    auto it = data.find(key);
    if (it != data.end() && it->second.is_list) {
        const auto& list = it->second.list_value;
        std::vector<std::string> result;
        int size = static_cast<int>(list.size());

        if (start < 0) start = size + start;
        if (stop < 0) stop = size + stop;

        start = std::max(0, std::min(start, size - 1));
        stop = std::max(0, std::min(stop, size - 1));

        auto it_start = std::next(list.begin(), start);
        auto it_stop = std::next(list.begin(), stop + 1);

        result.insert(result.end(), it_start, it_stop);
        return result;
    }
    return {};
}

size_t Cache::llen(const std::string& key) const {
    auto it = data.find(key);
    if (it != data.end() && it->second.is_list) {
        return it->second.list_value.size();
    }
    return 0;
}
