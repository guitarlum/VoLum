[Setup]
AppName=VoLum
AppContact=
AppCopyright=Copyright (C) 2026 Lum
AppPublisher=Lum
AppPublisherURL=https://github.com/guitarlum/VoLum
AppSupportURL=https://github.com/guitarlum/VoLum
AppVersion=0.7.15
VersionInfoVersion=0.7.15
DefaultDirName={autopf}\VoLum
DefaultGroupName=VoLum
Compression=lzma2
SolidCompression=yes
OutputDir=.\..\build-win\installer
ArchitecturesInstallIn64BitMode=x64
OutputBaseFilename=VoLum-Setup
SetupLogging=yes
ShowComponentSizes=no

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Messages]
WelcomeLabel1=Welcome to VoLum - NAM Player
SetupWindowTitle=VoLum Installer
SelectDirLabel3=VoLum will be installed in the following folder.
SelectDirBrowseLabel=To continue, click Next.

[Components]
Name: "app"; Description: "Standalone application (.exe)"; Types: full custom;
Name: "vst3_64"; Description: "64-bit VST3 Plugin (.vst3)"; Types: full custom; Check: Is64BitInstallMode;

[Dirs]
Name: "{cf64}\VST3\VoLum.vst3\"; Attribs: readonly; Check: Is64BitInstallMode; Components:vst3_64;

[Files]
; Standalone exe
Source: "..\build-win\app\x64\Release\VoLum.exe"; DestDir: "{app}"; Check: Is64BitInstallMode; Components:app; Flags: ignoreversion;

; VST3 plugin
Source: "..\build-win\VoLum.vst3\*.*"; Excludes: "\Contents\x86\*,*.pdb,*.exp,*.lib,*.ilk,*.ico,*.ini"; DestDir: "{cf64}\VST3\VoLum.vst3\"; Check: Is64BitInstallMode; Components:vst3_64; Flags: ignoreversion recursesubdirs;

; All 14 amp rig folders
Source: "..\..\rigs\Ampete One\*.nam"; DestDir: "{app}\rigs\Ampete One"; Flags: ignoreversion
Source: "..\..\rigs\Bad Cat mini Cat\*.nam"; DestDir: "{app}\rigs\Bad Cat mini Cat"; Flags: ignoreversion
Source: "..\..\rigs\Brunetti XL 2\*.nam"; DestDir: "{app}\rigs\Brunetti XL 2"; Flags: ignoreversion
Source: "..\..\rigs\Fryette Deliverance 120\*.nam"; DestDir: "{app}\rigs\Fryette Deliverance 120"; Flags: ignoreversion
Source: "..\..\rigs\H&K TriAmp Mk2\*.nam"; DestDir: "{app}\rigs\H&K TriAmp Mk2"; Flags: ignoreversion
Source: "..\..\rigs\Lichtlaerm Prometheus\*.nam"; DestDir: "{app}\rigs\Lichtlaerm Prometheus"; Flags: ignoreversion
Source: "..\..\rigs\Marshall 2204 1982\*.nam"; DestDir: "{app}\rigs\Marshall 2204 1982"; Flags: ignoreversion
Source: "..\..\rigs\Marshall JMP 2203 1976\*.nam"; DestDir: "{app}\rigs\Marshall JMP 2203 1976"; Flags: ignoreversion
Source: "..\..\rigs\Marshall JVM 210H OD1\*.nam"; DestDir: "{app}\rigs\Marshall JVM 210H OD1"; Flags: ignoreversion
Source: "..\..\rigs\Orange OD120 1975\*.nam"; DestDir: "{app}\rigs\Orange OD120 1975"; Flags: ignoreversion
Source: "..\..\rigs\Orange ORS100 1972\*.nam"; DestDir: "{app}\rigs\Orange ORS100 1972"; Flags: ignoreversion
Source: "..\..\rigs\Sebago Texas Flood\*.nam"; DestDir: "{app}\rigs\Sebago Texas Flood"; Flags: ignoreversion
Source: "..\..\rigs\Soldano SLO100\*.nam"; DestDir: "{app}\rigs\Soldano SLO100"; Flags: ignoreversion
Source: "..\..\rigs\THC Sunset\*.nam"; DestDir: "{app}\rigs\THC Sunset"; Flags: ignoreversion

; Docs
Source: "changelog.txt"; DestDir: "{app}"

[Registry]
; VST3 needs this to find the rigs folder
Root: HKLM; Subkey: "Software\VoLum\NeuralAmpModeler"; ValueType: string; ValueName: "AmpeteRigsPath"; ValueData: "{app}\rigs\Ampete One"; Flags: uninsdeletekey

[Icons]
Name: "{group}\VoLum"; Filename: "{app}\VoLum.exe"
Name: "{group}\Changelog"; Filename: "{app}\changelog.txt"
Name: "{group}\Uninstall VoLum"; Filename: "{app}\unins000.exe"

[Code]
var
  OkToCopyLog : Boolean;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssDone then
    OkToCopyLog := True;
end;

procedure DeinitializeSetup();
begin
  if OkToCopyLog then
    FileCopy (ExpandConstant ('{log}'), ExpandConstant ('{app}\InstallationLogFile.log'), FALSE);
  RestartReplace (ExpandConstant ('{log}'), '');
end;

[UninstallDelete]
Type: files; Name: "{app}\InstallationLogFile.log"
Type: filesandordirs; Name: "{app}\rigs"
