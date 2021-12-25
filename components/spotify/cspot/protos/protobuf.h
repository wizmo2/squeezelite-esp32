// THIS CORNFILE IS GENERATED. DO NOT EDIT! 🌽
#ifndef _PROTOBUFH
#define _PROTOBUFH
#include <utility>
#include <vector>
#include <type_traits>
#include <string>
#include <optional>

		class AnyRef;
		struct VectorOperations {
			void (*push_back)(AnyRef &vec, AnyRef &val);
			AnyRef (*at)(AnyRef &vec, size_t index);
			size_t (*size)(AnyRef &vec);
			void (*emplace_back)(AnyRef &vec);
			void (*clear)(AnyRef &vec);
			void (*reserve)(AnyRef &vec, size_t n);
		};
		
	

		class AnyRef;
		struct OptionalOperations {
			AnyRef (*get)(AnyRef &opt);
			bool (*has_value)(AnyRef &opt);
			void (*set)(AnyRef &opt, AnyRef &val);
			void (*reset)(AnyRef &opt);
			void (*emplaceEmpty)(AnyRef &opt);
		};
		
	
#include "ReflectionInternal.h"
#include "authentication.h"
#include "keyexchange.h"
#include "metadata.h"
#include "spirc.h"
#include "mercury.h"

		template<class T>
		void __reflectConstruct(void *mem) {
			new(mem) T;
		}
		template<class T>
		void __reflectDestruct(void *obj) {
			((T*) obj)->~T();
		}
		
	
