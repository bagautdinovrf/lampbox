# Установщик MediaBoxManager

Установщик собирается **Inno Setup 6** из готового Qt deploy-каталога. Основной сценарий сборки описан в [agent_build/README.md](../agent_build/README.md).

```powershell
& 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' `
    '/DAppVersion=1.1.1' `
    '/DPackageDir=D:\GitHub\lampbox\agent_build\deploy\Release' `
    '/DOutputDir=D:\GitHub\lampbox\Installer\bin' `
    'D:\GitHub\lampbox\Installer\installer.iss'
```

Параметры необязательны: значения по умолчанию — версия `1.1.1`, пакет `../agent_build/deploy/Release`, выход `Installer/bin`. Относительные пути разрешаются от каталога `installer.iss`. Результат — `MediaBoxManager-<версия>-Setup.exe`.

В пакете обязательны `bin/MediaBoxManager.exe`, `bin/MediaBoxPlayer.exe` и локальные библиотеки MSVC: `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`. Дерево deploy устанавливается без изменения структуры: `bin`, `plugins`, `translations`, Qt DLL, `qt.conf` и остальные локальные библиотеки среды MSVC. Файлы настроек приложения, журналы, отладочные символы, старый `lampbox.exe` и `vc_redist*.exe` исключены из пакета.

Установка выполняется с правами администратора в `%ProgramFiles%\MediaBox` (64-разрядный каталог Program Files), без страницы выбора каталога. Прежний каталог установки не используется. Общие ярлыки в меню «Пуск» и на рабочем столе создаются всегда, без страницы выбора ярлыков. В названия входит версия: например, `MediaBoxManager 1.1.1`. Прежние ярлыки без версии заменяются. На завершающей странице есть отмеченная галочка **«Запустить MediaBoxManager»**; приложение запускается от имени пользователя, открывшего установщик. В тихом режиме программа не запускается.

Для приложения в Program Files используется общий файл настроек `%PROGRAMDATA%\MediaBox\MediaBoxManager.conf`. Установщик создаёт каталог `%PROGRAMDATA%\MediaBox` с правом изменения для обычных пользователей. При первом запуске прежние настройки копируются один раз, исходники сохраняются, существующий общий конфиг не перезаписывается. Прежний `MediaBoxManager.conf` из `QStandardPaths::AppConfigLocation` имеет приоритет перед `MediaBoxManager.conf` и `lampbox.conf` рядом с приложением. Вне Program Files остаётся переносимый режим с прежним выбором локальной конфигурации. Журналы остаются в `QStandardPaths::AppLocalDataLocation`.

Деинсталлятор удаляет только зарегистрированные при установке файлы и ярлыки. Каталог `%PROGRAMDATA%\MediaBox`, настройки, журналы и другие созданные пользователем файлы сохраняются; каталоги и реестр прежнего LampBox/LampPlayer не изменяются. MediaBoxPlayer входит в пакет как отдельное приложение; регистрация службы появится после реализации сервиса.

Установочная эмблема и изображения мастера лежат в `assets`. Знак лампочки также используется в значке приложения `src/icons/app.ico` для Windows EXE и Qt. Экспорт из `assets/emblem.png` выполняется командой `python Installer/prepare-artwork.py` из корня репозитория (нужен Pillow). Лицензия подключается из `eula.rtf`.

Описание заключительной галочки: [Inno Setup: секция Run](https://jrsoftware.org/ishelp/topic_runsection.htm). Права установки: [PrivilegesRequired](https://jrsoftware.org/ishelp/topic_setup_privilegesrequired.htm).
