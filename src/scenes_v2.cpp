#include "scenes_v2.h"
#include "storage_manager.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Global scene manager instance
SceneManager sceneManager;

// External storage manager
extern StorageManager storageManager;

SceneManager::SceneManager() : _scenesCount(0), _activeScenesCount(0) {
    memset(_scenes, 0, sizeof(_scenes));
    memset(_activeScenes, 0, sizeof(_activeScenes));
}

bool SceneManager::begin() {
    Serial.println("[SCENES] Initializing Scene Manager...");
    
    // Load scenes from NVS
    if (loadScenes()) {
        Serial.printf("[SCENES] Loaded %d scenes from storage\n", _scenesCount);
    } else {
        Serial.println("[SCENES] No saved scenes found, starting fresh");
    }
    
    return true;
}

bool SceneManager::createScene(const char* name, const uint8_t* lightIndices, 
                               uint8_t lightsCount, const char* groupId) {
    if (_scenesCount >= MAX_SCENES_V2) {
        Serial.println("[SCENES] Maximum scenes reached");
        return false;
    }
    
    SceneV2& scene = _scenes[_scenesCount];
    
    // Generate UUID
    String uuid = generateSceneId();
    strcpy(scene.id, uuid.c_str());
    
    // Generate v1 compatible ID
    snprintf(scene.id_v1, sizeof(scene.id_v1), "%d", _scenesCount + 1);
    
    // Set name
    strncpy(scene.name, name, sizeof(scene.name) - 1);
    
    // Set type
    scene.type = SCENE_TYPE_LIGHT_SCENE;
    
    // Set group
    if (groupId) {
        strncpy(scene.group_id, groupId, sizeof(scene.group_id) - 1);
    } else {
        scene.group_id[0] = '\0';
    }
    
    // Initialize light states
    scene.lights_count = min(lightsCount, (uint8_t)MAX_LIGHTS);
    for (uint8_t i = 0; i < scene.lights_count; i++) {
        scene.light_states[i].light_index = lightIndices[i];
        // Initialize with default values
        scene.light_states[i].on = false;
        scene.light_states[i].bri = 254;
        scene.light_states[i].hue = 0;
        scene.light_states[i].sat = 0;
        scene.light_states[i].ct = 200;
        scene.light_states[i].xy[0] = 0.0;
        scene.light_states[i].xy[1] = 0.0;
        scene.light_states[i].colormode = 2; // CT
        scene.light_states[i].transition_time = 400; // 400ms default
    }
    
    // Initialize dynamic settings
    scene.dynamic.enabled = false;
    scene.dynamic.speed = 50;
    scene.dynamic.palette = PALETTE_SPRING_BLOSSOM;
    scene.dynamic.brightness = 254;
    scene.dynamic.update_interval = 100;
    
    // Set metadata
    scene.image_id[0] = '\0';
    scene.auto_dynamic = false;
    scene.recycle = false;
    scene.locked = false;
    scene.version = 1;
    scene.created_timestamp = millis();
    scene.modified_timestamp = millis();
    scene.app_data[0] = '\0';
    
    _scenesCount++;
    
    Serial.printf("[SCENES] Created scene: %s (ID: %s)\n", name, scene.id);
    
    // Save to NVS
    saveScenes();
    
    return true;
}

bool SceneManager::updateScene(const char* sceneId, const SceneLightState* states, uint8_t count) {
    int index = findSceneIndex(sceneId);
    if (index < 0) {
        Serial.printf("[SCENES] Scene not found: %s\n", sceneId);
        return false;
    }
    
    SceneV2& scene = _scenes[index];
    
    if (scene.locked) {
        Serial.printf("[SCENES] Scene is locked: %s\n", sceneId);
        return false;
    }
    
    // Update light states
    scene.lights_count = min(count, (uint8_t)MAX_LIGHTS);
    memcpy(scene.light_states, states, sizeof(SceneLightState) * scene.lights_count);
    
    // Update metadata
    scene.version++;
    scene.modified_timestamp = millis();
    
    Serial.printf("[SCENES] Updated scene: %s\n", scene.name);
    
    // Save to NVS
    saveScenes();
    
    return true;
}

