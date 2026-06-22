package cmd

import (
	"fmt"
	"github.com/fatih/color"
)

func Help(projectRoot string) {
	color.New(color.Bold).Println()
	color.New(color.Bold).Println("Available Commands:")
	color.New(color.FgCyan).Print("  /help")
	fmt.Println("    - Show this message")
	color.New(color.FgCyan).Print("  /eval")
	fmt.Println("    - Interactive evaluation selector")
	color.New(color.FgCyan).Print("  /run")
	fmt.Println("     - Execute the main M-Step pipeline")
	color.New(color.FgCyan).Print("  /status")
	fmt.Println("  - Check framework status")
	fmt.Println()
}