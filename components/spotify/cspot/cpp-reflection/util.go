package main

import (
	"path/filepath"
	"strings"
)

func AddIncludeForType(t GeneratableType, gen *CppGenerator) {
	switch v := t.(type) {
	case *ClassType:
		gen.AddLocalInclude(v.ProtoName + ".h")
	case *EnumType:
		gen.AddLocalInclude(v.ProtoName + ".h")
	case *VectorType:
		gen.AddLibraryInclude("vector")
		AddIncludeForType(v.InnerType, gen)
	case *OptionalType:
		gen.AddLibraryInclude("optional")
		AddIncludeForType(v.InnerType, gen)
	}
}

func StripExtenstion(filename string) string {
	return strings.TrimSuffix(filename, filepath.Ext(filename))
}
