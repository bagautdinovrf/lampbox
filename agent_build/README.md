# Сборка и проверки

Из корня репозитория:

```powershell
powershell -ExecutionPolicy Bypass -File .\agent_build\build.ps1
```

Скрипт выбирает последнюю установленную числовую версию Qt 6 в `C:\Qt` и её комплект MSVC x64, настраивает окружение Visual Studio через `vswhere` и `VsDevCmd`, затем запускает CMake, Ninja и CTest. По умолчанию собирается `Release`, тесты включены, параллелизм ограничен восемью процессами. Проект использует C++23; нужны Qt 6.12 или новее, Visual Studio 2022 или новее, инструменты MSVC x64 и Windows SDK. CMake 3.28 или новее и Ninja берутся из `C:\Qt\Tools`. Окружение вызывающей PowerShell-сессии восстанавливается после выполнения, включая ошибки.

```powershell
# Сборка Debug с удалением предыдущих результатов сборки
.\agent_build\build.ps1 -Configuration Debug -Clean -Jobs 4

# Только конфигурация CMake
.\agent_build\build.ps1 -ConfigureOnly

# Собрать, но не запускать тесты
.\agent_build\build.ps1 -SkipTests

# Собрать, проверить и установить приложение с библиотеками Qt
.\agent_build\build.ps1 -Deploy

# Собрать, проверить и создать установщик Inno Setup 6
.\agent_build\build.ps1 -Installer

# Явно указать компилятор Inno Setup, если он установлен в другом месте
.\agent_build\build.ps1 -Installer -InnoSetupCompiler 'D:\Tools\Inno Setup 6\ISCC.exe'

# Явно выбрать другую установленную Qt или передать параметры CMake
.\agent_build\build.ps1 -QtRoot C:\Qt -QtVersion 6.12.0 -QtKit msvc2022_64
.\agent_build\build.ps1 -CMakeArguments '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'
```

Обычный повторный запуск сохраняет кэш CMake и выполняет инкрементальную сборку. Скрипт сравнивает выбранную Qt, компилятор MSVC и генератор Ninja с `build/CMakeCache.txt`; при смене одного из них добавляет `--fresh`, чтобы обновить сохранённые пути к Qt и инструментам. Скрипт не откатывается на старую версию автоматически, если в самой новой отсутствует комплект MSVC x64; в этом случае установите комплект или укажите `-QtVersion`.

Результаты находятся внутри `agent_build`:

| Путь | Содержимое |
| --- | --- |
| `build/bin/MediaBoxManager.exe` | Редактор музыкальных и рекламных расписаний |
| `build/bin/MediaBoxPlayer.exe` | Фоновый аудиоплеер с управлением по TCP |
| `build/` | CMake, объектные файлы, исполняемые тесты и результаты CTest |
| `logs/<дата-время-id>/` | Отдельные журналы configure/build/test/deploy/installer и сводка `run.json` |
| `deploy/<конфигурация>/bin/` | Оба приложения с библиотеками и плагинами Qt после `-Deploy` |

Параметр `-Installer` включает `-Deploy` и после установки файлов запускает `ISCC.exe` для `Installer/installer.iss`. Нужен установленный Inno Setup 6: скрипт ищет компилятор в `PATH`, `Program Files (x86)` и `Program Files`; путь можно задать параметром `-InnoSetupCompiler`. Версия берётся из `project(MediaBoxManager VERSION ...)` корневого `CMakeLists.txt`. Готовый установщик сохраняется отдельно в `Installer/bin/MediaBoxManager-<версия>-Setup.exe`. `-ConfigureOnly` нельзя сочетать с `-Deploy` или `-Installer`.

Перед компиляцией установщика `-Installer` копирует DLL среды MSVC из самой новой версии `VC/Redist/MSVC/<версия>/x64/Microsoft.VC143.CRT` выбранной Visual Studio в `deploy/<конфигурация>/bin`. Наличие `vcruntime140.dll`, `vcruntime140_1.dll` и `msvcp140.dll` обязательно. Библиотеки устанавливаются рядом с приложением, поэтому отдельная установка системного `vc_redist.x64.exe` не требуется. Сам установщик запрашивает права администратора и устанавливает приложение для всех пользователей в `C:\Program Files\MediaBox`.

