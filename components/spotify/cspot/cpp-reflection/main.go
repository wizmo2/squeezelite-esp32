package main

import (
	"io/ioutil"
	"log"
	"os"

	"github.com/celer-network/pb3-gen-sol/generator"
	"google.golang.org/protobuf/proto"
)

func main() {
	c := NewCorntext()

	data, err := ioutil.ReadAll(os.Stdin)
	if err != nil {
		generator.Error(err, "reading input")
	}

	if err := proto.Unmarshal(data, c.Request); err != nil {
		generator.Error(err, "parsing input proto")
	}
	c.buildAllTypes()
	c.outputTypes()
	// Send back the results.
	data, err = proto.Marshal(c.Response)
	if err != nil {
		log.Fatal(err, "failed to marshal output proto")
	}
	_, err = os.Stdout.Write(data)
	if err != nil {
		log.Fatal(err, "failed to write output proto")
	}
}