bool SceneManager::deleteScene(const char* sceneId) {
    int index = findSceneIndex(sceneId);
    if (index < 0) {
        Serial.printf("[SCENES] Scene not found: %s\n", sceneId);
        return false;
    }
    
    SceneV2& scene = _scenes[index];
    
    if (scene.locked) {
        Serial.printf("[SCENES] Scene is locked: %s\n", sceneId);
        return false;
    }
    
    Serial.printf("[SCENES] Deleting scene: %s\n", scene.name);
    
    // Shift remaining scenes
    for (int i = index; i < _scenesCount - 1; i++) {
        _scenes[i] = _scenes[i + 1];
    }
    
    _scenesCount--;
    
    // Save to NVS
    saveScenes();
    
    return true;
}

SceneV2* SceneManager::getScene(const char* sceneId) {
    int index = findSceneIndex(sceneId);
    if (index < 0) {
        return nullptr;
    }
    return &_scenes[index];
}

SceneV2* SceneManager::getSceneByIndex(uint8_t index) {
    if (index >= _scenesCount) {
        return nullptr;
    }
    return &_scenes[index];
}

bool SceneManager::recallScene(const char* sceneId, uint16_t transitionTime) {
    int index = findSceneIndex(sceneId);
    if (index < 0) {
        Serial.printf("[SCENES] Scene not found: %s\n", sceneId);
        return false;
    }
    
    SceneV2& scene = _scenes[index];
    
    Serial.printf("[SCENES] Recalling scene: %s (transition: %dms)\n", 
                  scene.name, transitionTime);
    
    // Apply each light state
    bool success = true;
    for (uint8_t i = 0; i < scene.lights_count; i++) {
        SceneLightState& state = scene.light_states[i];
        state.transition_time = transitionTime; // Override transition time
        
        if (!applyLightState(state.light_index, state)) {
            Serial.printf("[SCENES] Failed to apply state to light %d\n", state.light_index);
            success = false;
        }
    }
    
    return success;
}

bool SceneManager::recallSceneForGroup(const char* sceneId, const char* groupId, 
                                       uint16_t transitionTime) {
    int index = findSceneIndex(sceneId);
    if (index < 0) {
        Serial.printf("[SCENES] Scene not found: %s\n", sceneId);
        return false;
    }
    
    SceneV2& scene = _scenes[index];
    
    // Verify scene belongs to group
    if (strlen(scene.group_id) > 0 && strcmp(scene.group_id, groupId) != 0) {
        Serial.printf("[SCENES] Scene does not belong to group: %s\n", groupId);
        return false;
    }
    
    return recallScene(sceneId, transitionTime);
}

bool SceneManager::captureScene(const char* sceneId, const uint8_t* lightIndices, 
                                uint8_t lightsCount) {
    int index = findSceneIndex(sceneId);
    if (index < 0) {
        Serial.printf("[SCENES] Scene not found: %s\n", sceneId);
        return false;
    }
    
    SceneV2& scene = _scenes[index];
    
    if (scene.locked) {
        Serial.printf("[SCENES] Scene is locked: %s\n", sceneId);
        return false;
    }
    
    Serial.printf("[SCENES] Capturing current state to scene: %s\n", scene.name);
    
    // Capture current state of each light
    scene.lights_count = min(lightsCount, (uint8_t)MAX_LIGHTS);
    for (uint8_t i = 0; i < scene.lights_count; i++) {
        uint8_t lightIdx = lightIndices[i];
        if (lightIdx >= lightsCount) continue;
        
        Light& light = lights[lightIdx];
        SceneLightState& state = scene.light_states[i];
        
        state.light_index = lightIdx;
        state.on = light.on;
        state.bri = light.bri;
        state.hue = light.hue;
        state.sat = light.sat;
        state.ct = light.ct;
        state.xy[0] = light.xy[0];
        state.xy[1] = light.xy[1];
        state.colormode = light.colormode;
        state.transition_time = 400;
    }
    
    // Update metadata
    scene.version++;
    scene.modified_timestamp = millis();
    
    // Save to NVS
    saveScenes();
    
    return true;
}

bool SceneManager::setDynamicScene(const char* sceneId, const DynamicSceneSettings& settings) {
    int index = findSceneIndex(sceneId);
    if (index < 0) {
        Serial.printf("[SCENES] Scene not found: %s\n", sceneId);
        return false;
    }
    
    SceneV2& scene = _scenes[index];
    scene.dynamic = settings;
    scene.type = SCENE_TYPE_DYNAMIC_SCENE;
    
    Serial.printf("[SCENES] Set dynamic settings for scene: %s\n", scene.name);
    
    // Save to NVS
    saveScenes();
    
    return true;
}

