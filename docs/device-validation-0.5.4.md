# Cardputer Zero device validation — 0.5.4-rev1

Validated 2026-11-24 against commit `55ed6d5f0915a5fba1c63a211f9626f9c5c9e7fe` on `JCMKCardputerZero`.

## Build and installation evidence

- GitHub Actions run `45417384784` succeeded for desktop job `138527211282` and ARM64 job `138527210271`.
- Artifact `14933325831` supplied Debian package `marauder 0.5.4-rev1` for ARM64.
- Debian SHA-256: `5be5c19dccd9b4880e53fcab8b8750992d2d9f8af617acb4f04db903505ed3c7`.
- Installed binary SHA-256: `2a199fb7095a5694ecaaa1fa4cb9765dff383ada9088410da663b514d01dfde7`; it matched the CI package.
- `dpkg` audit and dynamic-library resolution passed. A six-second Wayland launch smoke remained alive with an empty error log and left no process behind.
- `wlan1` remained NetworkManager-managed, disconnected, and operationally down during installation and smoke validation.

## Physical acceptance

The owner physically confirmed the classic menu/navigation correction and the final APPLaunch icon presentation. The accepted controls use unmodified `F` = Up, `X` = Down, `Z` = Left, and `C` = Right. The accepted launcher icon is the smaller antialiased white JCMK LLC mark on black.

This record is hardware evidence, not an automated CI result. Injection support remains unverified and was not exercised.
