//go:build windows

package main

import (
	_ "embed"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

//go:embed bundle/VencordArabicLiquidDX11.exe
var guiExe []byte

//go:embed bundle/VencordArabicInstallerCore.exe
var coreExe []byte

func writeExecutable(path string, data []byte) error {
	if err := os.WriteFile(path, data, 0o700); err != nil {
		return err
	}
	return os.Chmod(path, 0o700)
}

func main() {
	tmp, err := os.MkdirTemp("", "VencordArabicLiquidDX11-")
	if err != nil {
		showError(err)
		return
	}
	defer os.RemoveAll(tmp)

	guiPath := filepath.Join(tmp, "VencordArabicLiquidDX11.exe")
	corePath := filepath.Join(tmp, "VencordArabicInstallerCore.exe")
	if err = writeExecutable(guiPath, guiExe); err != nil {
		showError(err)
		return
	}
	if err = writeExecutable(corePath, coreExe); err != nil {
		showError(err)
		return
	}

	cmd := exec.Command(guiPath)
	cmd.Dir = tmp
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin
	if err = cmd.Run(); err != nil {
		if exitErr, ok := err.(*exec.ExitError); ok {
			os.Exit(exitErr.ExitCode())
		}
		showError(err)
	}
}

func showError(err error) {
	msg := fmt.Sprintf("Vencord Arabic Liquid Glass Installer could not start:\n\n%v", err)
	_ = exec.Command("powershell", "-NoProfile", "-WindowStyle", "Hidden", "-Command",
		"Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show($args[0], 'Vencord Arabic Installer', 'OK', 'Error') | Out-Null", msg).Run()
}
