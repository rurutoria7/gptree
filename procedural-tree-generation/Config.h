#pragma once

static const uint PersistentConfigOffset = 4;

enum class PersistentConfig : uint {
    TREE_TYPE = 0,
    TREE_ATTRACTION_UP,
    SEASON,
    WIND_STRENGTH,
    SEED,

    CAMERA_YAW,
    CAMERA_PITCH,
    CAMERA_DISTANCE,

    TIME,
    MOUSE_X,
    MOUSE_Y,
    KEY_SPACE_DOWN,
};

float LoadPersistentConfigFloat(in PersistentConfig config) {
    return PersistentScratchBuffer.Load<float>(PersistentConfigOffset + ((uint)config) * sizeof(uint));
}

uint LoadPersistentConfigUint(in PersistentConfig config) {
    return PersistentScratchBuffer.Load<uint>(PersistentConfigOffset + ((uint)config) * sizeof(uint));
}

void StorePersistentConfig(in PersistentConfig config, in float value) {
    PersistentScratchBuffer.Store<float>(PersistentConfigOffset + ((uint)config) * sizeof(uint), value);
}

void StorePersistentConfig(in PersistentConfig config, in uint value) {
    PersistentScratchBuffer.Store<uint>(PersistentConfigOffset + ((uint)config) * sizeof(uint), value);
}

float GetSeason() {
    return LoadPersistentConfigFloat(PersistentConfig::SEASON);
}

float GetWindStrength() {
    return LoadPersistentConfigFloat(PersistentConfig::WIND_STRENGTH);
}

float GetWindDirection() {
    return 0;
}
