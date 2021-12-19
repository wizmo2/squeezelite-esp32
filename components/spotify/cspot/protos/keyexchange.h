// THIS CORNFILE IS GENERATED. DO NOT EDIT! 🌽
#ifndef _KEYEXCHANGEH
#define _KEYEXCHANGEH
#include <vector>
#include <optional>
enum class Product {
PRODUCT_CLIENT = 0,
PRODUCT_LIBSPOTIFY = 1,
PRODUCT_MOBILE = 2,
PRODUCT_PARTNER = 3,
PRODUCT_LIBSPOTIFY_EMBEDDED = 5,
};

enum class Platform {
PLATFORM_WIN32_X86 = 0,
PLATFORM_OSX_X86 = 1,
PLATFORM_LINUX_X86 = 2,
PLATFORM_IPHONE_ARM = 3,
PLATFORM_S60_ARM = 4,
PLATFORM_OSX_PPC = 5,
PLATFORM_ANDROID_ARM = 6,
PLATFORM_WINDOWS_CE_ARM = 7,
PLATFORM_LINUX_X86_64 = 8,
PLATFORM_OSX_X86_64 = 9,
PLATFORM_PALM_ARM = 10,
PLATFORM_LINUX_SH = 11,
PLATFORM_FREEBSD_X86 = 12,
PLATFORM_FREEBSD_X86_64 = 13,
PLATFORM_BLACKBERRY_ARM = 14,
PLATFORM_SONOS = 15,
PLATFORM_LINUX_MIPS = 16,
PLATFORM_LINUX_ARM = 17,
PLATFORM_LOGITECH_ARM = 18,
PLATFORM_LINUX_BLACKFIN = 19,
PLATFORM_WP7_ARM = 20,
PLATFORM_ONKYO_ARM = 21,
PLATFORM_QNXNTO_ARM = 22,
PLATFORM_BCO_ARM = 23,
};

enum class Cryptosuite {
CRYPTO_SUITE_SHANNON = 0,
CRYPTO_SUITE_RC4_SHA1_HMAC = 1,
};

class LoginCryptoDiffieHellmanChallenge {
public:
std::vector<uint8_t> gs;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassLoginCryptoDiffieHellmanChallenge;
};

class LoginCryptoChallengeUnion {
public:
std::optional<LoginCryptoDiffieHellmanChallenge> diffie_hellman;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassLoginCryptoChallengeUnion;
};

class LoginCryptoDiffieHellmanHello {
public:
std::vector<uint8_t> gc;
uint32_t server_keys_known;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassLoginCryptoDiffieHellmanHello;
};

class LoginCryptoHelloUnion {
public:
std::optional<LoginCryptoDiffieHellmanHello> diffie_hellman;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassLoginCryptoHelloUnion;
};

class BuildInfo {
public:
Product product;
Platform platform;
uint64_t version;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassBuildInfo;
};

class FeatureSet {
public:
std::optional<bool> autoupdate2;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassFeatureSet;
};

class APChallenge {
public:
LoginCryptoChallengeUnion login_crypto_challenge;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassAPChallenge;
};

class APResponseMessage {
public:
std::optional<APChallenge> challenge;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassAPResponseMessage;
};

class LoginCryptoDiffieHellmanResponse {
public:
std::vector<uint8_t> hmac;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassLoginCryptoDiffieHellmanResponse;
};

class LoginCryptoResponseUnion {
public:
std::optional<LoginCryptoDiffieHellmanResponse> diffie_hellman;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassLoginCryptoResponseUnion;
};

class CryptoResponseUnion {
public:
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassCryptoResponseUnion;
};

class PoWResponseUnion {
public:
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassPoWResponseUnion;
};

class ClientResponsePlaintext {
public:
LoginCryptoResponseUnion login_crypto_response;
PoWResponseUnion pow_response;
CryptoResponseUnion crypto_response;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassClientResponsePlaintext;
};

class ClientHello {
public:
BuildInfo build_info;
LoginCryptoHelloUnion login_crypto_hello;
std::vector<Cryptosuite> cryptosuites_supported;
std::vector<uint8_t> client_nonce;
std::optional<std::vector<uint8_t>> padding;
std::optional<FeatureSet> feature_set;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassClientHello;
};

#endif
