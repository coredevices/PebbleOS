# Security Policy

## Supported Versions

Only the firmware version currently published to watches through the official
Pebble mobile app is supported. This is not necessarily the latest GitHub
release. A security fix ships either as a fix release of that version (e.g.
v4.38.4 -> v4.38.5) or in a new minor or major release, also through the
mobile app. Security updates are free of charge and are released without
undue delay.

## Reporting a Vulnerability

Please do not report security vulnerabilities in public issues, pull requests
or forum posts. Report them privately in either of these ways:

- GitHub: open the repository's
  [Security tab](https://github.com/coredevices/pebbleos/security) and select
  **Report a vulnerability**.
- Email: security@repebble.com

Include:

- the affected firmware version and watch model
- a description of the issue and its impact
- steps or a minimal app to reproduce it, if you have one

We will acknowledge the report, keep you updated while we investigate, and
credit you in the published advisory unless you ask us not to.

## Scope

This policy covers the PebbleOS firmware and SDK in this repository: for
example, a third-party app or watchface escaping the app sandbox, reading or
writing kernel memory, or crashing the watch through a system call.

Vulnerabilities in the official Pebble mobile app or in the online services
the watch relies on should be reported to security@repebble.com.

## Third-Party Components

PebbleOS includes third-party components, such as NimBLE, Moddable and Mbed
TLS. If a vulnerability is in one of them, we report it to its maintainers and
coordinate the fix and disclosure with them. Where required, we also
coordinate with the relevant national computer security incident response
teams (CSIRTs).

## Disclosure

We fix confirmed vulnerabilities and publish a GitHub security advisory once a
release containing the fix is available through the mobile app. Each advisory
describes the vulnerability, the affected versions, its impact and severity,
and how to get the fix, and references a CVE identifier where one is
assigned. Please do not disclose the issue publicly before then.