class ReflectField;
class ReflectEnumValue;
class ReflectType;
class SystemInfo;
class LoginCredentials;
class ClientResponseEncrypted;
class LoginCryptoDiffieHellmanChallenge;
class LoginCryptoChallengeUnion;
class LoginCryptoDiffieHellmanHello;
class LoginCryptoHelloUnion;
class BuildInfo;
class FeatureSet;
class APChallenge;
class APResponseMessage;
class LoginCryptoDiffieHellmanResponse;
class LoginCryptoResponseUnion;
class CryptoResponseUnion;
class PoWResponseUnion;
class ClientResponsePlaintext;
class ClientHello;
class Header;
class AudioFile;
class Restriction;
class Image;
class ImageGroup;
class Album;
class Artist;
class Track;
class Episode;
class TrackRef;
class State;
class Capability;
class DeviceState;
class Frame;


	
	class AnyRef {
		public:
				ReflectTypeID typeID;
				AnyRef() {};
				AnyRef(ReflectTypeID typeID, void *obj) {
					this->typeID = typeID;
					this->value.voidptr = obj;
				}
				template<typename T>
				T *as() {
					// if(T::_TYPE_ID != this->typeID) {
					// 	throw "invalid as call";
					// }
					return (T*) this->value.voidptr;
				}

				template<typename T>
				bool is() {
					if constexpr(std::is_same<T, ReflectTypeID>::value) {
						return ReflectTypeID::EnumReflectTypeID == this->typeID;
					} else 
					if constexpr(std::is_same<T, ReflectTypeKind>::value) {
						return ReflectTypeID::EnumReflectTypeKind == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<ReflectField>>::value) {
						return ReflectTypeID::VectorOfClassReflectField == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<ReflectEnumValue>>::value) {
						return ReflectTypeID::VectorOfClassReflectEnumValue == this->typeID;
					} else 
					if constexpr(std::is_same<T, int32_t>::value) {
						return ReflectTypeID::Int32 == this->typeID;
					} else 
					if constexpr(std::is_same<T, int64_t>::value) {
						return ReflectTypeID::Int64 == this->typeID;
					} else 
					if constexpr(std::is_same<T, uint32_t>::value) {
						return ReflectTypeID::Uint32 == this->typeID;
					} else 
					if constexpr(std::is_same<T, uint8_t>::value) {
						return ReflectTypeID::Uint8 == this->typeID;
					} else 
					if constexpr(std::is_same<T, int>::value) {
						return ReflectTypeID::Int == this->typeID;
					} else 
					if constexpr(std::is_same<T, unsigned char>::value) {
						return ReflectTypeID::UnsignedChar == this->typeID;
					} else 
					if constexpr(std::is_same<T, float>::value) {
						return ReflectTypeID::Float == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::string>::value) {
						return ReflectTypeID::String == this->typeID;
					} else 
					if constexpr(std::is_same<T, uint64_t>::value) {
						return ReflectTypeID::Uint64 == this->typeID;
					} else 
					if constexpr(std::is_same<T, char>::value) {
						return ReflectTypeID::Char == this->typeID;
					} else 
					if constexpr(std::is_same<T, double>::value) {
						return ReflectTypeID::Double == this->typeID;
					} else 
					if constexpr(std::is_same<T, bool>::value) {
						return ReflectTypeID::Bool == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<uint8_t>>::value) {
						return ReflectTypeID::VectorOfUint8 == this->typeID;
					} else 
					if constexpr(std::is_same<T, CpuFamily>::value) {
						return ReflectTypeID::EnumCpuFamily == this->typeID;
					} else 
					if constexpr(std::is_same<T, Os>::value) {
						return ReflectTypeID::EnumOs == this->typeID;
					} else 
					if constexpr(std::is_same<T, AuthenticationType>::value) {
						return ReflectTypeID::EnumAuthenticationType == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<std::string>>::value) {
						return ReflectTypeID::OptionalOfString == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<std::vector<uint8_t>>>::value) {
						return ReflectTypeID::OptionalOfVectorOfUint8 == this->typeID;
					} else 
					if constexpr(std::is_same<T, Product>::value) {
						return ReflectTypeID::EnumProduct == this->typeID;
					} else 
					if constexpr(std::is_same<T, Platform>::value) {
						return ReflectTypeID::EnumPlatform == this->typeID;
					} else 
					if constexpr(std::is_same<T, Cryptosuite>::value) {
						return ReflectTypeID::EnumCryptosuite == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<LoginCryptoDiffieHellmanChallenge>>::value) {
						return ReflectTypeID::OptionalOfClassLoginCryptoDiffieHellmanChallenge == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<LoginCryptoDiffieHellmanHello>>::value) {
						return ReflectTypeID::OptionalOfClassLoginCryptoDiffieHellmanHello == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<bool>>::value) {
						return ReflectTypeID::OptionalOfBool == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<APChallenge>>::value) {
						return ReflectTypeID::OptionalOfClassAPChallenge == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<LoginCryptoDiffieHellmanResponse>>::value) {
						return ReflectTypeID::OptionalOfClassLoginCryptoDiffieHellmanResponse == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<Cryptosuite>>::value) {
						return ReflectTypeID::VectorOfEnumCryptosuite == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<FeatureSet>>::value) {
						return ReflectTypeID::OptionalOfClassFeatureSet == this->typeID;
					} else 
					if constexpr(std::is_same<T, AudioFormat>::value) {
						return ReflectTypeID::EnumAudioFormat == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<AudioFormat>>::value) {
						return ReflectTypeID::OptionalOfEnumAudioFormat == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<Image>>::value) {
						return ReflectTypeID::VectorOfClassImage == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<ImageGroup>>::value) {
						return ReflectTypeID::OptionalOfClassImageGroup == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<Album>>::value) {
						return ReflectTypeID::OptionalOfClassAlbum == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<Artist>>::value) {
						return ReflectTypeID::VectorOfClassArtist == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<int32_t>>::value) {
						return ReflectTypeID::OptionalOfInt32 == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<Restriction>>::value) {
						return ReflectTypeID::VectorOfClassRestriction == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<AudioFile>>::value) {
						return ReflectTypeID::VectorOfClassAudioFile == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<Track>>::value) {
						return ReflectTypeID::VectorOfClassTrack == this->typeID;
					} else 
					if constexpr(std::is_same<T, MessageType>::value) {
						return ReflectTypeID::EnumMessageType == this->typeID;
					} else 
					if constexpr(std::is_same<T, PlayStatus>::value) {
						return ReflectTypeID::EnumPlayStatus == this->typeID;
					} else 
					if constexpr(std::is_same<T, CapabilityType>::value) {
						return ReflectTypeID::EnumCapabilityType == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<uint32_t>>::value) {
						return ReflectTypeID::OptionalOfUint32 == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<PlayStatus>>::value) {
						return ReflectTypeID::OptionalOfEnumPlayStatus == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<uint64_t>>::value) {
						return ReflectTypeID::OptionalOfUint64 == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<TrackRef>>::value) {
						return ReflectTypeID::VectorOfClassTrackRef == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<CapabilityType>>::value) {
						return ReflectTypeID::OptionalOfEnumCapabilityType == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<int64_t>>::value) {
						return ReflectTypeID::VectorOfInt64 == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<std::string>>::value) {
						return ReflectTypeID::VectorOfString == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<int64_t>>::value) {
						return ReflectTypeID::OptionalOfInt64 == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::vector<Capability>>::value) {
						return ReflectTypeID::VectorOfClassCapability == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<MessageType>>::value) {
						return ReflectTypeID::OptionalOfEnumMessageType == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<DeviceState>>::value) {
						return ReflectTypeID::OptionalOfClassDeviceState == this->typeID;
					} else 
					if constexpr(std::is_same<T, std::optional<State>>::value) {
						return ReflectTypeID::OptionalOfClassState == this->typeID;
					} else 
					 {
						return T::_TYPE_ID == this->typeID;
					}
				}
				

				ReflectType *reflectType();
				AnyRef getField(int i);
				template <typename T>
				static AnyRef of(T *obj)
				{
					ReflectTypeID typeID;
					if constexpr(std::is_same<T, ReflectTypeID>::value) {
						typeID = ReflectTypeID::EnumReflectTypeID;
					} else 
					if constexpr(std::is_same<T, ReflectTypeKind>::value) {
						typeID = ReflectTypeID::EnumReflectTypeKind;
					} else 
					if constexpr(std::is_same<T, std::vector<ReflectField>>::value) {
						typeID = ReflectTypeID::VectorOfClassReflectField;
					} else 
					if constexpr(std::is_same<T, std::vector<ReflectEnumValue>>::value) {
						typeID = ReflectTypeID::VectorOfClassReflectEnumValue;
					} else 
					if constexpr(std::is_same<T, int32_t>::value) {
						typeID = ReflectTypeID::Int32;
					} else 
					if constexpr(std::is_same<T, int64_t>::value) {
						typeID = ReflectTypeID::Int64;
					} else 
					if constexpr(std::is_same<T, uint32_t>::value) {
						typeID = ReflectTypeID::Uint32;
					} else 
					if constexpr(std::is_same<T, uint8_t>::value) {
						typeID = ReflectTypeID::Uint8;
					} else 
					if constexpr(std::is_same<T, int>::value) {
						typeID = ReflectTypeID::Int;
					} else 
					if constexpr(std::is_same<T, unsigned char>::value) {
						typeID = ReflectTypeID::UnsignedChar;
					} else 
					if constexpr(std::is_same<T, float>::value) {
						typeID = ReflectTypeID::Float;
					} else 
					if constexpr(std::is_same<T, std::string>::value) {
						typeID = ReflectTypeID::String;
					} else 
					if constexpr(std::is_same<T, uint64_t>::value) {
						typeID = ReflectTypeID::Uint64;
					} else 
					if constexpr(std::is_same<T, char>::value) {
						typeID = ReflectTypeID::Char;
					} else 
					if constexpr(std::is_same<T, double>::value) {
						typeID = ReflectTypeID::Double;
					} else 
					if constexpr(std::is_same<T, bool>::value) {
						typeID = ReflectTypeID::Bool;
					} else 
					if constexpr(std::is_same<T, std::vector<uint8_t>>::value) {
						typeID = ReflectTypeID::VectorOfUint8;
					} else 
					if constexpr(std::is_same<T, CpuFamily>::value) {
						typeID = ReflectTypeID::EnumCpuFamily;
					} else 
					if constexpr(std::is_same<T, Os>::value) {
						typeID = ReflectTypeID::EnumOs;
					} else 
					if constexpr(std::is_same<T, AuthenticationType>::value) {
						typeID = ReflectTypeID::EnumAuthenticationType;
					} else 
					if constexpr(std::is_same<T, std::optional<std::string>>::value) {
						typeID = ReflectTypeID::OptionalOfString;
					} else 
					if constexpr(std::is_same<T, std::optional<std::vector<uint8_t>>>::value) {
						typeID = ReflectTypeID::OptionalOfVectorOfUint8;
					} else 
					if constexpr(std::is_same<T, Product>::value) {
						typeID = ReflectTypeID::EnumProduct;
					} else 
					if constexpr(std::is_same<T, Platform>::value) {
						typeID = ReflectTypeID::EnumPlatform;
					} else 
					if constexpr(std::is_same<T, Cryptosuite>::value) {
						typeID = ReflectTypeID::EnumCryptosuite;
					} else 
					if constexpr(std::is_same<T, std::optional<LoginCryptoDiffieHellmanChallenge>>::value) {
						typeID = ReflectTypeID::OptionalOfClassLoginCryptoDiffieHellmanChallenge;
					} else 
					if constexpr(std::is_same<T, std::optional<LoginCryptoDiffieHellmanHello>>::value) {
						typeID = ReflectTypeID::OptionalOfClassLoginCryptoDiffieHellmanHello;
					} else 
					if constexpr(std::is_same<T, std::optional<bool>>::value) {
						typeID = ReflectTypeID::OptionalOfBool;
					} else 
					if constexpr(std::is_same<T, std::optional<APChallenge>>::value) {
						typeID = ReflectTypeID::OptionalOfClassAPChallenge;
					} else 
					if constexpr(std::is_same<T, std::optional<LoginCryptoDiffieHellmanResponse>>::value) {
						typeID = ReflectTypeID::OptionalOfClassLoginCryptoDiffieHellmanResponse;
					} else 
					if constexpr(std::is_same<T, std::vector<Cryptosuite>>::value) {
						typeID = ReflectTypeID::VectorOfEnumCryptosuite;
					} else 
					if constexpr(std::is_same<T, std::optional<FeatureSet>>::value) {
						typeID = ReflectTypeID::OptionalOfClassFeatureSet;
					} else 
					if constexpr(std::is_same<T, AudioFormat>::value) {
						typeID = ReflectTypeID::EnumAudioFormat;
					} else 
					if constexpr(std::is_same<T, std::optional<AudioFormat>>::value) {
						typeID = ReflectTypeID::OptionalOfEnumAudioFormat;
					} else 
					if constexpr(std::is_same<T, std::vector<Image>>::value) {
						typeID = ReflectTypeID::VectorOfClassImage;
					} else 
					if constexpr(std::is_same<T, std::optional<ImageGroup>>::value) {
						typeID = ReflectTypeID::OptionalOfClassImageGroup;
					} else 
					if constexpr(std::is_same<T, std::optional<Album>>::value) {
						typeID = ReflectTypeID::OptionalOfClassAlbum;
					} else 
					if constexpr(std::is_same<T, std::vector<Artist>>::value) {
						typeID = ReflectTypeID::VectorOfClassArtist;
					} else 
					if constexpr(std::is_same<T, std::optional<int32_t>>::value) {
						typeID = ReflectTypeID::OptionalOfInt32;
					} else 
					if constexpr(std::is_same<T, std::vector<Restriction>>::value) {
						typeID = ReflectTypeID::VectorOfClassRestriction;
					} else 
					if constexpr(std::is_same<T, std::vector<AudioFile>>::value) {
						typeID = ReflectTypeID::VectorOfClassAudioFile;
					} else 
					if constexpr(std::is_same<T, std::vector<Track>>::value) {
						typeID = ReflectTypeID::VectorOfClassTrack;
					} else 
					if constexpr(std::is_same<T, MessageType>::value) {
						typeID = ReflectTypeID::EnumMessageType;
					} else 
					if constexpr(std::is_same<T, PlayStatus>::value) {
						typeID = ReflectTypeID::EnumPlayStatus;
					} else 
					if constexpr(std::is_same<T, CapabilityType>::value) {
						typeID = ReflectTypeID::EnumCapabilityType;
					} else 
					if constexpr(std::is_same<T, std::optional<uint32_t>>::value) {
						typeID = ReflectTypeID::OptionalOfUint32;
					} else 
					if constexpr(std::is_same<T, std::optional<PlayStatus>>::value) {
						typeID = ReflectTypeID::OptionalOfEnumPlayStatus;
					} else 
					if constexpr(std::is_same<T, std::optional<uint64_t>>::value) {
						typeID = ReflectTypeID::OptionalOfUint64;
					} else 
					if constexpr(std::is_same<T, std::vector<TrackRef>>::value) {
						typeID = ReflectTypeID::VectorOfClassTrackRef;
					} else 
					if constexpr(std::is_same<T, std::optional<CapabilityType>>::value) {
						typeID = ReflectTypeID::OptionalOfEnumCapabilityType;
					} else 
					if constexpr(std::is_same<T, std::vector<int64_t>>::value) {
						typeID = ReflectTypeID::VectorOfInt64;
					} else 
					if constexpr(std::is_same<T, std::vector<std::string>>::value) {
						typeID = ReflectTypeID::VectorOfString;
					} else 
					if constexpr(std::is_same<T, std::optional<int64_t>>::value) {
						typeID = ReflectTypeID::OptionalOfInt64;
					} else 
					if constexpr(std::is_same<T, std::vector<Capability>>::value) {
						typeID = ReflectTypeID::VectorOfClassCapability;
					} else 
					if constexpr(std::is_same<T, std::optional<MessageType>>::value) {
						typeID = ReflectTypeID::OptionalOfEnumMessageType;
					} else 
					if constexpr(std::is_same<T, std::optional<DeviceState>>::value) {
						typeID = ReflectTypeID::OptionalOfClassDeviceState;
					} else 
					if constexpr(std::is_same<T, std::optional<State>>::value) {
						typeID = ReflectTypeID::OptionalOfClassState;
					} else 
					 {
						typeID = T::_TYPE_ID;
					}
					AnyRef a;
					a.typeID = typeID;
					a.value.voidptr = (void*) obj;
					return a;
				}
			
				union ReflectedTypes {
					void *voidptr;
					ReflectTypeID *u_EnumReflectTypeID;
					ReflectField *u_ClassReflectField;
					ReflectEnumValue *u_ClassReflectEnumValue;
					ReflectType *u_ClassReflectType;
					ReflectTypeKind *u_EnumReflectTypeKind;
					std::vector<ReflectField> *u_VectorOfClassReflectField;
					std::vector<ReflectEnumValue> *u_VectorOfClassReflectEnumValue;
					int32_t *u_Int32;
					int64_t *u_Int64;
					uint32_t *u_Uint32;
					uint8_t *u_Uint8;
					int *u_Int;
					unsigned char *u_UnsignedChar;
					float *u_Float;
					std::string *u_String;
					uint64_t *u_Uint64;
					char *u_Char;
					double *u_Double;
					bool *u_Bool;
					std::vector<uint8_t> *u_VectorOfUint8;
					CpuFamily *u_EnumCpuFamily;
					Os *u_EnumOs;
					AuthenticationType *u_EnumAuthenticationType;
					SystemInfo *u_ClassSystemInfo;
					LoginCredentials *u_ClassLoginCredentials;
					ClientResponseEncrypted *u_ClassClientResponseEncrypted;
					std::optional<std::string> *u_OptionalOfString;
					std::optional<std::vector<uint8_t>> *u_OptionalOfVectorOfUint8;
					Product *u_EnumProduct;
					Platform *u_EnumPlatform;
					Cryptosuite *u_EnumCryptosuite;
					LoginCryptoDiffieHellmanChallenge *u_ClassLoginCryptoDiffieHellmanChallenge;
					LoginCryptoChallengeUnion *u_ClassLoginCryptoChallengeUnion;
					LoginCryptoDiffieHellmanHello *u_ClassLoginCryptoDiffieHellmanHello;
					LoginCryptoHelloUnion *u_ClassLoginCryptoHelloUnion;
					BuildInfo *u_ClassBuildInfo;
					FeatureSet *u_ClassFeatureSet;
					APChallenge *u_ClassAPChallenge;
					APResponseMessage *u_ClassAPResponseMessage;
					LoginCryptoDiffieHellmanResponse *u_ClassLoginCryptoDiffieHellmanResponse;
					LoginCryptoResponseUnion *u_ClassLoginCryptoResponseUnion;
					CryptoResponseUnion *u_ClassCryptoResponseUnion;
					PoWResponseUnion *u_ClassPoWResponseUnion;
					ClientResponsePlaintext *u_ClassClientResponsePlaintext;
					ClientHello *u_ClassClientHello;
					std::optional<LoginCryptoDiffieHellmanChallenge> *u_OptionalOfClassLoginCryptoDiffieHellmanChallenge;
					std::optional<LoginCryptoDiffieHellmanHello> *u_OptionalOfClassLoginCryptoDiffieHellmanHello;
					std::optional<bool> *u_OptionalOfBool;
					std::optional<APChallenge> *u_OptionalOfClassAPChallenge;
					std::optional<LoginCryptoDiffieHellmanResponse> *u_OptionalOfClassLoginCryptoDiffieHellmanResponse;
					std::vector<Cryptosuite> *u_VectorOfEnumCryptosuite;
					std::optional<FeatureSet> *u_OptionalOfClassFeatureSet;
					Header *u_ClassHeader;
					AudioFormat *u_EnumAudioFormat;
					AudioFile *u_ClassAudioFile;
					Restriction *u_ClassRestriction;
					Image *u_ClassImage;
					ImageGroup *u_ClassImageGroup;
					Album *u_ClassAlbum;
					Artist *u_ClassArtist;
					Track *u_ClassTrack;
					Episode *u_ClassEpisode;
					std::optional<AudioFormat> *u_OptionalOfEnumAudioFormat;
					std::vector<Image> *u_VectorOfClassImage;
					std::optional<ImageGroup> *u_OptionalOfClassImageGroup;
					std::optional<Album> *u_OptionalOfClassAlbum;
					std::vector<Artist> *u_VectorOfClassArtist;
					std::optional<int32_t> *u_OptionalOfInt32;
					std::vector<Restriction> *u_VectorOfClassRestriction;
					std::vector<AudioFile> *u_VectorOfClassAudioFile;
					std::vector<Track> *u_VectorOfClassTrack;
					MessageType *u_EnumMessageType;
					PlayStatus *u_EnumPlayStatus;
					CapabilityType *u_EnumCapabilityType;
					TrackRef *u_ClassTrackRef;
					State *u_ClassState;
					Capability *u_ClassCapability;
					DeviceState *u_ClassDeviceState;
					Frame *u_ClassFrame;
					std::optional<uint32_t> *u_OptionalOfUint32;
					std::optional<PlayStatus> *u_OptionalOfEnumPlayStatus;
					std::optional<uint64_t> *u_OptionalOfUint64;
					std::vector<TrackRef> *u_VectorOfClassTrackRef;
					std::optional<CapabilityType> *u_OptionalOfEnumCapabilityType;
					std::vector<int64_t> *u_VectorOfInt64;
					std::vector<std::string> *u_VectorOfString;
					std::optional<int64_t> *u_OptionalOfInt64;
					std::vector<Capability> *u_VectorOfClassCapability;
					std::optional<MessageType> *u_OptionalOfEnumMessageType;
					std::optional<DeviceState> *u_OptionalOfClassDeviceState;
					std::optional<State> *u_OptionalOfClassState;
					
				} value;
				private:
		
	};
	
	
	
	template<class T>
	class __VectorManipulator {
		public:
			static void push_back(AnyRef &vec, AnyRef &val) {
				auto theVector = reinterpret_cast<std::vector<T>*>(vec.value.voidptr);
				auto theValue = *reinterpret_cast<T*>(val.value.voidptr);
				theVector->push_back(theValue);
			};
			static AnyRef at(AnyRef &vec, size_t index) {
				auto theVector = reinterpret_cast<std::vector<T>*>(vec.value.voidptr);
				return AnyRef::of<T>(&(*theVector)[index]);
			};
			static size_t size(AnyRef &vec) {
				auto theVector = reinterpret_cast<std::vector<T>*>(vec.value.voidptr);
				return theVector->size();
			};
			static void emplace_back(AnyRef &vec) {
				auto theVector = reinterpret_cast<std::vector<T>*>(vec.value.voidptr);
				theVector->emplace_back();
			};
			static void clear(AnyRef &vec) {
				auto theVector = reinterpret_cast<std::vector<T>*>(vec.value.voidptr);
				theVector->clear();
			};
			static void reserve(AnyRef &vec, size_t n) {
				auto theVector = reinterpret_cast<std::vector<T>*>(vec.value.voidptr);
				theVector->reserve(n);
			};
	};
	

	template<class T>
	class __OptionalManipulator {
		public:
			static AnyRef get(AnyRef &opt) {
				auto theOptional = reinterpret_cast<std::optional<T>*>(opt.value.voidptr);
				return AnyRef::of<T>(&**theOptional);
			}
			static bool has_value(AnyRef &opt) {
				auto theOptional = reinterpret_cast<std::optional<T>*>(opt.value.voidptr);
				return theOptional->has_value();
			}
			static void set(AnyRef &opt, AnyRef &val) {
				auto theOptional = reinterpret_cast<std::optional<T>*>(opt.value.voidptr);
				auto theValue = reinterpret_cast<T*>(val.value.voidptr);
				*theOptional = *theValue;
			}

			static void reset(AnyRef &opt) {
				auto theOptional = reinterpret_cast<std::optional<T>*>(opt.value.voidptr);
				theOptional->reset();
			}

			static void emplaceEmpty(AnyRef &opt) {
				auto theOptional = reinterpret_cast<std::optional<T>*>(opt.value.voidptr);
				theOptional->emplace();
			}
	};
		
	
