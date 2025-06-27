# Changelog

All notable changes to the Second Screen Android client will be documented in this file.

## [0.1.0] - 2025-06-24

### Added
- Initial implementation of Android second-screen client
- UDP receiver for H.264 streaming on port 5004
- MediaCodec-based hardware decoding with low latency focus
- XOR-based Forward Error Correction for packet loss recovery
- Touch event capture and transmission back to host
- Support for three display modes:
  - 1920×1080 @ 60Hz (Full HD)
  - 1752×2800 @ 60Hz (Tablet native)
  - 1752×2800 @ 120Hz (High refresh rate)
- Persistent notification with current display information
- Settings UI for manual display mode selection
- USB tethering support for reliable connection
