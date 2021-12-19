package main

import "fmt"

type VectorHelperTypes struct {
}

func (vht *VectorHelperTypes) GenerateVectorOperationsStruct(cg *CppGenerator) {
	fmt.Fprintln(cg.BodyBeforeLocalIncludes, `
		class AnyRef;
		struct VectorOperations {
			void (*push_back)(AnyRef &vec, AnyRef &val);
			AnyRef (*at)(AnyRef &vec, size_t index);
			size_t (*size)(AnyRef &vec);
			void (*emplace_back)(AnyRef &vec);
			void (*clear)(AnyRef &vec);
			void (*reserve)(AnyRef &vec, size_t n);
		};
		
	`)
}

func (vht *VectorHelperTypes) GenerateVectorManipulator(cg *CppGenerator) {
	fmt.Fprintln(cg.Body, `
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
	`)
}
