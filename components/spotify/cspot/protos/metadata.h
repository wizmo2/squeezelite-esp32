// THIS CORNFILE IS GENERATED. DO NOT EDIT! 🌽
#ifndef _METADATAH
#define _METADATAH
#include <optional>
#include <vector>
enum class AudioFormat {
OGG_VORBIS_96 = 0,
OGG_VORBIS_160 = 1,
OGG_VORBIS_320 = 2,
MP3_256 = 3,
MP3_320 = 4,
MP3_160 = 5,
MP3_96 = 6,
MP3_160_ENC = 7,
AAC_24 = 8,
AAC_48 = 9,
};

class AudioFile {
public:
std::optional<std::vector<uint8_t>> file_id;
std::optional<AudioFormat> format;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassAudioFile;
};

class Restriction {
public:
std::optional<std::string> countries_allowed;
std::optional<std::string> countries_forbidden;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassRestriction;
};

class Image {
public:
std::optional<std::vector<uint8_t>> file_id;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassImage;
};

class ImageGroup {
public:
std::vector<Image> image;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassImageGroup;
};

class Album {
public:
std::optional<std::vector<uint8_t>> gid;
std::optional<std::string> name;
std::optional<ImageGroup> cover_group;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassAlbum;
};

class Artist {
public:
std::optional<std::vector<uint8_t>> gid;
std::optional<std::string> name;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassArtist;
};

class Track {
public:
std::optional<std::vector<uint8_t>> gid;
std::optional<std::string> name;
std::optional<Album> album;
std::vector<Artist> artist;
std::optional<int32_t> duration;
std::vector<Restriction> restriction;
std::vector<AudioFile> file;
std::vector<Track> alternative;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassTrack;
};

class Episode {
public:
std::optional<std::vector<uint8_t>> gid;
std::optional<std::string> name;
std::optional<int32_t> duration;
std::vector<AudioFile> audio;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassEpisode;
};

#endif
