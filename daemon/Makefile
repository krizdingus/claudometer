.PHONY: build test run clean

build:
	go build -o bin/cydmonitor ./cmd/cydmonitor

test:
	go test ./...

run: build
	./bin/cydmonitor

clean:
	rm -rf bin/
