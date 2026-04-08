# Plasma Wallpaper Immich

KDE Plasma wallpaper plugin that fetches photos from an [Immich](https://immich.app/) server and rotates them as your desktop background.

## Features

- Authenticate using API key (recommended) or email/password
- Show photos from selected albums, selected tags, favorites, or all photos
- Configurable slideshow interval
- Fade transition between photos
- Wallpaper fill mode and fallback background color controls

## Requirements

- KDE Plasma 6
- Qt 6 development tools
- CMake 3.22+
- A running Immich instance with API access

On Arch Linux, typical build dependencies are:

- `cmake`
- `extra-cmake-modules`
- `qt6-base`
- `qt6-declarative`
- `plasma-framework`
- `zip` (for creating `.plasmoid` archives)

Package names vary by distribution.

## Build and Install

### Option 1: Use helper script

```bash
./install.sh
```

This performs a local CMake build and installs to your user prefix (`$HOME/.local`).

### Option 2: Manual CMake build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build -j"$(nproc)"
cmake --install build
```

After installing, restart Plasma shell or log out/in if the wallpaper does not immediately appear.

## Packaging and Development Helpers

- `./package-plasmoid.sh` creates a distributable `.plasmoid` package from `package/`
- `./kpac` provides small wrappers around common package actions
- `release-please-config.json` and `.release-please-manifest.json` are included for automated releases

## Usage

1. Open **Configure Desktop and Wallpaper**
2. Select **Immich** as wallpaper type
3. Enter your server URL and authentication details
4. Choose album/tag/favorites/all mode
5. Set slideshow interval and appearance options

## Security Notes

- API key mode is preferred over storing account password
- If you use password mode, your password is stored in Plasma wallpaper configuration

## License

GPL-2.0-or-later
