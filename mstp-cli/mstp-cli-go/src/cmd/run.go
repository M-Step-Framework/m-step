package cmd

import (
	"fmt"
	"github.com/fatih/color"
	"path/filepath"
)

func Run(projectRoot string) {
	const runScript = "run.sh"
	if err := runExternalScript(filepath.Join(projectRoot, ScriptDir, runScript)); err != nil {
		color.New(color.FgRed).Println("Failed to run pipeline script:")
		fmt.Printf("%v\n\n", err)
	} else {
		color.New(color.FgGreen).Println("✓ Pipeline started successfully.")
		fmt.Println()
	}
}
