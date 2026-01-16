[Setup]
AppName=WinSoftphone
AppVersion=0.1.0
DefaultDirName={pf}\WinSoftphone
DefaultGroupName=WinSoftphone
OutputBaseFilename=WinSoftphoneSetup
OutputDir={#SourcePath}\dist
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourcePath}\build\Release\winsoftphone.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourcePath}\config\default_config.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "{#SourcePath}\config\default_contacts.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "{#SourcePath}\config\default_history.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "{#SourcePath}\pjsip\runtime\*.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\WinSoftphone"; Filename: "{app}\winsoftphone.exe"

[Run]
Filename: "{app}\winsoftphone.exe"; Description: "Launch WinSoftphone"; Flags: nowait postinstall skipifsilent
