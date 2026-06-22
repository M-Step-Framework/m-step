package cmd

import (
	"github.com/fatih/color"
	"path/filepath"
)

func Eval(projectRoot string) {
	const evalScript = "/cmd-evaluation/run.sh"
	selected := runEvalSelector()
	if selected == "" {
		color.New(color.FgYellow).Println("Evaluation cancelled.")
	} else {
		color.New(color.FgCyan).Printf(
			"Running evaluation: %s\n\n", selected)
		if err := runExternalScript(
				filepath.Join(projectRoot, ScriptDir, evalScript), selected); err != nil {
			color.New(color.FgRed).Printf(
				"Evaluation %s failed: %v\n\n", selected, err)
		} else {
			color.New(color.FgGreen).Printf(
				"✓ Evaluation %s completed successfully.\n\n", selected)
		}
	}
}
