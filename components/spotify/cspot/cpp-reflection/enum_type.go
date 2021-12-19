package main

import "fmt"

type EnumValue struct {
	Name  string
	Value string
}

type EnumType struct {
	Name      string
	Values    []EnumValue
	ProtoName string
}

func (et *EnumType) IdentifierName() string {
	return "Enum" + et.Name
}

func (et *EnumType) DeclarationOrder() int {
	return -20
}

func (et *EnumType) CppType() string {
	return et.Name
}

func (et *EnumType) WriteDeclarations(gen *CppGenerator) {
	enumSubfile := gen.SubFile(et.ProtoName+".h", true)
	gen.AddLocalInclude(enumSubfile.Filename)
	enumSubfile.OutputEnumClass(et.Name, func() {
		for _, v := range et.Values {
			enumSubfile.OutputEnumClassField(v.Name, v.Value)
		}
	})
}
func (et *EnumType) WriteReflection(gen *CppGenerator) {
	enumValues := ""
	for _, v := range et.Values {
		enumValues += fmt.Sprintf("   ReflectEnumValue(%v, %v),\n", gen.EscapeCppString(v.Name), v.Value)
	}
	fmt.Fprintf(gen.Body, "ReflectType::ofEnum(/* mine id */ ReflectTypeID::%v, /* name */ %v, /* enum values */ std::move(std::vector<ReflectEnumValue>{%v}), /* size */ sizeof(%v))",
		et.IdentifierName(), gen.EscapeCppString(et.CppType()), enumValues, et.CppType())
}
