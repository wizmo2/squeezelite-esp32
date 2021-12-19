# Running

```sh
$ go build -o protoc-gen-cpprefl . && protoc --plugin=protoc-gen-cpprefl=protoc-gen-cpprefl --cpprefl_out okon protos/*.proto --proto_path protos/
```

Will get protos from `protos/` and output to `out/protobuf.h`
