#include "storage_manager.h"

StorageManager::StorageManager() 
    : _dirtyCount(0), _autoSaveEnabled(false), _debounceMs(5000) {
    memset(_dirtyFlags, 0, sizeof(_dirtyFlags));
}

StorageManager::~StorageManager() {
    flush(); // Save any pending changes
}

bool StorageManager::begin() {
    Serial.println("[STORAGE] Storage Manager initialized");
    return true;
}

bool StorageManager::erase(const char* nameSpace, const char* key) {
    if (!_prefs.begin(nameSpace, false)) {
        Serial.printf("[STORAGE] Failed to open namespace: %s\n", nameSpace);
        return false;
    }
    
    bool success = _prefs.remove(key);
    _prefs.end();
    
    if (success) {
        Serial.printf("[STORAGE] Erased %s/%s\n", nameSpace, key);
        markDirty(nameSpace);
    }
    
    return success;
}

bool StorageManager::eraseAll(const char* nameSpace) {
    if (!_prefs.begin(nameSpace, false)) {
        Serial.printf("[STORAGE] Failed to open namespace: %s\n", nameSpace);
        return false;
    }
    
    bool success = _prefs.clear();
    _prefs.end();
    
    if (success) {
        Serial.printf("[STORAGE] Cleared all keys in namespace: %s\n", nameSpace);
        clearDirty(nameSpace);
    }
    
    return success;
}

bool StorageManager::eraseNamespace(const char* nameSpace) {
    // ESP32 Preferences doesn't have a way to delete a namespace
    // We can only clear all keys in it
    return eraseAll(nameSpace);
}

size_t StorageManager::getFreeSpace(const char* nameSpace) {
    if (!_prefs.begin(nameSpace, true)) {
        return 0;
    }
    
    size_t free = _prefs.freeEntries();
    _prefs.end();
    
    return free;
}

size_t StorageManager::getUsedSpace(const char* nameSpace) {
    // ESP32 Preferences doesn't provide direct used space info
    // Return 0 for now - could be enhanced with manual tracking
    return 0;
}

void StorageManager::markDirty(const char* nameSpace) {
    DirtyFlag* flag = findDirtyFlag(nameSpace);
    
    if (!flag) {
        flag = createDirtyFlag(nameSpace);
    }
    
    if (flag) {
        flag->dirty = true;
        flag->lastChange = millis();
    }
}

bool StorageManager::isDirty(const char* nameSpace) {
    DirtyFlag* flag = findDirtyFlag(nameSpace);
    return flag ? flag->dirty : false;
}

void StorageManager::clearDirty(const char* nameSpace) {
    DirtyFlag* flag = findDirtyFlag(nameSpace);
    if (flag) {
        flag->dirty = false;
    }
}

void StorageManager::enableAutoSave(bool enable, uint32_t debounceMs) {
    _autoSaveEnabled = enable;
    _debounceMs = debounceMs;
    
    if (enable) {
        Serial.printf("[STORAGE] Auto-save enabled (debounce: %d ms)\n", debounceMs);
    } else {
        Serial.println("[STORAGE] Auto-save disabled");
    }
}

void StorageManager::flush() {
    for (uint8_t i = 0; i < _dirtyCount; i++) {
        if (_dirtyFlags[i].dirty) {
            Serial.printf("[STORAGE] Flushing namespace: %s\n", _dirtyFlags[i].nameSpace);
            _dirtyFlags[i].dirty = false;
            // Note: Actual data is already saved, this just clears the dirty flag
        }
    }
}

void StorageManager::update() {
    if (!_autoSaveEnabled) {
        return;
    }
    
    unsigned long now = millis();
    
    for (uint8_t i = 0; i < _dirtyCount; i++) {
        if (_dirtyFlags[i].dirty && 
            (now - _dirtyFlags[i].lastChange >= _debounceMs)) {
            
            Serial.printf("[STORAGE] Auto-saving namespace: %s\n", _dirtyFlags[i].nameSpace);
            _dirtyFlags[i].dirty = false;
            // Note: Actual data is already saved, this just clears the dirty flag
        }
    }
}

bool StorageManager::exists(const char* nameSpace, const char* key) {
    if (!_prefs.begin(nameSpace, true)) {
        return false;
    }
    
    bool exists = _prefs.isKey(key);
    _prefs.end();
    
    return exists;
}

void StorageManager::printStats() {
    Serial.println("[STORAGE] ========================================");
    Serial.println("[STORAGE] Storage Manager Statistics");
    Serial.println("[STORAGE] ========================================");
    
    for (uint8_t i = 0; i < _dirtyCount; i++) {
        Serial.printf("[STORAGE] Namespace: %s\n", _dirtyFlags[i].nameSpace);
        Serial.printf("[STORAGE]   Dirty: %s\n", _dirtyFlags[i].dirty ? "Yes" : "No");
        Serial.printf("[STORAGE]   Free entries: %zu\n", getFreeSpace(_dirtyFlags[i].nameSpace));
    }
    
    Serial.println("[STORAGE] ========================================");
}

StorageManager::DirtyFlag* StorageManager::findDirtyFlag(const char* nameSpace) {
    for (uint8_t i = 0; i < _dirtyCount; i++) {
        if (strcmp(_dirtyFlags[i].nameSpace, nameSpace) == 0) {
            return &_dirtyFlags[i];
        }
    }
    return nullptr;
}

StorageManager::DirtyFlag* StorageManager::createDirtyFlag(const char* nameSpace) {
    if (_dirtyCount >= MAX_DIRTY_FLAGS) {
        Serial.println("[STORAGE] WARNING: Maximum dirty flags reached");
        return nullptr;
    }
    
    DirtyFlag* flag = &_dirtyFlags[_dirtyCount++];
    strncpy(flag->nameSpace, nameSpace, sizeof(flag->nameSpace) - 1);
    flag->dirty = false;
    flag->lastChange = 0;
    
    return flag;
}
