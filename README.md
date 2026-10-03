# MediaBoxManager

MediaBoxManager — приложение для управления музыкальными и рекламными расписаниями
LampPlayer. В подпроекте `MediaBoxPlayer` создаётся отдельный сервис проигрывания
музыки. Сейчас это каркас на Qt Core; воспроизведение и регистрация системного
сервиса ещё не реализованы. Оба приложения собираются через CMake на Qt 6 и C++23.

Автор: Руслан Багаутдинов. Почта: [bagautdinovrf@ya.ru](mailto:bagautdinovrf@ya.ru).

## Сборка в Windows

Текущая установленная версия Qt: **6.12.0**, комплект
`C:\Qt\6.12.0\msvc2022_64`. Для него нужен Visual Studio 2022 с компонентами
«Разработка классических приложений на C++» и Windows SDK. Сборка требует
CMake 3.28 или новее и Ninja; инструменты из `C:\Qt\Tools` подходят.

Из корня репозитория выполните в PowerShell:

```powershell
.\agent_build\build.ps1
```

Скрипт выбирает последнюю установленную версию Qt с комплектом MSVC x64
в `C:\Qt`, настраивает окружение компилятора, собирает Release и запускает CTest.
Результаты находятся в `agent_build\build\bin\MediaBoxManager.exe` и
`agent_build\build\bin\MediaBoxPlayer.exe`, журналы — в `agent_build\logs`.

```powershell
# Сборка Debug и тесты
.\agent_build\build.ps1 -Configuration Debug

# Сборка, тесты и готовая папка с Qt DLL и плагинами
.\agent_build\build.ps1 -Deploy

# Сборка, тесты и установщик Inno Setup 6
.\agent_build\build.ps1 -Installer

# Явный выбор комплекта Qt
.\agent_build\build.ps1 -QtRoot C:\Qt -QtVersion 6.12.0 -QtKit msvc2022_64
```

Полный список параметров и дополнительные примеры находятся в
[agent_build/README.md](agent_build/README.md).

## CMake и Qt Creator

В Qt Creator откройте корневой `CMakeLists.txt` и выберите комплект Qt 6.12
MSVC 2022 x64. CMake автоматически обрабатывает `.ui`, Qt MOC, `.qrc` и
ресурс значка Windows. Стандарт C++23 обязателен; для установленного MSVC
CMake включает `/std:c++latest`.

Можно использовать CMake напрямую из **x64 Native Tools Command Prompt for VS 2022**
с CMake и Ninja в `PATH`:

```powershell
cmake -S . -B agent_build/build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build agent_build/build --parallel
ctest --test-dir agent_build/build --output-on-failure
cmake --install agent_build/build --prefix agent_build/deploy/Release
```

CTest настраивает путь к DLL Qt автоматически. Для ручного запуска исполняемых
файлов из папки сборки добавьте `C:\Qt\6.12.0\msvc2022_64\bin` в `PATH`.
Основной скрипт настраивает этот путь автоматически. Для генераторов с несколькими
конфигурациями добавьте `--config Release` к сборке и установке и `-C Release` к CTest.

Тесты проверяют редакторы расписания, кириллицу в именах каналов и аудиофайлов,
чтение и запись метаданных TagLib, ресурсы интерфейса, распределение рекламы
и команду версии. Текстовые отчёты QtTest сохраняются в `agent_build/build/tests`.

## Зависимости

Qt-компоненты MediaBoxManager: Core, Gui, Widgets, Xml; MediaBoxPlayer использует
Core. При `BUILD_TESTING=ON` также нужен Test.
Исходный код приложения и тесты используют C++23 без расширений компилятора.

