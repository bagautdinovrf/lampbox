# MediaBoxPlayer на Android

APK содержит только `MediaBoxService`: нет `Activity`, окна и значка запуска.
`QtService` загружает библиотеку MediaBoxPlayer и вызывает C++ `main()` с
`QAndroidService`. Очередь, аудиодвижок и TCP API те же, что на остальных платформах.
MediaBoxManager в этой реализации не изменяется.

## Сборка

Нужны Android-комплект Qt 6.12 с Multimedia и заголовками CorePrivate,
соответствующий desktop Qt для host tools, Android SDK/NDK и JDK.
Для Qt 6.12 официально указаны JDK 21 и NDK 27.2.12479018; приложение задаёт
минимальный API 28 и целевой API 36. Пример для установленного комплекта arm64:

```sh
/opt/Qt/6.12.0/android_arm64_v8a/bin/qt-cmake \
  -S MediaBoxPlayer -B build/player-android -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DQT_HOST_PATH=/opt/Qt/6.12.0/gcc_64 \
  -DANDROID_SDK_ROOT=/opt/android-sdk \
  -DANDROID_NDK_ROOT=/opt/android-sdk/ndk/27.2.12479018
cmake --build build/player-android --target apk
```

Пути зависят от установки SDK и Qt. Qt создаёт Gradle-проект через
`androiddeployqt`, используя этот каталог как `QT_ANDROID_PACKAGE_SOURCE_DIR`.
Manifest не включает Activity и не требует собственного Gradle-шаблона.

## Запуск и проверка debug APK

После установки созданного debug APK:

```sh
adb shell am start-foreground-service \
  -n org.mediabox.player/org.mediabox.player.MediaBoxService
adb forward tcp:17655 tcp:17655
adb shell run-as org.mediabox.player cat files/mediabox/mediaboxplayer/control.token
adb logcat -s MediaBoxPlayer Qt qt
```

Токен из последней команды чтения используется в запросах API v1 на
`127.0.0.1:17655`; формат описан в основной документации плеера. `run-as` работает
только для отладочного APK. Токен не выводится плеером в журнал и не передаётся
через Intent. Каталог настроек по умолчанию — `mediabox/mediaboxplayer` внутри закрытого
`Context.getFilesDir()` приложения. Общие файлы могут храниться в `files/mediabox`
того же приложения; Android не открывает этот каталог другим APK, включая
MediaBoxManager. Прежний `files/control.token` копируется при первом запуске,
если нового токена ещё нет; исходный файл сохраняется.
Аудиофайлы также должны быть доступны процессу плеера: используйте его каталог
файлов. Сетевые URL и `content://` в API этой версии не принимаются. Доступ ко всему хранилищу
и разрешения чтения медиатеки не запрашиваются.

Остановка процесса:

```sh
adb shell am stopservice \
  -n org.mediabox.player/org.mediabox.player.MediaBoxService
adb forward --remove tcp:17655
```

При первичной настройке устройства службу можно запустить явным Intent из
видимого приложения настройки, например:

```java
Intent service = new Intent().setComponent(new ComponentName(
        "org.mediabox.player", "org.mediabox.player.MediaBoxService"));
context.startForegroundService(service);
```

Это механизм начального запуска процесса Android. Для управления уже запущенным
плеером MediaBoxManager использует только TCP API с токеном, включая остановку
воспроизведения, настройки и получение состояния. Сервис экспортирован, поэтому другие приложения также могут запускать
и останавливать его. Intent не принимает аргументы плеера или команды управления.
Для управляемой установки можно ограничить жизненный цикл signature permission
и подписывать Manager и Player одним ключом; обычный запуск через `adb am` после
такого ограничения тоже будет недоступен. Передача токена Manager требует
отдельного доверенного provisioning: приложение Manager не может самостоятельно
прочитать закрытый каталог release APK плеера. Эта интеграция здесь не реализована.

## Жизненный цикл

- Android требует foreground service типа `mediaPlayback` и системное уведомление.
  Уведомление создаётся до загрузки Qt и не содержит элементов управления плеером.
  Отдельного интерфейса приложения нет. На Android 13+ без предоставленного
  разрешения на уведомления сервис по-прежнему отображается в системном списке
  активных приложений; это ограничение ОС, а не окно плеера.
- `PARTIAL_WAKE_LOCK` удерживается при запрошенном воспроизведении, включая загрузку
  и переход между треками, и снимается при паузе, остановке, исчерпании очереди
  после ошибок или уничтожении сервиса. В ожидании команд плеер
  не удерживает процессор от сна; доступность TCP во время Doze не гарантируется.
- На Android 12+ произвольный запуск foreground service из фонового приложения
  запрещён. Первичный запуск плеера при настройке устройства должен происходить
  из разрешённого пользовательского сценария, например из видимого приложения
  настройки. Запуск при загрузке устройства отсутствует:
  Android 15+ запрещает запуск `mediaPlayback` foreground service из `BOOT_COMPLETED`.
- Используется `START_NOT_STICKY`: ОС не перезапускает сервис автоматически после
  уничтожения процесса. При явной остановке освобождается wake lock, затем Qt
  завершает нативный процесс и аудиобэкенд. Очередь в этом MVP хранится в памяти.
  При новом запуске нужна новая загрузка очереди через Manager.

Сборка APK, воспроизведение с выключенным экраном и остановка сервиса требуют
проверки на Android-устройстве/эмуляторе. В текущей среде Android SDK, JDK и
Android-комплекта Qt нет; эти проверки не выполнены.

Основания реализации:
[Qt 6.12 Android Services](https://doc.qt.io/qt-6/android-services.html),
[поддерживаемые Android-конфигурации Qt](https://doc.qt.io/qt-6/android.html),
[Android media playback foreground services](https://developer.android.com/develop/background-work/services/fgs/service-types#media),
[ограничения фонового запуска](https://developer.android.com/develop/background-work/services/fgs/restrictions-bg-start).
