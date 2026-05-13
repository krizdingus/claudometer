package main

import (
	"fmt"
	"os"
)

const version = "0.1.0-dev"

func main() {
	if len(os.Args) > 1 {
		switch os.Args[1] {
		case "version":
			fmt.Println(version)
			return
		case "status":
			fmt.Fprintln(os.Stderr, "status: not yet implemented")
			os.Exit(2)
		case "reset-pairings":
			fmt.Fprintln(os.Stderr, "reset-pairings: not yet implemented")
			os.Exit(2)
		}
	}
	fmt.Fprintln(os.Stderr, "serve: not yet implemented")
	os.Exit(2)
}