TagLib **1.13.1** собирается статически через CMake FetchContent из
[официального архива](https://github.com/taglib/taglib/releases/tag/v1.13.1),
проверяемого по SHA-256. Эта версия сохраняет совместимость с используемым API
TagLib 1.x. Первый запуск конфигурации требует интернета; последующие используют
кэш в каталоге сборки. Старые локальные копии `src/taglib` для сборки не нужны.
Внешний zlib и C bindings TagLib отключены.

Для сборки без сети заранее распакуйте архив TagLib 1.13.1 и задайте
`-DLAMPBOX_TAGLIB_SOURCE_DIR=C:/deps/taglib-1.13.1` при конфигурации CMake.

## Запуск

MediaBoxManager запускается без проверки установки и предложения скачать LampPlayer.
Если конфигурация плеера отсутствует, расписания и медиаданные хранятся
в отдельном каталоге MediaBoxManager в локальных данных пользователя. При
обновлении используется существующий каталог `lampbox` с `timetable` или
`mediabox.conf`, если новый каталог ещё не создан. Новые настройки и данные
имеют приоритет; данные расписаний не копируются и не перемещаются.

При установке в Program Files настройки хранятся в общем файле
`%PROGRAMDATA%\MediaBox\MediaBoxManager.conf`. Установщик создаёт каталог
`%PROGRAMDATA%\MediaBox` с правом изменения для обычных пользователей;
при удалении приложения каталог и настройки сохраняются. Первый запуск
один раз копирует прежние настройки, сохраняя исходный файл и не перезаписывая
существующий общий конфиг. Прежний `MediaBoxManager.conf` из
`QStandardPaths::AppConfigLocation` имеет приоритет перед конфигурациями рядом
с приложением: сначала `MediaBoxManager.conf`, затем `lampbox.conf`.
Вне Program Files сохраняется переносимый режим с конфигурацией рядом
с приложением: `MediaBoxManager.conf`, а при его отсутствии — существующий
`lampbox.conf`. Журнал BoxLog находится в `QStandardPaths::AppLocalDataLocation`.

Для воспроизведения нужен отдельно установленный LampPlayer и его данные.
В Windows каталог его конфигурации задаётся значением `Path` в ключе
`HKLM\SOFTWARE\LampBox\Station`; также поддерживается `C:\myplayer\mediabox.conf`.
Для плеера в другом каталоге укажите путь к этому каталогу в новом ключе `Path`.
Сборка и автоматические тесты не требуют установки LampPlayer. Команда
`MediaBoxManager.exe version` выводит версию приложения и сохраняет файл `version`
в текущей папке.

Ключ `HKLM\SOFTWARE\LampBox\Station` сохранён для совместимости с существующим
LampPlayer. Подпроект MediaBoxPlayer пока не заменяет его в редакторе расписаний.

`cmake --install` создает папку `bin` с обоими приложениями и необходимыми библиотеками Qt.
Установщик создаётся через Inno Setup 6 командой `agent_build/build.ps1 -Installer`.
Скрипт включает развёртывание файлов и запускает `Installer/installer.iss`; готовый
файл находится в `Installer/bin/MediaBoxManager-<версия>-Setup.exe`. Версия берётся
из корневого `CMakeLists.txt`. Компилятор `ISCC.exe` должен быть в `PATH` или
стандартном каталоге Inno Setup 6; другой путь можно передать через
`-InnoSetupCompiler`. Установщик использует `agent_build/deploy/<конфигурация>`,
включает оба приложения и предлагает запустить MediaBoxManager на последней странице.
Установка выполняется для всех пользователей в `C:\Program Files\MediaBox`
с запросом прав администратора. DLL среды MSVC устанавливаются рядом с приложениями;
дополнительный запуск системного `vc_redist.x64.exe` не требуется.
Для пересборки установщика из уже проверенной папки deploy есть отдельная команда
в [agent_build/README.md](agent_build/README.md); параметры и поведение установки
описаны в [Installer/README.md](Installer/README.md).

Единая эмблема лампочки используется установщиком и приложением.
`python Installer/prepare-artwork.py` экспортирует изображения мастера и значок
`src/icons/app.ico` для Windows EXE и Qt из `Installer/assets/emblem.png`
(для экспорта нужен Pillow).
