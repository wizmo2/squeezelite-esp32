// THIS CORNFILE IS GENERATED. DO NOT EDIT! 🌽
#ifndef _AUTHENTICATIONH
#define _AUTHENTICATIONH
#include <optional>
#include <vector>
enum class CpuFamily {
CPU_UNKNOWN = 0,
CPU_X86 = 1,
CPU_X86_64 = 2,
CPU_PPC = 3,
CPU_PPC_64 = 4,
CPU_ARM = 5,
CPU_IA64 = 6,
CPU_SH = 7,
CPU_MIPS = 8,
CPU_BLACKFIN = 9,
};

enum class Os {
OS_UNKNOWN = 0,
OS_WINDOWS = 1,
OS_OSX = 2,
OS_IPHONE = 3,
OS_S60 = 4,
OS_LINUX = 5,
OS_WINDOWS_CE = 6,
OS_ANDROID = 7,
OS_PALM = 8,
OS_FREEBSD = 9,
OS_BLACKBERRY = 10,
OS_SONOS = 11,
OS_LOGITECH = 12,
OS_WP7 = 13,
OS_ONKYO = 14,
OS_PHILIPS = 15,
OS_WD = 16,
OS_VOLVO = 17,
OS_TIVO = 18,
OS_AWOX = 19,
OS_MEEGO = 20,
OS_QNXNTO = 21,
OS_BCO = 22,
};

enum class AuthenticationType {
AUTHENTICATION_USER_PASS = 0,
AUTHENTICATION_STORED_SPOTIFY_CREDENTIALS = 1,
AUTHENTICATION_STORED_FACEBOOK_CREDENTIALS = 2,
AUTHENTICATION_SPOTIFY_TOKEN = 3,
AUTHENTICATION_FACEBOOK_TOKEN = 4,
};

class SystemInfo {
public:
CpuFamily cpu_family;
Os os;
std::optional<std::string> system_information_string;
std::optional<std::string> device_id;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassSystemInfo;
};

class LoginCredentials {
public:
std::optional<std::string> username;
AuthenticationType typ;
std::optional<std::vector<uint8_t>> auth_data;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassLoginCredentials;
};

class ClientResponseEncrypted {
public:
LoginCredentials login_credentials;
SystemInfo system_info;
std::optional<std::string> version_string;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassClientResponseEncrypted;
};

#endif
