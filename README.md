# LampBox

LampBox — приложение для управления музыкальными и рекламными расписаниями LampPlayer.
Проект собирается через CMake на Qt 6 и C++23.

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
Результат находится в `agent_build\build\bin\lampbox.exe`, журналы —
в `agent_build\logs`.

```powershell
# Сборка Debug и тесты
.\agent_build\build.ps1 -Configuration Debug

# Сборка, тесты и готовая папка с Qt DLL и плагинами
.\agent_build\build.ps1 -Deploy

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

Qt-компоненты: Core, Gui, Widgets, Xml; при `BUILD_TESTING=ON` также Test.
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

LampBox запускается без проверки установки и предложения скачать LampPlayer.
Если конфигурация плеера отсутствует, расписания и медиаданные хранятся
в отдельном каталоге LampBox в локальных данных пользователя.
Для воспроизведения нужен отдельно установленный LampPlayer и его данные.
В Windows каталог его конфигурации задаётся значением `Path` в ключе
`HKLM\SOFTWARE\LampBox\Station`; также поддерживается `C:\myplayer\mediabox.conf`.
Для плеера в другом каталоге укажите путь к этому каталогу в новом ключе `Path`.
Сборка и автоматические тесты не требуют установки LampPlayer. Команда
`lampbox.exe version` выводит версию приложения и сохраняет файл `version`
в текущей папке.

`cmake --install` создает папку `bin` с приложением и необходимыми библиотеками Qt.
Исторический NSIS-скрипт в `Installer` сохранен; новый скрипт сборки создает
готовую папку приложения, а генерация NSIS-установщика в него не включена.
