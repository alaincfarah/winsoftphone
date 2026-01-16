[Setup]
AppName=WinSoftphone
AppVersion=0.1.0
DefaultDirName={pf}\WinSoftphone
DefaultGroupName=WinSoftphone
OutputBaseFilename=WinSoftphoneSetup
Compression=lzma2
SolidCompression=yes

[Files]
Source: "build\winsoftphone.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "config\default_config.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "config\default_contacts.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "config\default_history.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "pjsip\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\WinSoftphone"; Filename: "{app}\winsoftphone.exe"

[Run]
Filename: "{app}\winsoftphone.exe"; Description: "Launch WinSoftphone"; Flags: nowait postinstall skipifsilent
