#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {

    if (key.size() + value.size() > _max_size) {
        return false;
    }

    if (_lru_index.find(key) == _lru_index.end()) {
        PutIfAbsent(key,value);

    } else {
        auto cur_node = _lru_index.find(key);
        moveToHead(&cur_node->second.get());
        return true;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {

    size_t node_size = key.size() + value.size();
    if (_max_size - _cur_size < node_size) {
        while (_max_size - _cur_size <= node_size) {
            deleteNode(_lru_tail);
        }
    }

    if (!_lru_head) {
        std::unique_ptr<lru_node> new_node(new lru_node{key, value});

        _lru_head = std::move(new_node);
        _lru_tail = new_node.get();

    } else {

        std::unique_ptr<lru_node> new_node(new lru_node{key, value, nullptr, std::move(_lru_head)});
        _lru_head = std::move(new_node);
        _lru_head->next->prev = _lru_head.get();
    }

    _lru_index.emplace(_lru_head->key, *_lru_head);
    _cur_size += key.size() + value.size();
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }

    auto cur_node = _lru_index.find(key);
    deleteNode(&cur_node->second.get());
    PutIfAbsent(key,value);
    return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {

    if (_lru_index.find(key) == _lru_index.end()) {
        return false;
    }

    auto cur_node = _lru_index.find(key);
    deleteNode(&cur_node->second.get());

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {

    if (_lru_index.find(key) == _lru_index.end()) {
        return false;
    }

    auto cur_node = _lru_index.find(key);
    value = cur_node->second.get().value;
    moveToHead(&cur_node->second.get());

    return true;
}

void SimpleLRU::deleteNode(lru_node* node) {

    if (!node->next && node->prev) {
        _lru_head.reset();
        _lru_tail = nullptr;

    } else if (!node->next) {
        _lru_tail = node->prev.get();
        _lru_tail->next = nullptr;

    } else if (!node->prev) {
        _lru_head = std::move(node->next);
        _lru_head->prev = nullptr;

    } else {
        node->next->prev = node->prev;
        node->prev->next = std::move(node->next);

    }

    _cur_size -= node->key.size() + node->value.size();
    _lru_index.erase(node->key);
}

void SimpleLRU::moveToHead(lru_node* node) {

    if (!node->next && node->prev) {
        return;

    } else if (!node->next) {
        deleteNode(_lru_tail);
        node->next = std::move(_lru_head);
        _lru_head->next->prev = _lru_head.get();

    } else if (!node->prev) {
        return;

    } else {
        deleteNode(node);
        PutIfAbsent(node->key,node->value);
    }
}

} // namespace Backend
} // namespace Afina
