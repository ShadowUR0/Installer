//go:build wails

/*
 * SPDX-License-Identifier: GPL-3.0
 * Vencord Arabic Installer - Wails frontend preserving the original installer UX
 */

package main

import (
	"context"
	"embed"
	"errors"
	"fmt"
	"os"
	"os/exec"
	path "path/filepath"
	"runtime"
	"strings"
	"sync"
	"vencordinstaller/buildinfo"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
	"github.com/wailsapp/wails/v2/pkg/options/windows"
)

//go:embed all:frontend/dist
var wailsAssets embed.FS

var (
	wailsGithubReady = make(chan struct{})
	wailsGithubOK    bool
	wailsGithubOnce  sync.Once
)

type InstallInfo struct {
	Index       int    `json:"index"`
	Branch      string `json:"branch"`
	Path        string `json:"path"`
	Patched     bool   `json:"patched"`
	OpenAsar    bool   `json:"openAsar"`
	Flatpak     bool   `json:"flatpak"`
	DisplayName string `json:"displayName"`
}

type InstallerStatus struct {
	Ready          bool          `json:"ready"`
	GithubOK       bool          `json:"githubOk"`
	GithubError    string        `json:"githubError"`
	InstalledHash  string        `json:"installedHash"`
	LatestHash     string        `json:"latestHash"`
	FilesDir       string        `json:"filesDir"`
	FilesDirError  string        `json:"filesDirError"`
	DevInstall     bool          `json:"devInstall"`
	InstallerTag   string        `json:"installerTag"`
	InstallerHash  string        `json:"installerHash"`
	SelfOutdated   bool          `json:"selfOutdated"`
	Installs       []InstallInfo `json:"installs"`
}

type OperationResult struct {
	OK      bool            `json:"ok"`
	Title   string          `json:"title"`
	Message string          `json:"message"`
	Status  InstallerStatus `json:"status"`
}

type InstallerApp struct {
	ctx      context.Context
	mu       sync.Mutex
	installs []*DiscordInstall
}

func NewInstallerApp() *InstallerApp {
	return &InstallerApp{}
}

func (a *InstallerApp) startup(ctx context.Context) {
	a.ctx = ctx
	a.mu.Lock()
	a.refreshInstallsLocked()
	a.mu.Unlock()
}

func (a *InstallerApp) refreshInstallsLocked() {
	found := FindDiscords()
	a.installs = make([]*DiscordInstall, 0, len(found))
	for _, item := range found {
		if install, ok := item.(*DiscordInstall); ok && install != nil {
			a.installs = append(a.installs, install)
		}
	}
}

func (a *InstallerApp) installInfosLocked() []InstallInfo {
	result := make([]InstallInfo, 0, len(a.installs))
	for i, d := range a.installs {
		name := "Discord"
		if d.branch != "" {
			name = strings.ToUpper(d.branch[:1]) + d.branch[1:]
		}
		result = append(result, InstallInfo{
			Index:       i,
			Branch:      d.branch,
			Path:        d.path,
			Patched:     d.isPatched,
			OpenAsar:    d.IsOpenAsar(),
			Flatpak:     d.isFlatpak,
			DisplayName: name,
		})
	}
	return result
}

func (a *InstallerApp) statusLocked() InstallerStatus {
	status := InstallerStatus{
		Ready:         isWailsGithubReady(),
		GithubOK:      wailsGithubOK,
		InstalledHash: InstalledHash,
		LatestHash:    LatestHash,
		FilesDir:      FilesDir,
		DevInstall:    IsDevInstall,
		InstallerTag:  buildinfo.InstallerTag,
		InstallerHash: buildinfo.InstallerGitHash,
		SelfOutdated:  IsSelfOutdated,
		Installs:      a.installInfosLocked(),
	}
	if GithubError != nil {
		status.GithubError = GithubError.Error()
	}
	if FilesDirErr != nil {
		status.FilesDirError = FilesDirErr.Error()
	}
	return status
}

func (a *InstallerApp) GetStatus() InstallerStatus {
	a.mu.Lock()
	defer a.mu.Unlock()
	return a.statusLocked()
}

func (a *InstallerApp) Refresh() InstallerStatus {
	a.mu.Lock()
	defer a.mu.Unlock()
	a.refreshInstallsLocked()
	return a.statusLocked()
}

