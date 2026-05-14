.PHONY: build test run clean

build:
	go build -o bin/claudometer ./cmd/claudometer

test:
	go test ./...

run: build
	./bin/claudometer

clean:
	rm -rf bin/
