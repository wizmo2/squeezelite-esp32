// THIS CORNFILE IS GENERATED. DO NOT EDIT! 🌽
#ifndef __REFLECTION_INTERNALH
#define __REFLECTION_INTERNALH
#include <string>
#include <vector>
enum class ReflectTypeID {
EnumReflectTypeID = 0,
ClassReflectField = 1,
ClassReflectEnumValue = 2,
ClassReflectType = 3,
EnumReflectTypeKind = 4,
VectorOfClassReflectField = 5,
VectorOfClassReflectEnumValue = 6,
Double = 7,
Uint32 = 8,
Char = 9,
UnsignedChar = 10,
Float = 11,
Bool = 12,
String = 13,
Int32 = 14,
Int64 = 15,
Int = 16,
Uint8 = 17,
Uint64 = 18,
VectorOfUint8 = 19,
EnumCpuFamily = 20,
EnumOs = 21,
EnumAuthenticationType = 22,
ClassSystemInfo = 23,
ClassLoginCredentials = 24,
ClassClientResponseEncrypted = 25,
OptionalOfString = 26,
OptionalOfVectorOfUint8 = 27,
EnumProduct = 28,
EnumPlatform = 29,
EnumCryptosuite = 30,
ClassLoginCryptoDiffieHellmanChallenge = 31,
ClassLoginCryptoChallengeUnion = 32,
ClassLoginCryptoDiffieHellmanHello = 33,
ClassLoginCryptoHelloUnion = 34,
ClassBuildInfo = 35,
ClassFeatureSet = 36,
ClassAPChallenge = 37,
ClassAPResponseMessage = 38,
ClassLoginCryptoDiffieHellmanResponse = 39,
ClassLoginCryptoResponseUnion = 40,
ClassCryptoResponseUnion = 41,
ClassPoWResponseUnion = 42,
ClassClientResponsePlaintext = 43,
ClassClientHello = 44,
OptionalOfClassLoginCryptoDiffieHellmanChallenge = 45,
OptionalOfClassLoginCryptoDiffieHellmanHello = 46,
OptionalOfBool = 47,
OptionalOfClassAPChallenge = 48,
OptionalOfClassLoginCryptoDiffieHellmanResponse = 49,
VectorOfEnumCryptosuite = 50,
OptionalOfClassFeatureSet = 51,
ClassHeader = 52,
EnumAudioFormat = 53,
ClassAudioFile = 54,
ClassRestriction = 55,
ClassImage = 56,
ClassImageGroup = 57,
ClassAlbum = 58,
ClassArtist = 59,
ClassTrack = 60,
ClassEpisode = 61,
OptionalOfEnumAudioFormat = 62,
VectorOfClassImage = 63,
OptionalOfClassImageGroup = 64,
OptionalOfClassAlbum = 65,
VectorOfClassArtist = 66,
OptionalOfInt32 = 67,
VectorOfClassRestriction = 68,
VectorOfClassAudioFile = 69,
VectorOfClassTrack = 70,
EnumMessageType = 71,
EnumPlayStatus = 72,
EnumCapabilityType = 73,
ClassTrackRef = 74,
ClassState = 75,
ClassCapability = 76,
ClassDeviceState = 77,
ClassFrame = 78,
OptionalOfUint32 = 79,
OptionalOfEnumPlayStatus = 80,
OptionalOfUint64 = 81,
VectorOfClassTrackRef = 82,
OptionalOfEnumCapabilityType = 83,
VectorOfInt64 = 84,
VectorOfString = 85,
OptionalOfInt64 = 86,
VectorOfClassCapability = 87,
OptionalOfEnumMessageType = 88,
OptionalOfClassDeviceState = 89,
OptionalOfClassState = 90,
};

enum class ReflectTypeKind {
Primitive = 0,
Enum = 1,
Class = 2,
Vector = 3,
Optional = 4,
};

class ReflectField {
public:
ReflectTypeID typeID;
std::string name;
size_t offset;
uint32_t protobufTag;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassReflectField;

			ReflectField() {};
			ReflectField(ReflectTypeID typeID, std::string name, size_t offset, uint32_t protobufTag) {
				this->typeID = typeID;
				this->name = name;
				this->offset = offset;
				this->protobufTag = protobufTag;
			}
		};

class ReflectEnumValue {
public:
std::string name;
int value;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassReflectEnumValue;

			ReflectEnumValue(){};
			ReflectEnumValue( std::string name, int value) {
				this->name = name;
				this->value = value;
			}
		};

class ReflectType {
public:
ReflectTypeID typeID;
std::string name;
ReflectTypeKind kind;
size_t size;
ReflectTypeID innerType;
std::vector<ReflectField> fields;
std::vector<ReflectEnumValue> enumValues;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassReflectType;

		void (*_Construct)(void *mem);
		void (*_Destruct)(void *obj);
		VectorOperations vectorOps;
		OptionalOperations optionalOps;
		static ReflectType ofPrimitive(ReflectTypeID id, std::string name, size_t size) {
			ReflectType t;
			t.kind = ReflectTypeKind::Primitive;
			t.typeID = id;
			t.name = name;
			t.size = size;
			return t;
		}
		static ReflectType ofEnum(ReflectTypeID id, std::string name, std::vector<ReflectEnumValue> enumValues, size_t size) {
			ReflectType t;
			t.kind = ReflectTypeKind::Enum;
			t.typeID = id;
			t.name = name;
			t.size = size;
			t.enumValues = enumValues;
			return t;
		}
		static ReflectType ofVector(ReflectTypeID id, ReflectTypeID innerType, size_t size, 
			VectorOperations vectorOps,
			void (*_Construct)(void *mem), void (*_Destruct)(void *obj)) {
			ReflectType t;
			t.kind = ReflectTypeKind::Vector;
			t.typeID = id;
			t.innerType = innerType;
			t.size = size;
			t._Construct = _Construct;
			t._Destruct = _Destruct;
			t.vectorOps = vectorOps;
			return t;
		}
		static ReflectType ofOptional(ReflectTypeID id, ReflectTypeID innerType, size_t size, 
			OptionalOperations optionalOps,
			void (*_Construct)(void *mem), void (*_Destruct)(void *obj)) {
			ReflectType t;
			t.kind = ReflectTypeKind::Optional;
			t.typeID = id;
			t.innerType = innerType;
			t.size = size;
			t._Construct = _Construct;
			t._Destruct = _Destruct;
			t.optionalOps = optionalOps;
			return t;
		}
		static ReflectType ofClass(ReflectTypeID id, std::string name, std::vector<ReflectField> fields, size_t size, void (*_Construct)(void *mem), void (*_Destruct)(void *obj)) {
			ReflectType t;
			t.kind = ReflectTypeKind::Class;
			t.name = name;
			t.typeID = id;
			t.size = size;
			t.fields = std::move(fields);
			t._Construct = _Construct;
			t._Destruct = _Destruct;
			return t;
		}
		
		};

#endif
