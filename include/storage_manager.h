#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * Storage Manager - Enhanced NVS abstraction layer
 * 
 * Provides a clean interface for storing and retrieving data from ESP32 NVS.
 * Features:
 * - Template-based save/load for any data type
 * - Array storage with automatic count tracking
 * - Dirty flag tracking for efficient saves
 * - Auto-save with debouncing
 * - Space management and monitoring
 */

class StorageManager {
public:
    StorageManager();
    ~StorageManager();
    
    // Initialize storage manager
    bool begin();
    
    // Generic save/load for single items
    template<typename T>
    bool save(const char* nameSpace, const char* key, const T& data);
    
    template<typename T>
    bool load(const char* nameSpace, const char* key, T& data);
    
    // Array save/load with count tracking
    template<typename T>
    bool saveArray(const char* nameSpace, const char* keyPrefix, 
                   const T* array, uint8_t count, uint8_t maxCount);
    
    template<typename T>
    bool loadArray(const char* nameSpace, const char* keyPrefix, 
                   T* array, uint8_t& count, uint8_t maxCount);
    
    // Erase operations
    bool erase(const char* nameSpace, const char* key);
    bool eraseAll(const char* nameSpace);
    bool eraseNamespace(const char* nameSpace);
    
    // Space management
    size_t getFreeSpace(const char* nameSpace);
    size_t getUsedSpace(const char* nameSpace);
    
    // Dirty flag management (for auto-save)
    void markDirty(const char* nameSpace);
    bool isDirty(const char* nameSpace);
    void clearDirty(const char* nameSpace);
    
    // Auto-save
    void enableAutoSave(bool enable, uint32_t debounceMs = 5000);
    void flush(); // Force save all dirty namespaces
    void update(); // Call in loop() for auto-save
    
    // Utilities
    bool exists(const char* nameSpace, const char* key);
    void printStats();
    
private:
    Preferences _prefs;
    
    // Dirty tracking
    struct DirtyFlag {
        char nameSpace[32];
        bool dirty;
        unsigned long lastChange;
    };
    
    static const uint8_t MAX_DIRTY_FLAGS = 10;
    DirtyFlag _dirtyFlags[MAX_DIRTY_FLAGS];
    uint8_t _dirtyCount;
    
    // Auto-save settings
    bool _autoSaveEnabled;
    uint32_t _debounceMs;
    
    // Helper methods
    DirtyFlag* findDirtyFlag(const char* nameSpace);
    DirtyFlag* createDirtyFlag(const char* nameSpace);
};

// Template implementations must be in header

template<typename T>
bool StorageManager::save(const char* nameSpace, const char* key, const T& data) {
    if (!_prefs.begin(nameSpace, false)) {
        Serial.printf("[STORAGE] Failed to open namespace: %s\n", nameSpace);
        return false;
    }
    
    size_t written = _prefs.putBytes(key, &data, sizeof(T));
    _prefs.end();
    
    if (written == sizeof(T)) {
        markDirty(nameSpace);
        return true;
    }
    
    Serial.printf("[STORAGE] Failed to save %s/%s\n", nameSpace, key);
    return false;
}

template<typename T>
bool StorageManager::load(const char* nameSpace, const char* key, T& data) {
    if (!_prefs.begin(nameSpace, true)) { // Read-only
        Serial.printf("[STORAGE] Failed to open namespace: %s\n", nameSpace);
        return false;
    }
    
    size_t read = _prefs.getBytes(key, &data, sizeof(T));
    _prefs.end();
    
    if (read == sizeof(T)) {
        return true;
    }
    
    Serial.printf("[STORAGE] Failed to load %s/%s\n", nameSpace, key);
    return false;
}

template<typename T>
bool StorageManager::saveArray(const char* nameSpace, const char* keyPrefix, 
                               const T* array, uint8_t count, uint8_t maxCount) {
    if (count > maxCount) {
        Serial.printf("[STORAGE] Count %d exceeds max %d\n", count, maxCount);
        return false;
    }
    
    if (!_prefs.begin(nameSpace, false)) {
        Serial.printf("[STORAGE] Failed to open namespace: %s\n", nameSpace);
        return false;
    }
    
    // Save count
    char countKey[64];
    snprintf(countKey, sizeof(countKey), "%s_count", keyPrefix);
    _prefs.putUChar(countKey, count);
    
    // Save each item
    bool success = true;
    for (uint8_t i = 0; i < count; i++) {
        char itemKey[64];
        snprintf(itemKey, sizeof(itemKey), "%s_%d", keyPrefix, i);
        
        size_t written = _prefs.putBytes(itemKey, &array[i], sizeof(T));
        if (written != sizeof(T)) {
            Serial.printf("[STORAGE] Failed to save %s/%s\n", nameSpace, itemKey);
            success = false;
            break;
        }
    }
    
    _prefs.end();
    
    if (success) {
        markDirty(nameSpace);
    }
    
    return success;
}

template<typename T>
bool StorageManager::loadArray(const char* nameSpace, const char* keyPrefix, 
                               T* array, uint8_t& count, uint8_t maxCount) {
    if (!_prefs.begin(nameSpace, true)) { // Read-only
        Serial.printf("[STORAGE] Failed to open namespace: %s\n", nameSpace);
        return false;
    }
    
    // Load count
    char countKey[64];
    snprintf(countKey, sizeof(countKey), "%s_count", keyPrefix);
    count = _prefs.getUChar(countKey, 0);
    
    if (count == 0) {
        _prefs.end();
        return true; // No items, but not an error
    }
    
    if (count > maxCount) {
        Serial.printf("[STORAGE] Stored count %d exceeds max %d\n", count, maxCount);
        count = maxCount; // Clamp to max
    }
    
    // Load each item
    bool success = true;
    for (uint8_t i = 0; i < count; i++) {
        char itemKey[64];
        snprintf(itemKey, sizeof(itemKey), "%s_%d", keyPrefix, i);
        
        size_t read = _prefs.getBytes(itemKey, &array[i], sizeof(T));
        if (read != sizeof(T)) {
            Serial.printf("[STORAGE] Failed to load %s/%s\n", nameSpace, itemKey);
            success = false;
            break;
        }
    }
    
    _prefs.end();
    return success;
}

#endif // STORAGE_MANAGER_H
