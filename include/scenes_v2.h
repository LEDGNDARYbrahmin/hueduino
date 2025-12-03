#ifndef SCENES_V2_H
#define SCENES_V2_H

#include <Arduino.h>
#include "config.h"

// Maximum scenes supported
#define MAX_SCENES_V2 100

/**
 * Advanced Scene System
 * 
 * Supports:
 * - Static scenes (save/recall light states)
 * - Dynamic scenes (animated effects)
 * - Scene transitions
 * - Scene scheduling
 * - Scene groups/rooms
 */

// Scene types
enum SceneType {
    SCENE_TYPE_LIGHT_SCENE,      // Static light states
    SCENE_TYPE_GROUP_SCENE,      // Group-level scene
    SCENE_TYPE_DYNAMIC_SCENE     // Animated/dynamic scene
};

// Dynamic scene palettes
enum DynamicPalette {
    PALETTE_SPRING_BLOSSOM,
    PALETTE_TROPICAL_TWILIGHT,
    PALETTE_NORDIC_AURORA,
    PALETTE_AUTUMN_GOLD,
    PALETTE_OCEAN_DAWN,
    PALETTE_BLOOD_MOON,
    PALETTE_CUSTOM
};

// Per-light state in a scene
struct SceneLightState {
    uint8_t light_index;         // Index in lights array
    bool on;
    uint8_t bri;
    uint16_t hue;
    uint8_t sat;
    uint16_t ct;
    float xy[2];
    uint8_t colormode;           // 1=xy, 2=ct, 3=hs
    uint16_t transition_time;    // Transition time in ms
};

// Dynamic scene settings
struct DynamicSceneSettings {
    bool enabled;
    uint8_t speed;               // 0-100 (speed of animation)
    DynamicPalette palette;
    uint8_t brightness;          // Overall brightness 0-254
    uint16_t update_interval;    // ms between updates
};

// Scene structure (v2)
struct SceneV2 {
    char id[37];                 // UUID
    char id_v1[16];              // v1 compatibility ID (1-100)
    char name[64];
    SceneType type;
    char group_id[37];           // Associated group UUID
    
    // Light states
    SceneLightState light_states[MAX_LIGHTS];
    uint8_t lights_count;
    
    // Dynamic settings
    DynamicSceneSettings dynamic;
    
    // Metadata
    char image_id[37];           // Scene thumbnail reference
    bool auto_dynamic;           // Auto-convert to dynamic
    bool recycle;                // Can be auto-deleted
    bool locked;                 // Cannot be modified
    uint32_t version;
    uint32_t created_timestamp;
    uint32_t modified_timestamp;
    
    // App data (for Hue app compatibility)
    char app_data[128];
};

/**
 * Scene Manager
 * 
 * Manages scene creation, storage, recall, and transitions
 */
class SceneManager {
public:
    SceneManager();
    
    // Initialize scene manager
    bool begin();
    
    // Scene CRUD operations
    bool createScene(const char* name, const uint8_t* lights, uint8_t lightsCount, 
                     const char* groupId = nullptr);
    bool updateScene(const char* sceneId, const SceneLightState* states, uint8_t count);
    bool deleteScene(const char* sceneId);
    SceneV2* getScene(const char* sceneId);
    SceneV2* getSceneByIndex(uint8_t index);
    uint8_t getScenesCount() const { return _scenesCount; }
    
    // Scene recall
    bool recallScene(const char* sceneId, uint16_t transitionTime = 400);
    bool recallSceneForGroup(const char* sceneId, const char* groupId, uint16_t transitionTime = 400);
    
    // Capture current state as scene
    bool captureScene(const char* sceneId, const uint8_t* lights, uint8_t lightsCount);
    
    // Dynamic scenes
    bool setDynamicScene(const char* sceneId, const DynamicSceneSettings& settings);
    bool startDynamicScene(const char* sceneId);
    bool stopDynamicScene(const char* sceneId);
    void updateDynamicScenes(); // Call in loop()
    
    // Scene groups/filtering
    uint8_t getScenesByGroup(const char* groupId, SceneV2** scenes, uint8_t maxScenes);
    uint8_t getScenesByType(SceneType type, SceneV2** scenes, uint8_t maxScenes);
    
    // Persistence
    bool saveScenes();
    bool loadScenes();
    
    // Utilities
    String generateSceneId();
    bool sceneExists(const char* sceneId);
    void printScenes();
    
private:
    SceneV2 _scenes[MAX_SCENES_V2];
    uint8_t _scenesCount;
    
    // Dynamic scene tracking
    struct ActiveDynamicScene {
        char scene_id[37];
        unsigned long last_update;
        uint8_t animation_step;
        bool active;
    };
    
    ActiveDynamicScene _activeScenes[10]; // Max 10 concurrent dynamic scenes
    uint8_t _activeScenesCount;
    
    // Helper methods
    int findSceneIndex(const char* sceneId);
    bool applyLightState(uint8_t lightIndex, const SceneLightState& state);
    void updateDynamicScene(ActiveDynamicScene& activeScene);
    void calculateDynamicColor(const DynamicSceneSettings& settings, uint8_t step, 
                               uint16_t& hue, uint8_t& sat, uint8_t& bri);
};

// Global scene manager instance
extern SceneManager sceneManager;

#endif // SCENES_V2_H