extern ReflectType reflectTypeInfo[92];
	

	class UniqueAny: public AnyRef {
		public:
			UniqueAny() {
				this->value.voidptr = nullptr;
			};
			UniqueAny(ReflectTypeID typeID) {
				this->typeID = typeID;
				auto typeInfo = &reflectTypeInfo[static_cast<int>(typeID)];
				AnyRef a;
				this->value.voidptr = new unsigned char[typeInfo->size];
				typeInfo->_Construct(this->value.voidptr);
			};
			~UniqueAny() {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(typeID)];
				typeInfo->_Destruct(this->value.voidptr);
				delete reinterpret_cast<char *>(this->value.voidptr);
			};
	};

	class AnyVectorRef {
		public:
			AnyRef ref;
			AnyVectorRef(AnyRef r): ref(r) {}
			void push_back(AnyRef &v) {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				typeInfo->vectorOps.push_back(ref, v);
			}
			size_t size() {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				return typeInfo->vectorOps.size(ref);
			}

			void emplace_back() {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				typeInfo->vectorOps.emplace_back(ref);
			}

			void clear() {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				typeInfo->vectorOps.clear(ref);
			}

			void reserve(size_t n) {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				typeInfo->vectorOps.reserve(ref, n);
			}


			AnyRef at(size_t index) {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				return typeInfo->vectorOps.at(ref, index);
			}
	};

	class AnyOptionalRef {
		public:
			AnyRef ref;
			AnyOptionalRef(AnyRef r): ref(r) {}
			
			AnyRef get() {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				return typeInfo->optionalOps.get(ref);
			}

			bool has_value() {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				return typeInfo->optionalOps.has_value(ref);
			}
			void set(AnyRef &o) {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				typeInfo->optionalOps.set(ref, o);
			}
			void reset() {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				typeInfo->optionalOps.reset(ref);
			}

			void emplaceEmpty() {
				auto typeInfo = &reflectTypeInfo[static_cast<int>(this->ref.typeID)];
				typeInfo->optionalOps.emplaceEmpty(ref);
			}

	};

	#endif