func (a *InstallerApp) CompletePath(input string) []string {
	if input == "" {
		return nil
	}

	dir := path.Dir(input)
	prefix := path.Base(input)
	if strings.HasSuffix(input, string(os.PathSeparator)) || strings.HasSuffix(input, "/") || strings.HasSuffix(input, "\\") {
		dir = input
		prefix = ""
	}

	entries, err := os.ReadDir(dir)
	if err != nil {
		return nil
	}

	prefixLower := strings.ToLower(prefix)
	results := make([]string, 0, 16)
	for _, entry := range entries {
		if prefixLower != "" && !strings.HasPrefix(strings.ToLower(entry.Name()), prefixLower) {
			continue
		}
		candidate := path.Join(dir, entry.Name())
		if entry.IsDir() {
			candidate += string(os.PathSeparator)
		}
		results = append(results, candidate)
		if len(results) >= 24 {
			break
		}
	}
	return results
}

func (a *InstallerApp) Install(index int, customPath string) OperationResult {
	return a.runInstallOperation(index, customPath, false)
}

func (a *InstallerApp) Repair(index int, customPath string) OperationResult {
	return a.runInstallOperation(index, customPath, true)
}

func (a *InstallerApp) runInstallOperation(index int, customPath string, repair bool) OperationResult {
	a.mu.Lock()
	defer a.mu.Unlock()

	if err := waitForGithub(); err != nil {
		return a.failureLocked("Uh Oh!", fmt.Errorf("Failed to install the latest Vencord builds from GitHub: %w", err))
	}
	install, err := a.resolveInstallLocked(index, customPath)
	if err != nil {
		return a.failureLocked("Invalid Location", err)
	}
	if CheckScuffedInstall() {
		return a.failureLocked("Hold On!", errors.New("You have a broken Discord Install. Please reinstall Discord before proceeding"))
	}

	if repair && !IsDevInstall {
		if err := installLatestBuilds(); err != nil {
			return a.failureLocked("Uh Oh!", fmt.Errorf("Failed to install the latest Vencord builds from GitHub: %w", err))
		}
	}
	if err := install.patch(); err != nil {
		return a.failureLocked("Failed to patch this Install", humaniseInstallerError(err))
	}

	a.refreshInstallsLocked()
	if repair {
		return OperationResult{
			OK:      true,
			Title:   "Successfully Repaired",
			Message: "Vencord Arabic was updated and reinstalled successfully. If Discord is still open, fully close it first, then start it again",
			Status:  a.statusLocked(),
		}
	}
	return OperationResult{
		OK:      true,
		Title:   "Successfully Patched",
		Message: "If Discord is still open, fully close it first. Then start it and verify Vencord installed successfully by looking for its category in Discord Settings",
		Status:  a.statusLocked(),
	}
}

func (a *InstallerApp) Uninstall(index int, customPath string) OperationResult {
	a.mu.Lock()
	defer a.mu.Unlock()

	install, err := a.resolveInstallLocked(index, customPath)
	if err != nil {
		return a.failureLocked("Invalid Location", err)
	}
	if !install.isPatched {
		return a.failureLocked("Vencord is not installed", errors.New("The selected Discord install does not appear to be patched"))
	}
	if err := install.unpatch(); err != nil {
		return a.failureLocked("Failed to unpatch this Install", humaniseInstallerError(err))
	}

	a.refreshInstallsLocked()
	return OperationResult{
		OK:      true,
		Title:   "Successfully Unpatched",
		Message: "If Discord is still open, fully close it first. Then start it again, it should be back to stock!",
		Status:  a.statusLocked(),
	}
}

func (a *InstallerApp) ToggleOpenAsar(index int, customPath string) OperationResult {
	a.mu.Lock()
	defer a.mu.Unlock()

	install, err := a.resolveInstallLocked(index, customPath)
	if err != nil {
		return a.failureLocked("Invalid Location", err)
	}

	wasInstalled := install.IsOpenAsar()
	if wasInstalled {
		err = install.UninstallOpenAsar()
	} else {
		err = install.InstallOpenAsar()
	}
	if err != nil {
		return a.failureLocked("OpenAsar", humaniseInstallerError(err))
	}

	a.refreshInstallsLocked()
	if wasInstalled {
		return OperationResult{OK: true, Title: "Successfully Uninstalled OpenAsar", Message: "If Discord is still open, fully close it first. Then start it again and it should be back to stock!", Status: a.statusLocked()}
	}
	return OperationResult{OK: true, Title: "Successfully Installed OpenAsar", Message: "If Discord is still open, fully close it first. Then start it again and verify OpenAsar installed successfully!", Status: a.statusLocked()}
}

