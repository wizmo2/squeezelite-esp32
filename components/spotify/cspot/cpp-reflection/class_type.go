package main

import (
	"fmt"
)

type ClassField struct {
	Name        string
	ProtobufTag uint32
	Type        GeneratableType
}

type ClassType struct {
	Name                      string
	Fields                    []ClassField
	AdditionalCode            string
	AdditionalLibraryIncludes []string
	ProtoName                 string
}

func (ct *ClassType) IdentifierName() string {
	return "Class" + ct.Name
}

func (ct *ClassType) CppType() string {
	return ct.Name
}

func (et *ClassType) DeclarationOrder() int {
	return -10
}

func (ct *ClassType) ForwardDeclaration() string {
	return fmt.Sprintf("class %v;", ct.Name)
}

func (ct *ClassType) WriteDeclarations(gen *CppGenerator) {
	classSubfile := gen.SubFile(ct.ProtoName+".h", true)
	gen.AddLocalInclude(classSubfile.Filename)
	for _, f := range ct.Fields {
		AddIncludeForType(f.Type, classSubfile)
	}
	for _, i := range ct.AdditionalLibraryIncludes {
		classSubfile.AddLibraryInclude(i)
	}
	classSubfile.OutputClass(ct.Name, func() {
		for _, t := range ct.Fields {
			classSubfile.OutputClassField(t.Type.CppType(), t.Name)
		}
		classSubfile.OutputClassTypeID(ct.IdentifierName())
		fmt.Fprint(classSubfile.Body, ct.AdditionalCode)
	})
}
func (ct *ClassType) WriteReflection(gen *CppGenerator) {
	gen.AddLibraryInclude("vector")
	fieldsContents := ""
	for _, f := range ct.Fields {
		fieldsContents += fmt.Sprintf("ReflectField( /* typeID */ ReflectTypeID::%v, /* name */ %v, /* offset */ offsetof(%v, %v), /* protobuf tag */ %v),\n",
			f.Type.IdentifierName(), gen.EscapeCppString(f.Name), ct.CppType(), f.Name, f.ProtobufTag)
	}
	fmt.Fprintf(gen.Body, `ReflectType::ofClass(
	/* mine type id */ ReflectTypeID::%v, 
	/* name */ %v, 
	/* fields */ std::move(std::vector<ReflectField>{%v}), 
	/* size */ sizeof(%v), 
	__reflectConstruct<%v>,
	__reflectDestruct<%v>)`,
		ct.IdentifierName(), gen.EscapeCppString(ct.CppType()), fieldsContents, ct.CppType(), ct.CppType(), ct.CppType())
}
