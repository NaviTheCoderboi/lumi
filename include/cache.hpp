#ifndef CACHE_HPP
#define CACHE_HPP

#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

#include <functional>

template <typename Key, typename Value>
class LRUCache {
    using ListItem = typename std::list<std::pair<Key, Value>>::iterator;
    std::size_t capacity;
    std::list<std::pair<Key, Value>> items;
    std::unordered_map<Key, ListItem> map;
    std::function<void(Value)> onEvict;

   public:
    LRUCache(std::size_t capacity, std::function<void(Value)> onEvict = nullptr)
        : capacity(capacity), onEvict(onEvict) {};

    ~LRUCache() {
        clear();
    }

    std::optional<Value> get(const Key& key) {
        auto mapIt{map.find(key)};
        if (mapIt == map.end()) return std::nullopt;

        moveToFront(mapIt);
        return mapIt->second->second;
    };

    void set(const Key& key, Value value) {
        auto mapIt{map.find(key)};

        if (mapIt == map.end()) {
            if (items.size() >= capacity) {
                auto lastItem{items.back()};
                if (onEvict) {
                    onEvict(lastItem.second);
                }
                map.erase(lastItem.first);
                items.pop_back();
            }

            items.emplace_front(key, std::move(value));
            map.emplace(key, items.begin());

            return;
        }

        mapIt->second->second = std::move(value);
        moveToFront(mapIt);

        return;
    };

    void erase(const Key& key) {
        auto mapIt{map.find(key)};
        if (mapIt == map.end()) return;

        if (onEvict) {
            onEvict(mapIt->second->second);
        }
        items.erase(mapIt->second);
        map.erase(mapIt);
    };

    void clear() {
        if (onEvict) {
            for (const auto& item : items) {
                onEvict(item.second);
            }
        }
        items.clear();
        map.clear();
    };

   private:
    void moveToFront(typename std::unordered_map<Key, ListItem>::iterator it) {
        items.splice(items.begin(), items, it->second);
        it->second = items.begin();
    };
};

#endif  // CACHE_HPP