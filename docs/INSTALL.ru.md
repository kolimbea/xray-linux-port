# Установка и лаунчер (нативная Linux-версия S.T.A.L.K.E.R. GAMMA)

Альфа-версия. Нужна уже установленная Anomaly + GAMMA (через MO2 под Wine/Proton или
stalker-gamma-cli). Нативная версия только читает эту установку и никогда её не меняет.

## Требования
- 64-битный Linux с glibc 2.41 или новее: Debian 13, Ubuntu 25.04, Fedora 42, Arch/Manjaro, CachyOS и
  новее. Проверить: `ldd --version`.
- Видеокарта и драйвер с Vulkan 1.3 и `VK_EXT_shader_object`: NVIDIA (проприетарный драйвер),
  AMD/Intel — Mesa (RADV/ANV). Проверить: `vulkaninfo --summary`.
- Остальные библиотеки уже внутри пакета.

## Способ 1. AppImage (любой дистрибутив)
1. Скачайте `xray-native-<версия>-x86_64.AppImage` со страницы
   [Releases](https://github.com/kolimbea/xray-linux-port/releases).
2. Сделайте файл исполняемым и запустите:
   ```
   chmod +x xray-native-*-x86_64.AppImage
   ./xray-native-*-x86_64.AppImage
   ```
   (или в файловом менеджере: Свойства → «Разрешить запуск как программу», затем двойной щелчок).

## Способ 2. Архив .tar.gz
```
tar xf xray-native-<версия>-linux-x86_64.tar.gz
./xray-native/bin/xray-launcher
```

## Способ 3. Debian / Ubuntu (.deb)
```
sudo apt install ./xray-native-<версия>-amd64.deb
```
Лаунчер появится в меню приложений («S.T.A.L.K.E.R. GAMMA (native)») и как команда `xray-launcher`.

## Способ 4. Arch / Manjaro
Пакет `xray-native-bin` готовится к публикации в AUR. Пока можно собрать его из репозитория:
```
git clone https://github.com/kolimbea/xray-linux-port && cd xray-linux-port/tools/packaging/aur
makepkg -si
```
(или просто используйте AppImage).

## Способ 5. Flatpak
```
flatpak install --user xray-native-<версия>.flatpak
flatpak run io.github.kolimbea.XrayNative
```
Flatpak видит домашнюю папку, `/mnt`, `/media` и `/run/media` только для чтения — установка GAMMA
должна лежать там. Данные лаунчера хранятся в `~/.var/app/io.github.kolimbea.XrayNative/`.

## Первый запуск лаунчера
1. Откройте вкладку **Настройки** и укажите папку GAMMA — ту, в которой лежат `anomaly/` и `gamma/`
   (например `~/Games/gamma`). Лаунчер проверит, что там есть профиль GAMMA
   (`gamma/profiles/G.A.M.M.A/modlist.txt`).
2. По желанию нажмите **«Добавить в меню приложений»** — появится ярлык.
3. Нажмите **«Играть»**. При первом запуске лаунчер собирает папку игры из ваших модов так же, как
   это делает MO2 (по порядку `modlist.txt`): это может занять минуту. Сохранения и настройки
   копируются один раз и дальше живут отдельно от установки GAMMA.

Изменили список модов в MO2 — лаунчер заметит это и пересоберёт папку перед следующим запуском.

## Обновления
Лаунчер сам проверяет новые версии на GitHub. Вкладка **Обновления**: список изменений, кнопка
установки (загрузка, проверка контрольной суммы, распаковка) и откат на предыдущую версию — она
хранится рядом.

## Без окна (из терминала)
Всё, что делает окно, доступно и из командной строки; параметры выполняются по порядку:

```
xray-launcher --gamma ~/Games/gamma --install --play
```

`--gamma <папка>` (если GAMMA и Anomaly лежат отдельно — повторить со второй папкой), `--releases`,
`--install [версия]` (по умолчанию — новейшая), `--versions`, `--use <версия>`, `--overlay`, `--play`,
`--help`.

## Где что лежит
| Что | Где |
|---|---|
| Версии игры, папка игры, настройки лаунчера | `~/.local/share/xray-native/` |
| Сохранения и настройки игры | `~/.local/share/xray-native/game/appdata/` |
| Логи и отчёты о вылетах | `~/.local/share/xray-native/game/appdata/logs/` (кнопка «Открыть папку логов») |

## Если что-то пошло не так
- Игра закрылась с ошибкой — лаунчер покажет последний отчёт о вылете (`crash_*.log`). Приложите его
  и `xray_<имя>.log` из папки логов к сообщению об ошибке.
- Чёрный экран или ошибка Vulkan при запуске — проверьте `vulkaninfo --summary`: драйвер должен
  поддерживать Vulkan 1.3.
- Ошибки: [Issues](https://github.com/kolimbea/xray-linux-port/issues).

## Известные ограничения альфы
- Тесселяция пока отключена в Vulkan-рендере.
- С MSAA эффекты SSFX (TAA, motion vectors, SSS) отключаются — рекомендуется MSAA выкл.
- ReShade (Windows) не работает; вместо него можно использовать vkBasalt (`ENABLE_VKBASALT=1`).
