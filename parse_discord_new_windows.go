//go:build windows

package main

// ParseDiscordNew is only a distinct parser on Linux. On Windows, Discord's
// Squirrel layout is already handled by ParseDiscord, so the CLI fallback can
// safely reuse it.
func ParseDiscordNew(p, branch string, _ bool) *DiscordInstall {
	return ParseDiscord(p, branch)
}
