#!/bin/sh

set -eux

echoerr () {
	echo $@ >&2
}

if [ -z "${1:-}" ]; then
	echoerr "Usage:"
	echoerr ""
	echoerr "	test-pin HYPRLAND_REV [HYPRGRASS_REV]"
	echoerr ""
	echoerr "Runs tests with specified rev and prints the commit SHA of both repo"
	echoerr ""
	echoerr "HYPRGRASS_REV defaults to 'main'"
	exit 1
fi

hyprlandRev=${1}
hyprgrassRev=${2:-main}

hyprlandCommit="$(git ls-remote https://github.com/hyprwm/Hyprland.git "${hyprlandRev}" | cut -f 1)"
hyprgrassCommit="$(git rev-parse "${hyprgrassRev}")"

hlUrl="github:hyprwm/Hyprland/${hyprlandRev}" 

worktreeDir="pin-build"

{
	git worktree add "$worktreeDir" "$hyprgrassCommit"
	trap "git worktree remove --force '$worktreeDir'" EXIT INT HUP TERM
	cd "$worktreeDir"
	git fetch --unshallow

	# NOTE: nix build --override-input does not override input of inputs, i.e. hyprland/aquamarine
	# will not be updated, hence we need to run `flake update` :<
	nix flake update --override-input hyprland "$hlUrl"
	nix build --no-link '.#hyprgrassWithTests'

	cd ..
} >&2 # avoid cluttering stdout

echo "[\"${hyprlandCommit}\", \"${hyprgrassCommit}\"], # ${hyprlandRev}"
