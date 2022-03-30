#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }

    auto cur_node = _lru_index.find(key);
    if (cur_node == _lru_index.end()) {
        return addNode(key,value);

    } else {
        return changeNode(cur_node,value);
    }

}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }

    auto cur_node = _lru_index.find(key);
    if (cur_node != _lru_index.end()) {
        return false;
    }

    return addNode(key,value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto cur_node = _lru_index.find(key);
    if (cur_node == _lru_index.end()) {
       return false;
    }

    return changeNode(cur_node,value);;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto cur_node = _lru_index.find(key);
    if (cur_node == _lru_index.end()) {
        return false;
    }

    return deleteNode(&cur_node->second.get());
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto cur_node = _lru_index.find(key);
    if (cur_node == _lru_index.end()) {
        return false;
    }

    value = cur_node->second.get().value;
    return moveToHead(&cur_node->second.get());
}

bool SimpleLRU::addNode(const std::string &key, const std::string &value) {
    size_t node_size = key.size() + value.size();
    while (_max_size < _cur_size + node_size) {
        deleteNode(_lru_tail);
    }

    if (!_lru_head) {
        std::unique_ptr<lru_node> new_node(new lru_node{key, value, nullptr, nullptr});
        _lru_head = std::move(new_node);
        _lru_tail = _lru_head.get();

    } else {

        std::unique_ptr<lru_node> new_node(new lru_node{key, value, nullptr, std::move(_lru_head)});
        _lru_head = std::move(new_node);
        _lru_head->next->prev = _lru_head.get();
    }

    _lru_index.emplace(_lru_head->key, *_lru_head);
    _cur_size += key.size() + value.size();
    return true;
}

bool SimpleLRU::deleteNode(lru_node* node) {
    _cur_size -= node->key.size() + node->value.size();
    _lru_index.erase(node->key);

    if (!node->next && !node->prev) {
        _lru_head.reset();
        _lru_tail = nullptr;

    } else if (!node->next) {
        _lru_tail = node->prev;
        _lru_tail->next = nullptr;

    } else if (!node->prev) {
        _lru_head = std::move(node->next);
        _lru_head->prev = nullptr;

    } else {
        node->next->prev = node->prev;
        node->prev->next = std::move(node->next);
    }

    return true;
}

bool SimpleLRU::moveToHead(lru_node* node) {
    if (!node->next && !node->prev) {
        return true;

    } else if (!node->prev) {
        return true;

    } else if (!node->next) {
        node->next = std::move(_lru_head);
        _lru_head = std::move(node->prev->next);
        _lru_head->next->prev = _lru_head.get();
        _lru_tail = node->prev;

    } else {
        const std::string key = node->key;
        std::string value = node->value;
        deleteNode(node);
        PutIfAbsent(key, value);
    }

    _lru_tail->next = nullptr;
    _lru_head->prev = nullptr;
    return true;
}

bool SimpleLRU::changeNode(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>::iterator cur_node, const std::string &value) {
    std::string cur_value = cur_node->second.get().value;
    moveToHead(&cur_node->second.get());

    int delta = int(value.size()) - int(cur_value.size());
    int increase = std::max(delta, 0);

    while (_max_size < _cur_size + delta) {
        deleteNode(_lru_tail);
    }

    cur_node->second.get().value = value;
    _cur_size = _cur_size + delta;
    return true;
}

} // namespace Backend
} // namespace Afina
