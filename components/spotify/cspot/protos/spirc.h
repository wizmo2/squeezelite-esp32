// THIS CORNFILE IS GENERATED. DO NOT EDIT! 🌽
#ifndef _SPIRCH
#define _SPIRCH
#include <optional>
#include <vector>
enum class MessageType {
kMessageTypeHello = 1,
kMessageTypeGoodbye = 2,
kMessageTypeProbe = 3,
kMessageTypeNotify = 10,
kMessageTypeLoad = 20,
kMessageTypePlay = 21,
kMessageTypePause = 22,
kMessageTypePlayPause = 23,
kMessageTypeSeek = 24,
kMessageTypePrev = 25,
kMessageTypeNext = 26,
kMessageTypeVolume = 27,
kMessageTypeShuffle = 28,
kMessageTypeRepeat = 29,
kMessageTypeVolumeDown = 31,
kMessageTypeVolumeUp = 32,
kMessageTypeReplace = 33,
kMessageTypeLogout = 34,
kMessageTypeAction = 35,
};

enum class PlayStatus {
kPlayStatusStop = 0,
kPlayStatusPlay = 1,
kPlayStatusPause = 2,
kPlayStatusLoading = 3,
};

enum class CapabilityType {
kSupportedContexts = 1,
kCanBePlayer = 2,
kRestrictToLocal = 3,
kDeviceType = 4,
kGaiaEqConnectId = 5,
kSupportsLogout = 6,
kIsObservable = 7,
kVolumeSteps = 8,
kSupportedTypes = 9,
kCommandAcks = 10,
};

class TrackRef {
public:
std::optional<std::vector<uint8_t>> gid;
std::optional<std::string> uri;
std::optional<bool> queued;
std::optional<std::string> context;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassTrackRef;
};

class State {
public:
std::optional<std::string> context_uri;
std::optional<uint32_t> index;
std::optional<uint32_t> position_ms;
std::optional<PlayStatus> status;
std::optional<uint64_t> position_measured_at;
std::optional<std::string> context_description;
std::optional<bool> shuffle;
std::optional<bool> repeat;
std::optional<uint32_t> playing_track_index;
std::vector<TrackRef> track;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassState;
};

class Capability {
public:
std::optional<CapabilityType> typ;
std::vector<int64_t> intValue;
std::vector<std::string> stringValue;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassCapability;
};

class DeviceState {
public:
std::optional<std::string> sw_version;
std::optional<bool> is_active;
std::optional<bool> can_play;
std::optional<uint32_t> volume;
std::optional<std::string> name;
std::optional<uint32_t> error_code;
std::optional<int64_t> became_active_at;
std::optional<std::string> error_message;
std::vector<Capability> capabilities;
std::vector<std::string> local_uris;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassDeviceState;
};

class Frame {
public:
std::optional<uint32_t> version;
std::optional<std::string> ident;
std::optional<std::string> protocol_version;
std::optional<uint32_t> seq_nr;
std::optional<MessageType> typ;
std::optional<DeviceState> device_state;
std::optional<State> state;
std::optional<uint32_t> position;
std::optional<uint32_t> volume;
std::optional<int64_t> state_update_id;
std::vector<std::string> recipient;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassFrame;
};

#endif
