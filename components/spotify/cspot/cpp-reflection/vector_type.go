package main

import (
	"fmt"
	"text/template"
)

type VectorType struct {
	InnerType GeneratableType
}

func NewVectorType(inner GeneratableType) GenericType {
	return &VectorType{
		InnerType: inner,
	}
}

func (vt *VectorType) GetInnerType() GeneratableType {
	return vt.InnerType
}

func (vt *VectorType) IdentifierName() string {
	return "VectorOf" + vt.InnerType.IdentifierName()
}
func (vt *VectorType) CppType() string {
	return fmt.Sprintf("std::vector<%v>", vt.InnerType.CppType())
}

func (vt *VectorType) WriteDeclarations(gen *CppGenerator) {
	gen.AddLibraryInclude("vector")
}
func (vt *VectorType) WriteReflection(gen *CppGenerator) {

	template.Must(template.New("any.cpp").Parse(`
	ReflectType::ofVector(
		/* mine typeId */ ReflectTypeID::{{ .IdentifierName }},
		/* inner type id */  ReflectTypeID::{{ .InnerType.IdentifierName }},
		/* size */ sizeof({{ .CppType }}),
		VectorOperations{
			.push_back = __VectorManipulator<{{ .InnerType.CppType }}>::push_back,
			.at = __VectorManipulator<{{ .InnerType.CppType }}>::at,
			.size = __VectorManipulator<{{ .InnerType.CppType }}>::size,
			.emplace_back =  __VectorManipulator<{{ .InnerType.CppType }}>::emplace_back,
			.clear = __VectorManipulator<{{ .InnerType.CppType }}>::clear,
			.reserve = __VectorManipulator<{{ .InnerType.CppType }}>::reserve,
		},
		__reflectConstruct<{{ .CppType }}>,
		__reflectDestruct<{{ .CppType }}>
	)
	
	
	
	`)).Execute(gen.Body, vt)
	// fmt.Fprintf(gen.Body, `ReflectType::ofVector(
	// 		/* mine typeId */ ReflectTypeID::%v,
	// 		/* inner type id */  ReflectTypeID::%v,
	// 		/* size */ sizeof(%v),
	// 		VectorOperations{
	// 			.push_back = __VectorManipulator<%v>
	// 		},
	// 		__reflectConstruct<%v>,
	// 		__reflectDestruct<%v>
	// 	)`,
	// 	vt.IdentifierName(), vt.InnerType.IdentifierName(), vt.CppType(), vt.CppType(), vt.CppType(), vt.CppType())
}