bool SceneManager::startDynamicScene(const char* sceneId) {
    if (_activeScenesCount >= 10) {
        Serial.println("[SCENES] Maximum active dynamic scenes reached");
        return false;
    }
    
    int index = findSceneIndex(sceneId);
    if (index < 0) {
        return false;
    }
    
    SceneV2& scene = _scenes[index];
    if (!scene.dynamic.enabled) {
        Serial.printf("[SCENES] Scene is not dynamic: %s\n", scene.name);
        return false;
    }
    
    // Add to active scenes
    ActiveDynamicScene& active = _activeScenes[_activeScenesCount++];
    strcpy(active.scene_id, sceneId);
    active.last_update = millis();
    active.animation_step = 0;
    active.active = true;
    
    Serial.printf("[SCENES] Started dynamic scene: %s\n", scene.name);
    
    return true;
}

bool SceneManager::stopDynamicScene(const char* sceneId) {
    // Find and remove from active scenes
    for (uint8_t i = 0; i < _activeScenesCount; i++) {
        if (strcmp(_activeScenes[i].scene_id, sceneId) == 0) {
            // Shift remaining scenes
            for (uint8_t j = i; j < _activeScenesCount - 1; j++) {
                _activeScenes[j] = _activeScenes[j + 1];
            }
            _activeScenesCount--;
            
            Serial.printf("[SCENES] Stopped dynamic scene: %s\n", sceneId);
            return true;
        }
    }
    
    return false;
}

void SceneManager::updateDynamicScenes() {
    unsigned long now = millis();
    
    for (uint8_t i = 0; i < _activeScenesCount; i++) {
        ActiveDynamicScene& active = _activeScenes[i];
        if (!active.active) continue;
        
        SceneV2* scene = getScene(active.scene_id);
        if (!scene) continue;
        
        // Check if it's time to update
        if (now - active.last_update >= scene->dynamic.update_interval) {
            updateDynamicScene(active);
            active.last_update = now;
        }
    }
}

uint8_t SceneManager::getScenesByGroup(const char* groupId, SceneV2** scenes, uint8_t maxScenes) {
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < _scenesCount && count < maxScenes; i++) {
        if (strcmp(_scenes[i].group_id, groupId) == 0) {
            scenes[count++] = &_scenes[i];
        }
    }
    
    return count;
}

uint8_t SceneManager::getScenesByType(SceneType type, SceneV2** scenes, uint8_t maxScenes) {
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < _scenesCount && count < maxScenes; i++) {
        if (_scenes[i].type == type) {
            scenes[count++] = &_scenes[i];
        }
    }
    
    return count;
}

bool SceneManager::saveScenes() {
    Serial.printf("[SCENES] Saving %d scenes to NVS...\n", _scenesCount);
    
    bool success = storageManager.saveArray("scenes", "scene", _scenes, _scenesCount, MAX_SCENES_V2);
    
    if (success) {
        Serial.println("[SCENES] Scenes saved successfully");
    } else {
        Serial.println("[SCENES] Failed to save scenes");
    }
    
    return success;
}

bool SceneManager::loadScenes() {
    Serial.println("[SCENES] Loading scenes from NVS...");
    
    bool success = storageManager.loadArray("scenes", "scene", _scenes, _scenesCount, MAX_SCENES_V2);
    
    if (success) {
        Serial.printf("[SCENES] Loaded %d scenes successfully\n", _scenesCount);
    } else {
        Serial.println("[SCENES] Failed to load scenes");
    }
    
    return success;
}

String SceneManager::generateSceneId() {
    // Generate UUID-like ID
    char uuid[37];
    snprintf(uuid, sizeof(uuid), "%08x-%04x-%04x-%04x-%012x",
             random(0xFFFFFFFF), random(0xFFFF), random(0xFFFF),
             random(0xFFFF), random(0xFFFFFFFF));
    return String(uuid);
}

bool SceneManager::sceneExists(const char* sceneId) {
    return findSceneIndex(sceneId) >= 0;
}

void SceneManager::printScenes() {
    Serial.println("[SCENES] ========================================");
    Serial.printf("[SCENES] Total Scenes: %d\n", _scenesCount);
    Serial.println("[SCENES] ========================================");
    
    for (uint8_t i = 0; i < _scenesCount; i++) {
        SceneV2& scene = _scenes[i];
        Serial.printf("[SCENES] %d. %s\n", i + 1, scene.name);
        Serial.printf("[SCENES]    ID: %s\n", scene.id);
        Serial.printf("[SCENES]    Type: %d\n", scene.type);
        Serial.printf("[SCENES]    Lights: %d\n", scene.lights_count);
        Serial.printf("[SCENES]    Dynamic: %s\n", scene.dynamic.enabled ? "Yes" : "No");
    }
    
    Serial.println("[SCENES] ========================================");
}

