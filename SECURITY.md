# Security policy

Light OS is a hobby project that is meant to run on a **trusted home network**. Please read the "Security notes" section of the [README](README.md):
the web server has no password and must never be exposed to the internet.

## Reporting a vulnerability

If you find a security problem (for example a way to crash the device remotely or to read data that should be private), please **do not open a public issue**.
Email **mehrdad2200@gmail.com** with:
- a description of the problem and how to reproduce it,
- the firmware version (see `CHANGELOG.md`),
- your board and network setup if relevant.

You will get a reply as soon as possible. Fixes are published in a new release and mentioned in `CHANGELOG.md`.

## Never share

- Your Wi-Fi name and password (`secrets.h`).
- Photos or logs that show them.
