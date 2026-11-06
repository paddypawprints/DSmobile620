---
name: dsmobile620-driver
description: >-
  Build the native macOS driver for the Brother DS-620 / DSmobile 620 USB
  scanner. Use when working in this repo, or when the user mentions the
  DS-620, DSmobile 620, 04f9:60e0, DriverKit dext, bulk-only transport, or
  the Avision scanner command set.
---

# DS-620 macOS driver

The scanner firmware already exists (revision `0.40`). This project is a DriverKit extension plus a user-space protocol library. Do not treat the extension as device firmware.

## Architecture

The DS-620 is a scanner that uses USB mass storage as a pipe. Claim the interface and speak bulk-only transport. Do not send scanner commands through Apple's disk stack.

- The dext owns interface 0 and the two bulk pipes. Its user client exposes bulk out, bulk in, and abort.
- The app-side C++ library implements bulk-only transport (command wrapper, data stage, status wrapper) and the Avision command sequence. It stays free of IOKit so it can be rebuilt and tested without reloading the extension.
- First program is a command-line tool inside the app bundle. Image Capture is a later front end.

Match the dext on `IOUSBHostInterface`, not only the device, so it beats `IOUSBMassStorageDriver`. The USB family assigns the probe score from the personality keys and ignores an explicit `IOProbeScore`. Class 8 plus subclass 6, which is all that mass-storage personality matches, scores 50000. Vendor, product, interface number, configuration, and `bcdDevice` together score 100000, so the dext personality uses that set and does not also list the class, subclass, or protocol:

- `idVendor` `1273`, `idProduct` `24800`
- `bInterfaceNumber` `0`, `bConfigurationValue` `1`, `bcdDevice` `256`

`IOUSBMassStorageInterfaceNub` is the exclusive owner until that dext is installed and the scanner is replugged. A normal `IOUSBHost` or libusb open from an app will fail while that nub owns the interface. There is no supported user-space SCSI pass-through on this Mac. Finder stays empty even when enumeration works: inquiry succeeds, no media, no `/dev/disk`.

## Device

Probed on this Mac (direct USB-C, 480 Mb/s, 500 mA):

| Field | Value |
|---|---|
| USB id | `04f9:60e0`, `bcdDevice` `0x0100` |
| Strings | Brother, DS-620, serial `U63543M5U122370` |
| Interface | class 8, subclass 6, protocol `0x50`, two endpoints, config 1, interface 0 |
| SCSI inquiry | vendor `Brother`, product `DS-620`, revision `0.40`, peripheral type 0 |
| Mass-storage note | `No INQUIRY VPD Pages` |

The closed Linux backend `libsane-dsseries` (`dsdrv`, `NvUSBScan`) is an Avision stack. Symbols include `Inquiry`, `SetWindow`, `StartScan`, calibration, and ADF status. The command family is the Avision SCSI scanner set (the ancestor of SANE's `avision` backend), not Brother's printer-scanner protocol. Stock SANE does not claim this USB id. The exact Set Window block, pixel layout, and calibration bytes are still unknown. Recover them from a SCSI capture or from `dsdrv`, then implement them in the user-space library.

Brother ships no driver for this Mac. Official Mac downloads stop at macOS 14, TWAIN plus DSmobileCapture, no ICA. VueScan has no DS-620 page. Linux `libsane-dsseries` 1.0.5 is x86-64 only.

## Language split

The dext is DriverKit C++17, the IOKit subset. Follow [Creating a Driver Using the DriverKit SDK](https://developer.apple.com/documentation/driverkit/creating-a-driver-using-the-driverkit-sdk) and the IOKit C++ rules in [C++ Language Considerations](https://developer.apple.com/library/archive/documentation/Porting/Conceptual/PortingDrivers/other-language/other-language.html).

Inside the dext:

- Class interface lives in the `.iig` file. Behavior lives in the `.cpp`.
- Lifecycle is `init`, `Start`, `Stop`, `free`. Public methods use IOKit names (`Start`, `Stop`, `NewUserClient`).
- No exceptions. Return `kern_return_t` and check it.
- No standard RTTI, no `std::` library, no libc++. No non-trivial constructors. Allocate in `init`. If `init` fails, the caller releases the object.
- Reference counting. A method with `Get` in the name returns at +0. Other returned objects are +1 and the caller releases them. `Copy` is the +1 form.
- Fixed-width integers at the USB boundary. Log through the DriverKit log facility.
- The dext does not touch the file system, the network, or arbitrary IPC.

The protocol library is ordinary C++. Exceptions, the standard library, and a normal test harness belong there.

## Signing and tools

A DriverKit dext needs full Xcode (`xcodebuild` from `Xcode.app`). Command Line Tools alone cannot build or embed the extension. Before creating the dext target, confirm `xcode-select -p` points at an `Xcode.app` bundle.

Entitlements, from a paid Apple Developer profile:

- dext: `com.apple.developer.driverkit`, `com.apple.developer.driverkit.transport.usb`. The development profile grants `idVendor` as the string `*`. The signed entitlement has to use that same string or amfid rejects the dext at launch with "No matching profile found" for the USB transport entitlement. The personality, not the entitlement, limits the match to vendor `1273`.
- app: `com.apple.developer.system-extension.install`
- dext also has `com.apple.developer.driverkit.allow-any-userclient-access`, which the development profile grants. On this Mac that is not enough: `mac_iokit_check_open_service` rejects `IOServiceOpen` with `kIOReturnNotPermitted` before the dext entitlement is consulted. The app needs `com.apple.developer.driverkit.userclient-access` listing `dev.dsmobile620.driver`, and Apple has to assign that capability to the app id before a profile can contain it.

`systemextensionsctl developer on` only skips the same-version replacement check, and on this OS it refuses to run while System Integrity Protection is enabled. It does not grant DriverKit entitlements. A free Apple ID, ad-hoc signature, or SIP change does not substitute for the profile.

`inquiry` from `DS620.app` submits the activation request and waits. After the user allows the extension, replug the scanner so the dext takes interface 0.

## Repository

One repo, three pieces:

- `protocol/` is the IOKit-free C++ library. `Scanner::inquiry` speaks SCSI INQUIRY over bulk-only transport. `just build protocol` compiles it and runs it against `SimulatedTransport`.
- `dext/` is the DriverKit extension (`dev.dsmobile620.driver`). It matches interface `04f9:60e0` class 8 / subclass 6 / protocol 80 and exposes bulk out, bulk in, and abort. `just build dext` compiles it.
- `app/` is the `DS620` app target. `inquiry --mock` uses the simulator. `inquiry` submits an `OSSystemExtensionRequest` for `dev.dsmobile620.driver`, waits for approval, then opens the dext user client. `just build app` builds the app and embeds the dext.

`DS620.xcodeproj` is what `just build app` and `just build dext` call. The app deployment target is macOS 26.6. The dext deployment target is DriverKit 25.6: on this Mac the running system extensions are 25.6, and Xcode 27.2 rewrites a DriverKit 26.x target into 27.x, which will not load here. With no development team selected, those two recipes compile without code signing.

Do not add a second driver stack.

## Milestones

1. INQUIRY through the dext path prints `Brother`, `DS-620`, and `0.40`.
2. One grayscale page at 300 dpi written as a PGM.
3. Color, calibration, and long-page behavior after the grayscale path is solid.

Confirm the device is present with `system_profiler SPUSBHostDataType` before debugging the protocol. On macOS 26 the data type is `SPUSBHostDataType`, not `SPUSBDataType`. A successful enum is `Brother` `DS-620` at 480 Mb/s, vendor `0x04f9`, product `0x60e0`.