Настройки Manager всегда хранятся в `%ProgramData%\MediaBox\MediaBoxManager\MediaBoxManager.conf`, журналы — в том же каталоге. Путь не зависит от установки в Program Files или запуска EXE из другой папки. Установщик предоставляет обычным пользователям изменение каталога Manager и общих подкаталогов `timetable`, `media`, `cron`, `nncronlt`. В корне `%ProgramData%\MediaBox` разрешены чтение и создание файлов/каталогов без наследования этих прав на Player. Каталог Player установщик не создаёт: [скрипт службы](../MediaBoxPlayer/deploy/windows/README.md) создаёт `%ProgramData%\MediaBox\MediaBoxPlayer` с закрытым ACL.

Прежние настройки копируются при первом запуске только при отсутствии нового конфига: сначала `%ProgramData%\MediaBox\MediaBoxManager.conf`, затем файл из прежнего `QStandardPaths::AppConfigLocation`, после него `MediaBoxManager.conf` и `lampbox.conf` рядом с приложением. Исходный файл сохраняется. Хранение настроек рядом с EXE больше не выбирается автоматически.

Эмблема `Installer/assets/emblem.png` общая для установщика и приложения. Команда `python Installer/prepare-artwork.py` (нужен Pillow) экспортирует изображения мастера и `src/icons/app.ico`, используемый Windows EXE и Qt. После изменения значка для включения его в приложение требуется пересборка EXE; отдельная компиляция установщика использует существующий deploy.

Если папка `agent_build/deploy/Release` уже собрана и проверена и содержит указанные DLL среды MSVC в `bin`, установщик можно пересобрать отдельно, без повторной сборки приложения и запуска тестов:

```powershell
$projectVersion = [regex]::Match((Get-Content .\CMakeLists.txt -Raw),
    '(?im)^\s*project\s*\(\s*MediaBoxManager\s+VERSION\s+(\d+\.\d+\.\d+)\b').Groups[1].Value
$packageDirectory = (Resolve-Path .\agent_build\deploy\Release).Path
$outputDirectory = Join-Path (Get-Location).Path 'Installer\bin'
& 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' `
    "/DAppVersion=$projectVersion" "/DPackageDir=$packageDirectory" "/DOutputDir=$outputDirectory" `
    .\Installer\installer.iss
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }
```

В установщик входит весь готовый каталог deploy: MediaBoxManager, MediaBoxPlayer, библиотеки и плагины Qt. Подробнее об установщике — в [Installer/README.md](../Installer/README.md).

`-Clean` удаляет только проверенный каталог `agent_build/build`. Если в нём есть символьная ссылка или junction, удаление блокируется. Журналы и папка установки сохраняются. Ошибка любого этапа прерывает выполнение с ненулевым кодом; отсутствие зарегистрированных тестов также считается ошибкой. `-SkipTests` пропускает только запуск CTest, сохраняя сборку тестов. Для отключения самих тестовых целей передайте `-SkipTests -CMakeArguments '-DBUILD_TESTING=OFF'`.

Если CMake использует получение сторонних исходников через FetchContent, при первой конфигурации чистого клона понадобится доступ к сети. Для запуска `build/bin/MediaBoxManager.exe` и `build/bin/MediaBoxPlayer.exe` нужны DLL Qt в `PATH`; переносимый каталог приложений создаётся параметром `-Deploy`.

Оба приложения собираются по умолчанию. MediaBoxPlayer использует Qt Core, Network и Multimedia;
для MediaBoxManager также нужен Network. Manager управляет плеером через
[TCP API v1](../MediaBoxPlayer/TCP_API.md); настройка подключения описана в
[README](../README.md#подключение-mediaboxplayer). Исторический ключ
`HKLM\SOFTWARE\LampBox\Station` используется только для поиска данных редактора.
