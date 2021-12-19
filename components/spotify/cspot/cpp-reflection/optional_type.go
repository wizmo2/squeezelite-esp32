package main

import (
	"fmt"
	"text/template"
)

type OptionalType struct {
	InnerType GeneratableType
}

func NewOptionalType(inner GeneratableType) GenericType {
	return &OptionalType{
		InnerType: inner,
	}
}

func (vt *OptionalType) GetInnerType() GeneratableType {
	return vt.InnerType
}

func (vt *OptionalType) IdentifierName() string {
	return "OptionalOf" + vt.InnerType.IdentifierName()
}
func (vt *OptionalType) CppType() string {
	return fmt.Sprintf("std::optional<%v>", vt.InnerType.CppType())
}

func (vt *OptionalType) WriteDeclarations(gen *CppGenerator) {
	gen.AddLibraryInclude("optional")
}
func (vt *OptionalType) WriteReflection(gen *CppGenerator) {

	template.Must(template.New("any.cpp").Parse(`
	ReflectType::ofOptional(
		/* mine typeId */ ReflectTypeID::{{ .IdentifierName }},
		/* inner type id */  ReflectTypeID::{{ .InnerType.IdentifierName }},
		/* size */ sizeof({{ .CppType }}),
		/* option */ OptionalOperations{
			.get = __OptionalManipulator<{{ .InnerType.CppType }}>::get,
			.has_value = __OptionalManipulator<{{ .InnerType.CppType }}>::has_value,
			.set = __OptionalManipulator<{{ .InnerType.CppType }}>::set,
			.reset = __OptionalManipulator<{{ .InnerType.CppType }}>::reset,
			.emplaceEmpty =  __OptionalManipulator<{{ .InnerType.CppType }}>::emplaceEmpty,
		},
		__reflectConstruct<{{ .CppType }}>,
		__reflectDestruct<{{ .CppType }}>
	)
	
	
	
	`)).Execute(gen.Body, vt)

}