func (a *InstallerApp) UpdateInstaller() OperationResult {
	a.mu.Lock()
	defer a.mu.Unlock()

	if !CanUpdateSelf() {
		return a.failureLocked("Installer Update", errors.New("No installer update is currently available"))
	}
	if err := UpdateSelf(); err != nil {
		return a.failureLocked("Failed to update self!", err)
	}
	if err := RelaunchSelf(); err != nil {
		return a.failureLocked("Failed to restart self!", err)
	}
	return OperationResult{OK: true, Title: "Updated", Message: "The installer was updated successfully", Status: a.statusLocked()}
}

func (a *InstallerApp) OpenFilesDirectory() error {
	var command *exec.Cmd
	switch runtime.GOOS {
	case "windows":
		command = exec.Command("explorer", FilesDir)
	case "darwin":
		command = exec.Command("open", FilesDir)
	default:
		command = exec.Command("xdg-open", FilesDir)
	}
	return command.Start()
}

func (a *InstallerApp) resolveInstallLocked(index int, customPath string) (*DiscordInstall, error) {
	if index >= 0 {
		if index >= len(a.installs) {
			return nil, fmt.Errorf("invalid Discord install index: %d", index)
		}
		return a.installs[index], nil
	}

	customPath = strings.TrimSpace(customPath)
	if customPath == "" {
		return nil, errors.New("The specified location is not a valid Discord install. Make sure you select the base folder")
	}
	install := ParseDiscord(customPath, "")
	if install == nil {
		return nil, errors.New("The specified location is not a valid Discord install. Make sure you select the base folder. Hint: Discord snap is not supported; use flatpak or .deb")
	}
	return install, nil
}

func (a *InstallerApp) failureLocked(title string, err error) OperationResult {
	return OperationResult{OK: false, Title: title, Message: err.Error(), Status: a.statusLocked()}
}

func InstallLatestBuilds() error {
	if IsDevInstall {
		return nil
	}
	if err := waitForGithub(); err != nil {
		return err
	}
	return installLatestBuilds()
}

func HandleScuffedInstall() {
	// Wails surfaces this state through OperationResult instead of an ImGui popup.
}

func humaniseInstallerError(err error) error {
	if !errors.Is(err, os.ErrPermission) {
		return err
	}
	switch runtime.GOOS {
	case "windows":
		return errors.New("Permission denied. Make sure your Discord is fully closed (from the tray)!")
	case "darwin":
		return errors.New("Permission denied. Please grant the installer Full Disk Access in System Settings")
	default:
		return errors.New("Permission denied. Maybe try running me as Administrator/Root?")
	}
}

func isWailsGithubReady() bool {
	select {
	case <-wailsGithubReady:
		return true
	default:
		return false
	}
}

func waitForGithub() error {
	<-wailsGithubReady
	if !wailsGithubOK {
		if GithubError != nil {
			return GithubError
		}
		return errors.New("failed to fetch release information from GitHub")
	}
	return nil
}

func watchGithubDownloader() {
	ok := <-GithubDoneChan
	wailsGithubOK = ok
	wailsGithubOnce.Do(func() { close(wailsGithubReady) })
}

func main() {
	LogLevel = LevelDebug
	InitGithubDownloader()
	go watchGithubDownloader()

	app := NewInstallerApp()
	err := wails.Run(&options.App{
		Title:            "Vencord Arabic Installer",
		Width:            1200,
		Height:           800,
		MinWidth:         900,
		MinHeight:        650,
		Frameless:        false,
		DisableResize:    false,
		BackgroundColour: &options.RGBA{R: 31, G: 32, B: 35, A: 255},
		AssetServer:      &assetserver.Options{Assets: wailsAssets},
		OnStartup:        app.startup,
		Bind:             []interface{}{app},
		Windows: &windows.Options{
			WebviewIsTransparent: false,
			WindowIsTranslucent:  false,
			BackdropType:         windows.None,
			DisableWindowIcon:    false,
			Theme:                windows.Dark,
		},
	})
	if err != nil {
		Log.Error("Wails UI failed:", err)
	}
}
