#!/usr/bin/env bash
# Uses bash for pipefail / PIPESTATUS (GitHub macOS /bin/sh may not be bash).

# Optional: xcpretty https://github.com/xcpretty/xcpretty (CI falls back to plain tee)

BASEDIR=$(dirname "$0")

cd "$BASEDIR/.."

#---------------------------------------------------------------------------------------------------------
#variables

IPLUG2_ROOT=../iPlug2
XCCONFIG=$IPLUG2_ROOT/../common-mac.xcconfig
SCRIPTS=$IPLUG2_ROOT/Scripts

# CODESIGN disabled by default. 
CODESIGN=0

# macOS codesigning/notarization
NOTARIZE_BUNDLE_ID=com.Lum.VoLum
NOTARIZE_BUNDLE_ID_DEMO=com.Lum.VoLum.DEMO
APP_SPECIFIC_ID=TODO
APP_SPECIFIC_PWD=TODO

# AAX/PACE wraptool codesigning
ILOK_ID=TODO
ILOK_PWD=TODO
WRAP_GUID=TODO

DEMO=0
BUILD_INSTALLER=1
# When 1 with BUILD_INSTALLER, also build standalone DMG + VST3 zip after the installer (`full all`).
PACKAGE_ZIP=0
FAST_DEV=0

for arg in "$@"; do
  case "$arg" in
    demo)
      DEMO=1
      ;;
    full)
      ;;
    zip)
      BUILD_INSTALLER=0
      ;;
    installer)
      BUILD_INSTALLER=1
      PACKAGE_ZIP=0
      ;;
    all)
      BUILD_INSTALLER=1
      PACKAGE_ZIP=1
      ;;
    dev|fast)
      FAST_DEV=1
      BUILD_INSTALLER=0
      ;;
  esac
done

CLEAN_BUILD=1
FORCE_RECOMPILE=1
# Default 1; set PACKAGE_DSYMS=0 to skip *-dSYMs.zip (e.g. release CI). FAST_DEV always skips.
PACKAGE_DSYMS="${PACKAGE_DSYMS:-1}"
DMG_FORMAT=UDZO

if [ "$FAST_DEV" == "1" ]; then
  CLEAN_BUILD=0
  FORCE_RECOMPILE=0
  PACKAGE_DSYMS=0
  DMG_FORMAT=UDRW
fi

