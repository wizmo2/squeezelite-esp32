package main

import "fmt"

type OptionalHelperTypes struct {
}

func (oht *OptionalHelperTypes) GenerateOptionalOperationsStruct(cg *CppGenerator) {
	fmt.Fprintln(cg.BodyBeforeLocalIncludes, `
		class AnyRef;
		struct OptionalOperations {
			AnyRef (*get)(AnyRef &opt);
			bool (*has_value)(AnyRef &opt);
			void (*set)(AnyRef &opt, AnyRef &val);
			void (*reset)(AnyRef &opt);
			void (*emplaceEmpty)(AnyRef &opt);
		};
		
	`)
}

func (oht *OptionalHelperTypes) GenerateOptionalManipulator(cg *CppGenerator) {
	fmt.Fprintln(cg.Body, `
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
		
	`)
}
