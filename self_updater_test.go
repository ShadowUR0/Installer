package main

import "testing"

func TestReleaseSourceCommit(t *testing.T) {
    const hash = "9f2090b232b7691f2806aa6570ef2d0b321dd296"

    tests := []struct {
        name string
        body string
        want string
    }{
        {"rolling release", "Installer release.\n\nSource commit: " + hash + "\n", hash},
        {"no source commit", "Release notes without revision", ""},
        {"invalid revision", "Source commit: not-a-commit", ""},
        {"short revision", "Source commit: 9f2090b2", ""},
        {"invalid hex", "Source commit: 9f2090b232b7691f2806aa6570ef2d0b321dd29x", ""},
    }

    for _, tc := range tests {
        t.Run(tc.name, func(t *testing.T) {
            if got := releaseSourceCommit(tc.body); got != tc.want {
                t.Fatalf("releaseSourceCommit() = %q; want %q", got, tc.want)
            }
        })
    }
}
