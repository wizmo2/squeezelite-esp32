package main

type AliasType struct {
	Of      GeneratableType
	cppType string
}

func (at *AliasType) IdentifierName() string {
	return at.Of.IdentifierName()
}

func (at *AliasType) CppType() string {
	return at.cppType
}

func (at *AliasType) WriteDeclarations(gen *CppGenerator) {

}
func (at *AliasType) WriteReflection(gen *CppGenerator) {

}
