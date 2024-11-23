#!/bin/sh

set -eu

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

git stash push
origCommit="$(git rev-parse HEAD)"
git checkout "${hyprgrassRev}"

nix build --no-link .#hyprgrassWithTests \
	--override-input hyprland "github:hyprwm/Hyprland/${hyprlandRev}" \

git checkout "$origCommit"
git stash pop

echo "[\"${hyprlandCommit}\", \"${hyprgrassCommit}\"], # ${hyprlandRev}"