// Private helper methods

int SceneManager::findSceneIndex(const char* sceneId) {
    for (int i = 0; i < _scenesCount; i++) {
        if (strcmp(_scenes[i].id, sceneId) == 0 || 
            strcmp(_scenes[i].id_v1, sceneId) == 0) {
            return i;
        }
    }
    return -1;
}

bool SceneManager::applyLightState(uint8_t lightIndex, const SceneLightState& state) {
    if (lightIndex >= lightsCount) {
        return false;
    }
    
    Light& light = lights[lightIndex];
    
    // Build JSON payload for light
    StaticJsonDocument<512> doc;
    doc["on"] = state.on;
    doc["bri"] = state.bri;
    doc["transitiontime"] = state.transition_time / 100; // Convert ms to deciseconds
    
    if (state.colormode == 1) { // XY
        JsonArray xy = doc.createNestedArray("xy");
        xy.add(state.xy[0]);
        xy.add(state.xy[1]);
    } else if (state.colormode == 2) { // CT
        doc["ct"] = state.ct;
    } else if (state.colormode == 3) { // HS
        doc["hue"] = state.hue;
        doc["sat"] = state.sat;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    // Send to light
    HTTPClient http;
    String url = "http://" + light.ip.toString() + "/state";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.PUT(payload);
    http.end();
    
    if (httpCode == 200) {
        // Update local state
        light.on = state.on;
        light.bri = state.bri;
        light.hue = state.hue;
        light.sat = state.sat;
        light.ct = state.ct;
        light.xy[0] = state.xy[0];
        light.xy[1] = state.xy[1];
        light.colormode = state.colormode;
        
        return true;
    }
    
    return false;
}

void SceneManager::updateDynamicScene(ActiveDynamicScene& activeScene) {
    SceneV2* scene = getScene(activeScene.scene_id);
    if (!scene) return;
    
    // Calculate next color based on palette and step
    uint16_t hue;
    uint8_t sat, bri;
    calculateDynamicColor(scene->dynamic, activeScene.animation_step, hue, sat, bri);
    
    // Apply to all lights in scene
    for (uint8_t i = 0; i < scene->lights_count; i++) {
        SceneLightState& state = scene->light_states[i];
        state.hue = hue;
        state.sat = sat;
        state.bri = bri;
        state.colormode = 3; // HS mode
        state.transition_time = scene->dynamic.update_interval;
        
        applyLightState(state.light_index, state);
    }
    
    // Advance animation step
    activeScene.animation_step = (activeScene.animation_step + 1) % 360;
}

void SceneManager::calculateDynamicColor(const DynamicSceneSettings& settings, uint8_t step,
                                        uint16_t& hue, uint8_t& sat, uint8_t& bri) {
    // Calculate hue based on palette and step
    float progress = (float)step / 360.0;
    
    switch (settings.palette) {
        case PALETTE_SPRING_BLOSSOM:
            hue = 50000 + (uint16_t)(15000 * sin(progress * 2 * PI));
            sat = 200;
            break;
        case PALETTE_TROPICAL_TWILIGHT:
            hue = 10000 + (uint16_t)(20000 * sin(progress * 2 * PI));
            sat = 254;
            break;
        case PALETTE_NORDIC_AURORA:
            hue = 30000 + (uint16_t)(25000 * sin(progress * 2 * PI));
            sat = 180;
            break;
        case PALETTE_AUTUMN_GOLD:
            hue = 5000 + (uint16_t)(10000 * sin(progress * 2 * PI));
            sat = 220;
            break;
        case PALETTE_OCEAN_DAWN:
            hue = 40000 + (uint16_t)(15000 * sin(progress * 2 * PI));
            sat = 200;
            break;
        case PALETTE_BLOOD_MOON:
            hue = 0 + (uint16_t)(5000 * sin(progress * 2 * PI));
            sat = 254;
            break;
        default:
            hue = (uint16_t)(65535 * progress);
            sat = 254;
            break;
    }
    
    // Apply speed modifier
    float speedFactor = settings.speed / 100.0;
    hue = (uint16_t)(hue * speedFactor) % 65535;
    
    // Apply brightness
    bri = settings.brightness;
}
