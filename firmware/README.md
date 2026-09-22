# Firmware downloads

Ready-to-flash firmware images are published on the project's [GitHub Releases](https://github.com/pjalocha/nrf52-ogn-tracker/releases) page.

Do not use a firmware image intended for another device. The release assets are named by PlatformIO environment:

```text
nrf52-ogn-tracker-T-Echo-vX.Y.Z.uf2
nrf52-ogn-tracker-Wio-Tracker-vX.Y.Z.uf2
```

Each release also contains `SHA256SUMS` for verifying the downloaded files and release notes describing the changes. The UF2 image is the file to copy to the device's bootloader USB drive.

## Creating a release

Create and push an annotated version tag. The numeric part becomes the
firmware `VERSION` reported by the device:

```bash
git tag -a v0.1.36 -m "Release v0.1.36"
git push origin v0.1.36
```

The GitHub Actions release workflow builds the tagged source and publishes the T-Echo and Wio-Tracker UF2 images automatically.
