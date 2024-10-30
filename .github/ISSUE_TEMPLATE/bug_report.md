---
name: Bug report
about: Create a report to help us improve
title: ''
labels: bug
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.

**Log**

- For compile errors, run `hyprpm update -f -v --no-shallow` or for nix/home-manager/nixos-rebuild: add the `-L` flag.

- For runtime crashes, enable logging with `hyprctl keyword debug:disable_logs false` then send the crash report in `.cache/hyprland/hyprlandCrashReport{pid}.txt`

- For other runtime bugs, if you see something useful in logs after enabling them, also attach it

```
paste log here...
```

**Screenshots**
If applicable, add screenshots to help explain your problem.

**Desktop (please complete the following information):**
 - OS: [e.g. Arch, nix/nixos]
 - output of `hyprctl version` or `Hyprland -v`:
   ```
   Hyprland v0.0.0 built from ...
   ```
- output of `hyprctl plugin list`:
   ```
   Plugin hyprgrass...
   ```

**Additional context**
Add any other context about the problem here.
