#
# Windows-1251
#

# Included files
!include Sections.nsh
!include MUI2.nsh
!include Library.nsh
!include LogicLib.nsh

Name "LampBox"
!addplugindir ".\plugins"
SetCompressor /SOLID lzma
# General Symbol Definitions
!define REGKEY "SOFTWARE\$(^Name)"
#set in command line "ex: /DVERSION=1.0.6.666"!define VERSION "1.0.4"
!define COMPANY "OOO Lampmedia"
!define URL www.lampmedia.ru
#!define CONTENTDIR "C:\Users\All Users\Lampmedia\"

!define PRODUCT_NAME "LampBox"
!define COMPANY_NAME "Lampmedia"
!define PACKAGE_DIR "package"

# MUI Symbol Definitions
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_STARTMENUPAGE_REGISTRY_ROOT HKLM
!define MUI_STARTMENUPAGE_NODISABLE
!define MUI_STARTMENUPAGE_REGISTRY_KEY ${REGKEY}
#!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME StartMenuGroup
!define MUI_STARTMENUPAGE_DEFAULTFOLDER Lampmedia
!define MUI_FINISHPAGE_RUN "$PROGRAMFILES\${COMPANY_NAME}\${PRODUCT_NAME}\lampbox.exe"

!define MUI_UNFINISHPAGE_NOAUTOCLOSE

ShowInstDetails nevershow
ShowUninstDetails nevershow

BrandingText 'ООО "Лампмедиа"'

# Variables
#Var StartMenuGroup
Var LibInstall

# Installer pages
!insertmacro MUI_PAGE_LICENSE eula.rtf
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

#Устанавливаем язык инсталлятора
!insertmacro MUI_LANGUAGE "Russian"

CRCCheck on
XPStyle on
ShowInstDetails show
VIProductVersion 1.0.4.0
VIAddVersionKey ProductName "Lampmedia LampBox"
VIAddVersionKey ProductVersion "${VERSION}"
VIAddVersionKey CompanyName "${COMPANY}"
VIAddVersionKey CompanyWebsite "${URL}"
VIAddVersionKey FileVersion "${VERSION}"
VIAddVersionKey FileDescription "Установщик локального менеджера Лампмедиа"
VIAddVersionKey LegalCopyright ""
InstallDirRegKey HKLM "${REGKEY}" Path
ShowUninstDetails show

InstallDir "$PROGRAMFILES\${COMPANY_NAME}\${PRODUCT_NAME}"

!define /file LAMPBOX_VERSION "${PACKAGE_DIR}\version"

OutFile LampBox_trial-${LAMPBOX_VERSION}.exe
!ifndef TRIAL
OutFile LampBox-${LAMPBOX_VERSION}.exe
!define TRIAL "false"
!endif


Section "Установка"
    # Для всех пользователей
    SetShellVarContext all
    # Директория установки
    SetOutPath $INSTDIR
    # Файлы
    File /r "${PACKAGE_DIR}\*"
    
    WriteRegStr HKLM "${REGKEY}" Path $INSTDIR

    # Подготовка деинсталлятора
    WriteUninstaller $INSTDIR\uninstall.exe

    #####################################
    # Создание ярлыков программы
    #####################################
    !define PROGRAM_MENU "$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}\"

    createDirectory "$SMPROGRAMS\${COMPANY_NAME}\"
    createDirectory "$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}\"
    # Ярлык программы в программах
    CreateShortCut "$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_NAME}.exe"
    # Ярлык программы на рабочем столе
    CreateShortCut "$Desktop\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_NAME}.exe"
    # Ярлык деинсталлятора в программах
    CreateShortCut "$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}\Удалить ${PRODUCT_NAME}.lnk" "$INSTDIR\uninstall.exe"

    # Создание контент директорий
    !define CONTENTDIR "$APPDATA\${COMPANY_NAME}"
    CreateDirectory "${CONTENTDIR}\"
    CreateDirectory "${CONTENTDIR}\music\"
    CreateDirectory "${CONTENTDIR}\ads\"

    # Запись значений в реестр
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" DisplayName "${PRODUCT_NAME}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" DisplayVersion "${LAMPBOX_VERSION}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" Publisher "${COMPANY}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" URLInfoAbout "${URL}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" DisplayIcon "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" UninstallString "$INSTDIR\uninstall.exe"
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" NoModify 1
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" NoRepair 1
    
    SetRegView 64
    WriteRegStr HKLM "SOFTWARE\Lampmedia\Station" Trial "${TRIAL}"
    SetRegView 32
    WriteRegStr HKLM "SOFTWARE\Lampmedia\Station" Trial "${TRIAL}"
SectionEnd

Section Uninstall
    # Для всех пользователей
    SetShellVarContext all

    RMDir /r "$INSTDIR"
    RMDir /r "${PROGRAM_MENU}"
    Delete "$Desktop\${PRODUCT_NAME}.lnk"

    DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
    #Delete "$SMPROGRAMS\$StartMenuGroup\Uninstall ${PRODUCT_NAME}.lnk"

    #DeleteRegValue HKLM "${REGKEY}" StartMenuGroup
    DeleteRegValue HKLM "${REGKEY}" Path
    DeleteRegKey /IfEmpty HKLM "${REGKEY}\Components"
    DeleteRegKey /IfEmpty HKLM "${REGKEY}"

    #RMDir $SMPROGRAMS\$StartMenuGroup
SectionEnd

# Installer functions
Function .onInit
    InitPluginsDir
    Push $R1
    File /oname=$PLUGINSDIR\spltmp.bmp lamp_logo.bmp
    advsplash::show 1000 600 400 -1 $PLUGINSDIR\spltmp
    Pop $R1
    Pop $R1
    Push $0
    ReadRegStr $0 HKLM "${REGKEY}" Path
    ClearErrors
    StrCmp $0 "" +2
    StrCpy $LibInstall 1
    Pop $0
FunctionEnd

# Uninstaller functions
Function un.onInit
    ReadRegStr $INSTDIR HKLM "${REGKEY}" Path
    #!insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuGroup
FunctionEnd

