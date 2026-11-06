# Build one module: protocol, app, or dext.
build module='':
    #!/usr/bin/env bash
    set -euo pipefail

    xcode() {
        local sign=()
        if [[ -n "${DEVELOPMENT_TEAM:-}" ]] || grep -Eq 'DEVELOPMENT_TEAM = [A-Z0-9]+;' DS620.xcodeproj/project.pbxproj; then
            if [[ -n "${DEVELOPMENT_TEAM:-}" ]]; then
                sign+=(DEVELOPMENT_TEAM="$DEVELOPMENT_TEAM")
            fi
        else
            echo "No development team set; compiling without code signing." >&2
            sign+=(CODE_SIGNING_ALLOWED=NO)
        fi
        local args=(
            -project DS620.xcodeproj
            -configuration Debug
            -destination 'platform=macOS,arch=arm64'
            -derivedDataPath build/xcode
            "$@"
        )
        if ((${#sign[@]} > 0)); then
            args+=("${sign[@]}")
        fi
        if [[ " ${sign[*]-} " != *"CODE_SIGNING_ALLOWED=NO"* ]]; then
            args+=(-allowProvisioningUpdates)
        fi
        xcodebuild "${args[@]}" build
    }

    case "{{module}}" in
        protocol)
            make test
            ;;
        app)
            xcode -scheme DS620
            ;;
        dext)
            xcode -scheme DS620Driver
            ;;
        *)
            echo "usage: just build {protocol|app|dext}" >&2
            exit 1
            ;;
    esac
