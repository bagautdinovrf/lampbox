# MediaBoxPlayer

Подпроект будущего сервиса проигрывания музыки. Сейчас он содержит консольный процесс на Qt Core: обработку `--help`, `--version` и цикл событий для дальнейшей реализации сервиса. Проигрывание, управление очередью и регистрация системной службы пока не реализованы. Обычный запуск оставляет процесс работающим до его завершения пользователем; в терминале используйте `Ctrl+C`.

Требования совпадают с MediaBoxManager: CMake 3.28 или новее, Qt 6.12 или новее и компилятор с поддержкой C++23. На Windows используйте совместимый комплект Qt/MSVC и окружение Visual Studio для сборки x64. Нужен только модуль Qt Core.

Подпроект собирается вместе с MediaBoxManager через `add_subdirectory(MediaBoxPlayer)` в корневом CMake. Исполняемый файл называется `MediaBoxPlayer`; его версия — `0.1.0`.

Для самостоятельной сборки из корня репозитория выполните следующие команды в PowerShell с настроенным окружением MSVC. Замените путь к Qt на установленный у вас; CMake и Ninja должны быть доступны в `PATH`.

```powershell
cmake -S .\MediaBoxPlayer -B .\build\MediaBoxPlayer -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.12.0\msvc2022_64"
cmake --build .\build\MediaBoxPlayer
ctest --test-dir .\build\MediaBoxPlayer --output-on-failure
```

При включённом `BUILD_TESTING` CTest проверяет команду `--version`. Для отключения этой проверки передайте `-DBUILD_TESTING=OFF` при конфигурации CMake.

Для запуска непосредственно из каталога сборки добавьте библиотеки Qt в `PATH`:

```powershell
$env:PATH = "C:\Qt\6.12.0\msvc2022_64\bin;$env:PATH"
.\build\MediaBoxPlayer\bin\MediaBoxPlayer.exe --help
.\build\MediaBoxPlayer\bin\MediaBoxPlayer.exe --version
.\build\MediaBoxPlayer\bin\MediaBoxPlayer.exe
```

Установка создаёт отдельный каталог с исполняемым файлом и развёрнутыми зависимостями Qt на поддерживаемых Qt платформах. На Windows установленный процесс можно запускать без добавления Qt в `PATH`:

```powershell
cmake --install .\build\MediaBoxPlayer --prefix .\build\MediaBoxPlayer-install
.\build\MediaBoxPlayer-install\bin\MediaBoxPlayer.exe --version
```

При сборке из корня оба приложения устанавливаются общей командой `cmake --install` для корневого каталога сборки. Для генератора с несколькими конфигурациями укажите `--config Release` в командах сборки и установки, а также `-C Release` в команде CTest. На Linux и macOS команды аналогичны; исполняемый файл не имеет расширения `.exe`. На macOS Qt не развёртывает библиотеки автоматически для этого консольного процесса, поэтому для запуска установленного MediaBoxPlayer требуется доступная среда Qt.
