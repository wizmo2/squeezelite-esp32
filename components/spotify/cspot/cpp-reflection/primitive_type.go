package main

import "fmt"

type PrimitiveType struct {
	identifierName string
	cppType        string
}

func (pt *PrimitiveType) IdentifierName() string {
	return pt.identifierName
}

func (pt *PrimitiveType) CppType() string {
	return pt.cppType
}

func (pt *PrimitiveType) WriteDeclarations(gen *CppGenerator) {
	if pt.cppType == "std::string" {
		gen.AddLibraryInclude("string")
	}
}
func (pt *PrimitiveType) WriteReflection(gen *CppGenerator) {
	fmt.Fprintf(gen.Body, "ReflectType::ofPrimitive(/* type id */ ReflectTypeID::%v, /* name */ %v, /* size */ sizeof(%v))",
		pt.IdentifierName(), gen.EscapeCppString(pt.CppType()), pt.CppType())
}
