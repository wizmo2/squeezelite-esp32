package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path"
	"strings"
	"unicode"
)

type CppGenerator struct {
	includes                []string
	files                   map[string]*CppGenerator
	Body                    *bytes.Buffer
	BodyBeforeLocalIncludes *bytes.Buffer
	Filename                string
	IsHeader                bool
}

// NewCppGenerator docsy bo ci wywali sie blad xd
func NewCppGenerator(filename string) *CppGenerator {
	isHeader := true
	if strings.HasSuffix(filename, ".cpp") {
		isHeader = false
	}
	return &CppGenerator{
		includes:                []string{},
		Body:                    &bytes.Buffer{},
		BodyBeforeLocalIncludes: &bytes.Buffer{},
		files:                   make(map[string]*CppGenerator),
		Filename:                filename,
		IsHeader:                isHeader,
	}
}

func (cg *CppGenerator) SubFile(filename string, isHeader bool) *CppGenerator {
	if gen, ok := cg.files[filename]; ok {
		return gen
	}
	gen := NewCppGenerator(filename)
	gen.IsHeader = isHeader
	gen.files = cg.files
	cg.files[filename] = gen
	return gen
}

// AddLibraryInclude yes
func (cg *CppGenerator) AddLibraryInclude(name string) *CppGenerator {
	resultingLine := fmt.Sprintf("#include <%s>", name)
	for _, a := range cg.includes {
		if a == resultingLine {
			return cg
		}
	}
	cg.includes = append(cg.includes, resultingLine)
	return cg
}

func (cg *CppGenerator) AddLocalInclude(name string) *CppGenerator {
	resultingLine := fmt.Sprintf("#include \"%s\"", name)
	for _, a := range cg.includes {
		if a == resultingLine {
			return cg
		}
	}
	cg.includes = append(cg.includes, resultingLine)
	return cg
}

// OutputClassField yes
func (cg *CppGenerator) OutputClassField(theType string, name string) {
	fmt.Fprintf(cg.Body, "%v %v;\n", theType, name)
}

// OutputClassTypeID yes
func (cg *CppGenerator) OutputClassTypeID(theID string) {
	fmt.Fprintf(cg.Body, "static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::%v;\n", theID)
}

// OutputClass yes
func (cg *CppGenerator) OutputClass(name string, cb func()) {
	fmt.Fprintf(cg.Body, "class %v {\npublic:\n", name)
	cb()
	fmt.Fprintf(cg.Body, "};\n\n")
}

// OutputEnumClass
func (cg *CppGenerator) OutputEnumClass(name string, cb func()) {
	fmt.Fprintf(cg.Body, "enum class %v {\n", name)
	cb()
	fmt.Fprintf(cg.Body, "};\n\n")
}

func (cg *CppGenerator) OutputArrayVariable(t string, name string, length int, cb func()) {
	fmt.Fprintf(cg.Body, "%v %v[%d] = {\n", t, name, length)
	cb()
	fmt.Fprintf(cg.Body, "};\n\n")
}

func (cg *CppGenerator) OutputArrayVariableExtern(t string, name string, length int) {
	fmt.Fprintf(cg.Body, "extern %v %v[%d];", t, name, length)
}

func (cg *CppGenerator) OutputEnumClassField(name string, value string) {
	fmt.Fprintf(cg.Body, "%v", name)
	if value != "" {
		fmt.Fprintf(cg.Body, " = %v", value)
	}
	fmt.Fprintf(cg.Body, ",\n")
}

func (cg *CppGenerator) EscapeCppString(str string) string {
	d, _ := json.Marshal(str)

	return string(d)
}

func (cg *CppGenerator) WriteToWriter(w io.Writer) {
	fmt.Fprintf(w, "// THIS CORNFILE IS GENERATED. DO NOT EDIT! 🌽\n")
	guardString := "_"
	for _, c := range []rune(cg.Filename) {
		if unicode.IsUpper(c) {
			guardString += "_"
		}
		if unicode.IsLetter(c) {
			guardString += strings.ToUpper(string([]rune{c}))
		}
	}
	if cg.IsHeader {

		fmt.Fprintf(w, "#ifndef %v\n", guardString)
		fmt.Fprintf(w, "#define %v\n", guardString)
	}
	for _, a := range cg.includes {
		if strings.Contains(a, "<") {
			fmt.Fprintf(w, "%v\n", a)
		}
	}
	io.Copy(w, cg.BodyBeforeLocalIncludes)
	for _, a := range cg.includes {
		if !strings.Contains(a, "<") && a != fmt.Sprintf("#include \"%v\"", cg.Filename) {
			fmt.Fprintf(w, "%v\n", a)
		}
	}
	io.Copy(w, cg.Body)
	if cg.IsHeader {
		fmt.Fprintf(w, "#endif\n")
	}
}

func (cg *CppGenerator) OutputToDirectory(dirPath string) {
	f, _ := os.Create(path.Join(dirPath, cg.Filename))
	defer f.Close()
	cg.WriteToWriter(f)

	for _, fileToOutput := range cg.files {
		f, _ := os.Create(path.Join(dirPath, fileToOutput.Filename))
		defer f.Close()
		fileToOutput.WriteToWriter(f)
	}

}
