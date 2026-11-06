# DSmobile620

A macOS driver for the Brother DS-620 / DSmobile 620 USB sheetfed scanner.

Brother's official Mac software stops at macOS 14 and does not include an Image Capture driver. This repository is an independent [DriverKit](https://developer.apple.com/documentation/driverkit) extension plus a user-space protocol library. It is a personal project. Contributions are welcome: bug reports, SCSI captures, and pull requests.

## Layout

The DS-620 uses USB mass storage as a pipe and the Avision SCSI scanner command set. The extension claims interface 0 and exposes the two bulk pipes. Scanner commands stay in the user-space library, which does not link IOKit, so the protocol can be tested without reloading the extension.

| Path | Role |
|---|---|
| `dext/` | DriverKit extension `dev.dsmobile620.driver`. Bulk out, bulk in, and abort. |
| `protocol/` | C++ library. `Scanner::inquiry` speaks SCSI INQUIRY over bulk-only transport. |
| `app/` | `DS620` command-line tool inside an app bundle. |

`inquiry --mock` uses the simulator. `inquiry` activates the extension and then sends SCSI INQUIRY.

## Build

You need full Xcode and [just](https://github.com/casey/just). Loading the extension on a Mac also needs a paid Apple Developer Program membership. The app target is macOS 26.6. The extension target is DriverKit 25.6.

```sh
just build protocol
just build dext
just build app
```

`just build protocol` compiles the library and runs the inquiry test against the simulator.

## Device

The scanner this code was written against is USB `04f9:60e0`, `bcdDevice` `0x0100`, product string `DS-620`, SCSI revision `0.40`. Interface 0 is class 8, subclass 6, protocol `0x50`, with two bulk endpoints.

## License

[MIT](LICENSE).
