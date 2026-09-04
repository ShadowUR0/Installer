//go:build wails

/*
 * SPDX-License-Identifier: GPL-3.0
 * Vencord Arabic Installer - Liquid Glass Wails frontend
 */

package main

import (
	"context"
	"embed"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"sync"

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
	Ready         bool          `json:"ready"`
	GithubOK      bool          `json:"githubOk"`
	GithubError   string        `json:"githubError"`
	InstalledHash string        `json:"installedHash"`
	LatestHash    string        `json:"latestHash"`
	FilesDir      string        `json:"filesDir"`
	FilesDirError string        `json:"filesDirError"`
	DevInstall    bool          `json:"devInstall"`
	Installs      []InstallInfo `json:"installs"`
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
	a.refreshInstallsLocked()
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

func (a *InstallerApp) Install(index int) OperationResult {
	return a.runInstallOperation(index, false)
}

func (a *InstallerApp) Repair(index int) OperationResult {
	return a.runInstallOperation(index, true)
}

func (a *InstallerApp) runInstallOperation(index int, repair bool) OperationResult {
	a.mu.Lock()
	defer a.mu.Unlock()

	if err := waitForGithub(); err != nil {
		return a.failureLocked("تعذر الوصول إلى ملفات Vencord", err)
	}
	install, err := a.installAt(index)
	if err != nil {
		return a.failureLocked("تعذر تحديد نسخة Discord", err)
	}
	if CheckScuffedInstall() {
		return a.failureLocked("تثبيت Discord يحتاج إصلاحا", errors.New("اكتشفنا تثبيت Discord غير سليم. أعد تثبيت Discord ثم حاول مجددا"))
	}

	if repair && !IsDevInstall {
		if err := installLatestBuilds(); err != nil {
			return a.failureLocked("فشل تنزيل أحدث ملفات Vencord Arabic", err)
		}
	}
	if err := install.patch(); err != nil {
		return a.failureLocked("فشل تثبيت Vencord Arabic", humaniseInstallerError(err))
	}

	a.refreshInstallsLocked()
	title := "تم التثبيت"
	message := "تم تثبيت Vencord Arabic بنجاح. أغلق Discord بالكامل ثم افتحه من جديد"
	if repair {
		title = "تم الإصلاح"
		message = "تم تحديث ملفات Vencord Arabic وإعادة تثبيتها بنجاح"
	}
	return OperationResult{OK: true, Title: title, Message: message, Status: a.statusLocked()}
}

func (a *InstallerApp) Uninstall(index int) OperationResult {
	a.mu.Lock()
	defer a.mu.Unlock()

	install, err := a.installAt(index)
	if err != nil {
		return a.failureLocked("تعذر تحديد نسخة Discord", err)
	}
	if !install.isPatched {
		return a.failureLocked("Vencord غير مثبت", errors.New("النسخة المحددة لا تبدو مثبتا عليها Vencord"))
	}
	if err := install.unpatch(); err != nil {
		return a.failureLocked("فشل إلغاء التثبيت", humaniseInstallerError(err))
	}

	a.refreshInstallsLocked()
	return OperationResult{
		OK:      true,
		Title:   "تم إلغاء التثبيت",
		Message: "تمت إعادة Discord إلى حالته الأصلية. أغلقه بالكامل ثم افتحه من جديد",
		Status:  a.statusLocked(),
	}
}

func (a *InstallerApp) ToggleOpenAsar(index int) OperationResult {
	a.mu.Lock()
	defer a.mu.Unlock()

	install, err := a.installAt(index)
	if err != nil {
		return a.failureLocked("تعذر تحديد نسخة Discord", err)
	}

	if install.IsOpenAsar() {
		err = install.UninstallOpenAsar()
	} else {
		err = install.InstallOpenAsar()
	}
	if err != nil {
		return a.failureLocked("تعذر تغيير حالة OpenAsar", humaniseInstallerError(err))
	}

	a.refreshInstallsLocked()
	return OperationResult{OK: true, Title: "تم", Message: "تم تحديث OpenAsar بنجاح", Status: a.statusLocked()}
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

func (a *InstallerApp) installAt(index int) (*DiscordInstall, error) {
	if index < 0 || index >= len(a.installs) {
		return nil, fmt.Errorf("invalid Discord install index: %d", index)
	}
	return a.installs[index], nil
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
	// Wails surfaces this through OperationResult instead of opening an ImGui popup.
}

func humaniseInstallerError(err error) error {
	if !errors.Is(err, os.ErrPermission) {
		return err
	}
	if runtime.GOOS == "windows" {
		return errors.New("لا توجد صلاحية لتعديل ملفات Discord. أغلق Discord بالكامل من شريط النظام ثم حاول مجددا")
	}
	return err
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
		return errors.New("تعذر جلب معلومات الإصدار من GitHub")
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
		Width:            1080,
		Height:           720,
		MinWidth:         880,
		MinHeight:        600,
		Frameless:        true,
		DisableResize:    false,
		BackgroundColour: &options.RGBA{R: 12, G: 14, B: 20, A: 0},
		AssetServer:      &assetserver.Options{Assets: wailsAssets},
		OnStartup:        app.startup,
		Bind:             []interface{}{app},
		Windows: &windows.Options{
			WebviewIsTransparent: true,
			WindowIsTranslucent:  true,
			BackdropType:         windows.Acrylic,
			DisableWindowIcon:    false,
			Theme:                windows.Dark,
		},
	})
	if err != nil {
		Log.Error("Wails UI failed:", err)
	}
}
