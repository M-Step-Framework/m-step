package cmd

import (
	"github.com/fatih/color"
)

func Quit(projectRoot string) {
	color.New(color.FgYellow).Println("Exiting M-Step CLI...")
}