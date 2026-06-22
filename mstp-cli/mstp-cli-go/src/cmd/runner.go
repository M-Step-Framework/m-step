package cmd

import (
	"os"
	"os/exec"
)

const ScriptDir = "../scripts"

func runExternalScript(scriptPath string, args ...string) error {
	cmd := exec.Command(scriptPath, args...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}
