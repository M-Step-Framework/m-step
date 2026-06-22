package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/chzyer/readline"
	figure "github.com/common-nighthawk/go-figure"
	"github.com/fatih/color"

	"mstp-cli-go/src/cmd"
)

const (
	banner         = "M - STEP"
	welcomeMessage = "Welcome to the M-Step Framework CLI."
)

func printHeader() {
	asciiArt := figure.NewFigure(banner, "", true).String()
	color.New(color.FgCyan, color.Bold).Print(asciiArt)
	color.New(color.Faint).Println(welcomeMessage)
	fmt.Println("Type /help for commands, or 'exit' to quit.")
	fmt.Println()
}

func resolveProjectRoot() (string, error) {
	_, currentFile, _, ok := runtime.Caller(0)
	if !ok {
		return "", errors.New("unable to resolve project root")
	}

	return filepath.Dir(filepath.Dir(currentFile)), nil
}

func executeCommand(command, projectRoot string) bool {
	input := strings.ToLower(strings.TrimSpace(command))

	switch input {
	case "exit", "quit":
		cmd.Quit(projectRoot)
		return false
	case "/help":
		cmd.Help(projectRoot)
	case "/eval":
		cmd.Eval(projectRoot)
	case "/run":
		cmd.Run(projectRoot)
	default:
		if input != "" {
			color.New(color.FgRed).Print("Unknown command:")
			fmt.Printf(" %s\n\n", input)
		}
	}

	return true
}

func main() {
	projectRoot, err := resolveProjectRoot()

	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to resolve project root: %v\n", err)
		os.Exit(1)
	}

	printHeader()

	prompt := color.New(color.FgMagenta).Sprint("> ")
	reader, err := readline.NewEx(&readline.Config{
		Prompt:            prompt,
		HistoryLimit:      100,
		InterruptPrompt:   "",
		EOFPrompt:         "",
		HistorySearchFold: true,
	})
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to initialize prompt: %v\n", err)
		os.Exit(1)
	}
	defer reader.Close()

	for {
		line, err := reader.Readline()
		if err != nil {
			if errors.Is(err, readline.ErrInterrupt) {
				// Ctrl+C clears the current line and keeps the session running.
				continue
			}
			if errors.Is(err, io.EOF) {
				break // Ctrl+D exits.
			}

			fmt.Fprintf(os.Stderr, "prompt error: %v\n", err)
			continue
		}

		if !executeCommand(line, projectRoot) {
			break
		}
	}
}