# Use last matching line so duplicate defines cannot desync archive names vs get_archive_name.py / CI verify.
VERSION_LINE=$(grep '^#define PLUG_VERSION_HEX' config.h | tail -n 1)
VERSION=${VERSION_LINE//#define PLUG_VERSION_HEX }
VERSION=${VERSION//\'}
MAJOR_VERSION=$(($VERSION & 0xFFFF0000))
MAJOR_VERSION=$(($MAJOR_VERSION >> 16))
MINOR_VERSION=$(($VERSION & 0x0000FF00))
MINOR_VERSION=$(($MINOR_VERSION >> 8))
BUG_FIX=$(($VERSION & 0x000000FF))

FULL_VERSION=$MAJOR_VERSION"."$MINOR_VERSION"."$BUG_FIX

PLUGIN_LINE=$(grep '^#define BUNDLE_NAME' config.h | tail -n 1)
PLUGIN_NAME=${PLUGIN_LINE//#define BUNDLE_NAME }
PLUGIN_NAME=${PLUGIN_NAME//\"}

# Project/config files keep the upstream "NeuralAmpModeler" prefix even though
# BUNDLE_NAME (and therefore PLUGIN_NAME) is now "VoLum".
PROJECT_PREFIX=NeuralAmpModeler
ICON_NAME=icons/$PLUGIN_NAME

ARCHIVE_NAME=$PLUGIN_NAME-v$FULL_VERSION-mac

if [ $DEMO == 1 ]; then
  ARCHIVE_NAME=$ARCHIVE_NAME-demo
fi

if [ "$FAST_DEV" == "1" ]; then
  ARCHIVE_NAME=$ARCHIVE_NAME-dev
fi

# TODO: use get_archive_name script
# if [ $DEMO == 1 ]; then
#   ARCHIVE_NAME=`python3 ${SCRIPTS}/get_archive_name.py ${PLUGIN_NAME} mac demo`
# else
#   ARCHIVE_NAME=`python3 ${SCRIPTS}/get_archive_name.py ${PLUGIN_NAME} mac full`
# fi

VST2=`echo | grep VST2_PATH $XCCONFIG`
VST2=$HOME${VST2//\VST2_PATH = \$(HOME)}/$PLUGIN_NAME.vst

VST3=`echo | grep VST3_PATH $XCCONFIG`
VST3=$HOME${VST3//\VST3_PATH = \$(HOME)}/$PLUGIN_NAME.vst3

AU=`echo | grep AU_PATH $XCCONFIG`
AU=$HOME${AU//\AU_PATH = \$(HOME)}/$PLUGIN_NAME.component

APP=`echo | grep APP_PATH $XCCONFIG`
APP=$HOME${APP//\APP_PATH = \$(HOME)}/$PLUGIN_NAME.app

# Dev build folder
AAX=`echo | grep AAX_PATH $XCCONFIG`
AAX=${AAX//\AAX_PATH = }/$PLUGIN_NAME.aaxplugin
AAX_FINAL="/Library/Application Support/Avid/Audio/Plug-Ins/$PLUGIN_NAME.aaxplugin"

PKG="build-mac/installer/$PLUGIN_NAME Installer.pkg"
PKG_US="build-mac/installer/$PLUGIN_NAME Installer.unsigned.pkg"
RIGS_SRC="$(pwd)/../rigs"

CERT_ID=`echo | grep CERTIFICATE_ID $XCCONFIG`
CERT_ID=${CERT_ID//\CERTIFICATE_ID = }
DEV_ID_APP_STR="Developer ID Application: ${CERT_ID}"
DEV_ID_INST_STR="Developer ID Installer: ${CERT_ID}"

echo $VST2
echo $VST3
echo $AU
echo $APP
echo $AAX

if [ "$CLEAN_BUILD" == "1" ] && [ -d build-mac ]; then
  rm -f -R build-mac
fi

if [ $DEMO == 1 ]; then
  if [ "$FAST_DEV" == "1" ]; then
    echo "making $PLUGIN_NAME version $FULL_VERSION DEMO mac development build..."
  else
    echo "making $PLUGIN_NAME version $FULL_VERSION DEMO mac distribution..."
  fi
#   cp "resources/img/AboutBox_Demo.png" "resources/img/AboutBox.png"
else
  if [ "$FAST_DEV" == "1" ]; then
    echo "making $PLUGIN_NAME version $FULL_VERSION mac development build..."
  else
    echo "making $PLUGIN_NAME version $FULL_VERSION mac distribution..."
  fi
#   cp "resources/img/AboutBox_Registered.png" "resources/img/AboutBox.png"
fi

if [ "$FAST_DEV" != "1" ]; then
  sleep 2
fi

if [ "$FORCE_RECOMPILE" == "1" ]; then
  echo "touching source to force recompile"
  echo ""
  touch *.cpp
fi

#---------------------------------------------------------------------------------------------------------
#remove existing binaries

echo "remove existing binaries"
echo ""

if [ -d $APP ]; then
  rm -f -R -f $APP
fi

if [ -d $AU ]; then
 rm -f -R $AU
fi

if [ -d $VST2 ]; then
  rm -f -R $VST2
fi

if [ -d $VST3 ]; then
  rm -f -R $VST3
fi

if [ -d "${AAX}" ]; then
  sudo rm -f -R "${AAX}"
fi

if [ -d "${AAX_FINAL}" ]; then
  sudo rm -f -R "${AAX_FINAL}"
fi

#---------------------------------------------------------------------------------------------------------
# build xcode project. Change target to build individual formats, or add to All target in the xcode project

# GitHub Actions: no Apple dev certs. APP/AUv3 targets use Automatic + DEVELOPMENT_TEAM in the
# xcodeproj; command-line overrides must clear entitlements for CI or Xcode errors:
# "APP isn't code signed but requires entitlements".
HOST_ARCH=$(uname -m)
XC_EXTRA=(
  ARCHS="arm64 x86_64"
  ONLY_ACTIVE_ARCH=NO
)
if [ "$FAST_DEV" == "1" ]; then
  XC_EXTRA=(
    ARCHS="$HOST_ARCH"
    ONLY_ACTIVE_ARCH=YES
  )
fi
if [ "$CODESIGN" != "1" ]; then
  XC_EXTRA=(
    "${XC_EXTRA[@]}"
    CODE_SIGN_IDENTITY=-
    CODE_SIGN_STYLE=Manual
    DEVELOPMENT_TEAM=
    CODE_SIGN_ENTITLEMENTS=
    ENABLE_HARDENED_RUNTIME=NO
  )
fi

# Default to the deliverables we actually ship and test. AU still uses legacy
# Carbon Resources/Rez and is currently best treated as opt-in work.
XCODE_TARGETS=( -target "VST3" -target "APP" )
if [ "$FAST_DEV" == "1" ]; then
  XCODE_TARGETS=( -target "APP" )
fi
if [ "${MACOS_BUILD_ALL_TARGETS:-0}" = "1" ]; then
  XCODE_TARGETS=( -target "All" )
fi

set -o pipefail
if command -v xcpretty >/dev/null 2>&1; then
  xcodebuild -project ./projects/$PROJECT_PREFIX-macOS.xcodeproj -xcconfig ./config/$PROJECT_PREFIX-mac.xcconfig DEMO_VERSION=$DEMO "${XCODE_TARGETS[@]}" -UseModernBuildSystem=NO -configuration Release "${XC_EXTRA[@]}" 2>&1 | tee build-mac.log | xcpretty
else
  xcodebuild -project ./projects/$PROJECT_PREFIX-macOS.xcodeproj -xcconfig ./config/$PROJECT_PREFIX-mac.xcconfig DEMO_VERSION=$DEMO "${XCODE_TARGETS[@]}" -UseModernBuildSystem=NO -configuration Release "${XC_EXTRA[@]}" 2>&1 | tee build-mac.log
fi
XC_EC=$?
set +o pipefail

if [ "$XC_EC" -ne 0 ]; then
  echo "ERROR: build failed, aborting"
  echo "---- build-mac.log (tail) ----"
  tail -n 200 build-mac.log 2>/dev/null || true
  echo "------------------------------"
  exit 1
else
  rm -f build-mac.log
fi

#---------------------------------------------------------------------------------------------------------
# set bundle icons - http://www.hamsoftengineering.com/codeSharing/SetFileIcon/SetFileIcon.html

echo "setting icons"
echo ""

if [ -d $AU ]; then
  ./$SCRIPTS/SetFileIcon -image resources/$ICON_NAME.icns -file $AU
fi

if [ -d $VST2 ]; then
  ./$SCRIPTS/SetFileIcon -image resources/$ICON_NAME.icns -file $VST2
fi

if [ -d $VST3 ]; then
  ./$SCRIPTS/SetFileIcon -image resources/$ICON_NAME.icns -file $VST3
fi

if [ -d "${AAX}" ]; then
  ./$SCRIPTS/SetFileIcon -image resources/$ICON_NAME.icns -file "${AAX}"
fi

#---------------------------------------------------------------------------------------------------------
#strip symbols from binaries

echo "stripping binaries"
echo ""

if [ -d $APP ]; then
  strip -x $APP/Contents/MacOS/$PLUGIN_NAME
fi

if [ -d $AU ]; then
  strip -x $AU/Contents/MacOS/$PLUGIN_NAME
fi

if [ -d $VST2 ]; then
  strip -x $VST2/Contents/MacOS/$PLUGIN_NAME
fi

if [ -d $VST3 ]; then
  strip -x $VST3/Contents/MacOS/$PLUGIN_NAME
fi

if [ -d "${AAX}" ]; then
  strip -x "${AAX}/Contents/MacOS/$PLUGIN_NAME"
fi

if [ -d "$RIGS_SRC" ] && [ -d "$APP" ]; then
  echo "embedding VoLumRigs into app bundle..."
  rm -R -f "$APP/Contents/Resources/VoLumRigs"
  mkdir -p "$APP/Contents/Resources/VoLumRigs"
  for amp_dir in "$RIGS_SRC"/*/; do
    [ -d "$amp_dir" ] || continue
    amp_name=$(basename "$amp_dir")
    mkdir -p "$APP/Contents/Resources/VoLumRigs/$amp_name"
    cp "$amp_dir"*.nam "$APP/Contents/Resources/VoLumRigs/$amp_name/" 2>/dev/null || true
  done
fi

# Same sibling layout as the portable VST3 zip: ~/Library/Audio/Plug-Ins/VST3/VoLumRigs next to VoLum.vst3
if [ -d "$RIGS_SRC" ] && [ -d "$VST3" ]; then
  echo "copying VoLumRigs next to VST3 install..."
  VST3_PARENT=$(dirname "$VST3")
  rm -R -f "$VST3_PARENT/VoLumRigs"
  mkdir -p "$VST3_PARENT/VoLumRigs"
  for amp_dir in "$RIGS_SRC"/*/; do
    [ -d "$amp_dir" ] || continue
    amp_name=$(basename "$amp_dir")
    mkdir -p "$VST3_PARENT/VoLumRigs/$amp_name"
    cp "$amp_dir"*.nam "$VST3_PARENT/VoLumRigs/$amp_name/" 2>/dev/null || true
  done
elif [ -d "$VST3" ] && [ ! -d "$RIGS_SRC" ]; then
  echo "WARNING: rigs directory not found: $RIGS_SRC (VST3 install has no VoLumRigs copy)"
fi

if [ $CODESIGN == 1 ]; then
  #---------------------------------------------------------------------------------------------------------
  # code sign AAX binary with wraptool

  # echo "copying AAX ${PLUGIN_NAME} from 3PDev to main AAX folder"
  # sudo cp -p -R "${AAX}" "${AAX_FINAL}"
  # mkdir "${AAX_FINAL}/Contents/Factory Presets/"
  
  # echo "code sign AAX binary"
  # /Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool sign --verbose --account $ILOK_ID --password $ILOK_PWD --wcguid $WRAP_GUID --signid "${DEV_ID_APP_STR}" --in "${AAX_FINAL}" --out "${AAX_FINAL}"

  #---------------------------------------------------------------------------------------------------------

  #---------------------------------------------------------------------------------------------------------
  echo "code-sign binaries"
  echo ""

  codesign --force -s "${DEV_ID_APP_STR}" -v $APP --deep --strict --options=runtime #hardened runtime for app
  xattr -cr $AU 
  codesign --force -s "${DEV_ID_APP_STR}" -v $AU --deep --strict
  # xattr -cr $VST2 
  # codesign --force -s "${DEV_ID_APP_STR}" -v $VST2 --deep --strict
  xattr -cr $VST3 
  codesign --force -s "${DEV_ID_APP_STR}" -v $VST3 --deep --strict
  #---------------------------------------------------------------------------------------------------------
fi

if [ $BUILD_INSTALLER == 1 ]; then
  #---------------------------------------------------------------------------------------------------------
  # installer

  # Only remove the installer DMG name (wildcard VoLum-*.dmg also matched *-app.dmg and broke `full all`).
  rm -f "build-mac/${ARCHIVE_NAME}.dmg"

  echo "building installer"
  echo ""

  # makeinstaller-mac.sh packages from build-mac/<name>.{app,vst3}; Xcode installs to DSTROOT (~).
  echo "staging ${PLUGIN_NAME}.app and ${PLUGIN_NAME}.vst3 into build-mac/ for pkgbuild..."
  rm -rf "build-mac/${PLUGIN_NAME}.app" "build-mac/${PLUGIN_NAME}.vst3"
  if [ -d "$APP" ]; then
    cp -R "$APP" "build-mac/${PLUGIN_NAME}.app"
  else
    echo "WARNING: missing $APP — installer will omit standalone app"
  fi
  if [ -d "$VST3" ]; then
    cp -R "$VST3" "build-mac/${PLUGIN_NAME}.vst3"
  else
    echo "WARNING: missing $VST3 — installer will omit VST3"
  fi

  ./scripts/makeinstaller-mac.sh $FULL_VERSION

  if [ $CODESIGN == 1 ]; then
    echo "code-sign installer for Gatekeeper on macOS 10.8+"
    echo ""
    mv "${PKG}" "${PKG_US}"
    productsign --sign "${DEV_ID_INST_STR}" "${PKG_US}" "${PKG}"
    rm -R -f "${PKG_US}"
  fi

  #set installer icon
  ./$SCRIPTS/SetFileIcon -image resources/$ICON_NAME.icns -file "${PKG}"

  #---------------------------------------------------------------------------------------------------------
  # make dmg, can use dmgcanvas http://www.araelium.com/dmgcanvas/ to make a nice dmg, fallback to hdiutil
  echo "building dmg"
  echo ""

  if [ -d installer/$PLUGIN_NAME.dmgCanvas ]; then
    dmgcanvas installer/$PLUGIN_NAME.dmgCanvas build-mac/$ARCHIVE_NAME.dmg
  else
    cp installer/changelog.txt build-mac/installer/
    if [ -f installer/known-issues.txt ]; then
      cp installer/known-issues.txt build-mac/installer/
    fi
    if [ -f "manual/$PLUGIN_NAME manual.pdf" ]; then
      cp "manual/$PLUGIN_NAME manual.pdf" build-mac/installer/
    fi
    hdiutil create build-mac/$ARCHIVE_NAME.dmg -format UDZO -srcfolder build-mac/installer/ -ov -anyowners -volname $PLUGIN_NAME
  fi

  rm -R -f build-mac/installer/

  if [ $CODESIGN == 1 ]; then
    #---------------------------------------------------------------------------------------------------------
    #notarize dmg
    echo "notarizing"
    echo ""
    # you need to create an app-specific id/password https://support.apple.com/en-us/HT204397
    # arg 1 Set to the dmg path
    # arg 2 Set to a bundle ID (doesn't have to match your )
    # arg 3 Set to the app specific Apple ID username/email
    # arg 4 Set to the app specific Apple password  
    PWD=`pwd`

    if [ $DEMO == 1 ]; then
      ./$SCRIPTS/notarise.sh "${PWD}/build-mac" "${PWD}/build-mac/${ARCHIVE_NAME}.dmg" $NOTARIZE_BUNDLE_ID $APP_SPECIFIC_ID $APP_SPECIFIC_PWD
    else
      ./$SCRIPTS/notarise.sh "${PWD}/build-mac" "${PWD}/build-mac/${ARCHIVE_NAME}.dmg" $NOTARIZE_BUNDLE_ID_DEMO $APP_SPECIFIC_ID $APP_SPECIFIC_PWD
    fi

    if [ "${PIPESTATUS[0]}" -ne "0" ]; then
      echo "ERROR: notarize script failed, aborting"
      exit 1
    fi

  fi
fi

# Standalone DMG + VST3 zip (also runs after installer when PACKAGE_ZIP=1 / `full all`).
if [ $BUILD_INSTALLER == 0 ] || [ $PACKAGE_ZIP == 1 ]; then
  #---------------------------------------------------------------------------------------------------------
  # app dmg + vst3 zip, or a faster standalone-only dev dmg

  # Make the standalone self-contained before we copy it into the archive.
  # VoLumPaths on macOS checks the app bundle's Contents/Resources/VoLumRigs
  # first, so keep the built app populated there.
  if [ "$FAST_DEV" == "1" ]; then
    APP_DMG="build-mac/${ARCHIVE_NAME}.dmg"
  else
    APP_DMG="build-mac/${ARCHIVE_NAME}-app.dmg"
    VST3_ZIP="build-mac/${ARCHIVE_NAME}-vst3.zip"
  fi

  if [ ! -d "$APP" ]; then
    echo "ERROR: missing app bundle: $APP"
    exit 1
  fi

  if [ "$FAST_DEV" != "1" ] && [ ! -d "$VST3" ]; then
    echo "ERROR: missing VST3 bundle: $VST3"
    exit 1
  fi

  if [ "$FAST_DEV" == "1" ]; then
    rm -R -f build-mac/dmg "$APP_DMG"
    mkdir -p build-mac/dmg
  else
    rm -R -f build-mac/dmg build-mac/vst3-zip "$APP_DMG" "$VST3_ZIP"
    mkdir -p build-mac/dmg build-mac/vst3-zip
  fi

  if [ "$FAST_DEV" == "1" ]; then
    echo "building standalone dev dmg..."
  else
    echo "building standalone dmg..."
  fi
  echo ""
  cp -R "$APP" "build-mac/dmg/$PLUGIN_NAME.app"
  ln -s /Applications build-mac/dmg/Applications
  hdiutil create "$APP_DMG" -format "$DMG_FORMAT" -srcfolder build-mac/dmg -ov -anyowners -volname "$PLUGIN_NAME"
  rm -R build-mac/dmg

  if [ "$FAST_DEV" != "1" ]; then
    cp -R "$VST3" "build-mac/vst3-zip/$PLUGIN_NAME.vst3"

    # Portable VST3 packaging keeps rigs as a sibling folder so the plugin can
    # resolve them from the extracted archive or the user's VST3 directory.
    if [ -d "$RIGS_SRC" ]; then
      echo "bundling VoLumRigs..."
      mkdir -p build-mac/vst3-zip/VoLumRigs
      for amp_dir in "$RIGS_SRC"/*/; do
        [ -d "$amp_dir" ] || continue
        amp_name=$(basename "$amp_dir")
        mkdir -p "build-mac/vst3-zip/VoLumRigs/$amp_name"
        cp "$amp_dir"*.nam "build-mac/vst3-zip/VoLumRigs/$amp_name/" 2>/dev/null || true
      done
    else
      echo "WARNING: rigs directory not found: $RIGS_SRC"
    fi

    echo "zipping VST3 package..."
    echo ""
    ditto -c -k build-mac/vst3-zip "$VST3_ZIP"
    rm -R build-mac/vst3-zip
  fi
fi

#---------------------------------------------------------------------------------------------------------
# dSYMs
if [ "$PACKAGE_DSYMS" == "1" ]; then
  rm -R -f build-mac/*-dSYMs.zip

  echo "packaging dSYMs"
  echo ""
  zip -r ./build-mac/$ARCHIVE_NAME-dSYMs.zip ./build-mac/*.dSYM
fi

#---------------------------------------------------------------------------------------------------------

# prepare out folder for CI

echo "preparing output folder"
echo ""
mkdir -p ./build-mac/out
rm -f ./build-mac/out/${ARCHIVE_NAME}*.dmg ./build-mac/out/${ARCHIVE_NAME}*.zip
if [ "$PACKAGE_DSYMS" != "1" ]; then
  rm -f ./build-mac/out/${ARCHIVE_NAME}*-dSYMs.zip
fi
mv ./build-mac/*.dmg ./build-mac/out 2>/dev/null || true
mv ./build-mac/*.zip ./build-mac/out 2>/dev/null || true

#---------------------------------------------------------------------------------------------------------

#if [ $DEMO == 1 ]
#then
#  git checkout installer/VoLum.iss
#  git checkout installer/NeuralAmpModeler.pkgproj
#  git checkout resources/img/AboutBox.png
#fi

echo "done!"
echo ""
