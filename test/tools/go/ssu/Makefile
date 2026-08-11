.PHONY: all format-check module-check vet test build clean
GO_ENV := GOWORK=off GOPROXY=off GOSUMDB=off GOFLAGS=-buildvcs=false
SDK_MODULE := atomgit.com/openeuler/ubs-engine.git
all: format-check module-check vet test build
format-check:
	@test -z "$$(gofmt -l .)"
module-check:
	@test "$$($(GO_ENV) go list -m -f '{{with .Replace}}{{.Dir}}{{end}}' $(SDK_MODULE))" = "$$(cd ../../../.. && pwd -P)"
vet: module-check
	$(GO_ENV) go vet ./...
test: module-check
	$(GO_ENV) go test ./...
build: module-check
	mkdir -p bin
	$(GO_ENV) go build -o bin/ubse_ssu_test_go .
clean:
	rm -rf bin
