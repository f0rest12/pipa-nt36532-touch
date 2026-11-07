# nt36532-touch

Скелет KMDF-драйвера тача **NovaTek NT36532** для Windows 11 ARM64 на Xiaomi Pad 6 (pipa).
Архітектура: KMDF-драйвер біндиться на `ACPI\PIPA3653`, відкриває **SPB** (контролер SPI4/SE4 через
`qcspi8250` + SpbCx), читає пакети тача по перериванню (TLMM GPIO39) і віддає HID-звіти через
**VHF** (Virtual HID Framework). Збирається у хмарі через GitHub Actions — ПК не потрібен.

## Що вже є
- Повний каркас драйвера, що **має зібратись** і завантажитись: PnP/power, відкриття SPB-таргета,
  переривання ISR/DPC, VHF з дескриптором тач-дигітайзера (поки 1 контакт).
- CI (`.github/workflows/build.yml`): ставить WDK, крос-білд під ARM64, артефакт `nt36532.sys`.

## Чого ще нема (TODO, познач у коді)
- Протокол NT36xxx: reset, завантаження прошивки (`nt36532_tianma.bin`/`_csot.bin`), парсинг
  event-буфера → координати. Брати з Linux `drivers/input/touchscreen/nt36xxx` (SPI-гілка).
- Розширення HID до 10 пальців + перо (`novatek,pen-support`).

## Як зібрати (без ПК, з телефона)
1. Створи порожній **приватний** репозиторій на GitHub (з телефона, у браузері).
2. Залий вміст цієї теки в репозиторій (через веб-інтерфейс «Add file → Upload files», або з Fedora:
   `git init && git add . && git commit -m init && git branch -M main && git remote add origin <url> && git push -u origin main`).
3. Вкладка **Actions** → workflow `build-nt36532` запуститься на push (або тисни «Run workflow»).
4. Коли зелений — відкрий run → **Artifacts** → скачай `nt36532-arm64.zip` (у браузері планшета
   потрапить у `~/Downloads`).

## Як поставити на планшет
```bash
# на Fedora
cd ~/Downloads && unzip nt36532-arm64.zip -d ~/pipa/drv-touch
cd ~/pipa
# підписати нашим тестовим ланцюжком (як інші драйвери)
for c in drv-touch/*.cat; do osslsigncode sign -certs chain.pem -key sign.key -h sha256 -in "$c" -out "$c.new" && mv "$c.new" "$c" && echo "OK $c"; done
./pipa-agent.sh drivers ~/pipa/drv-touch
```
Драйвер стане в DriverStore; прив'яжеться, коли в ACPI з'явиться вузол `ACPI\PIPA3653`
(вузол SPI4 + тач додамо в acpitabl окремим кроком, хендофф §10.4).

## Нюанси CI (перший run може впасти — норма, ітеруємо)
- Крок **Install WDK** — найкрихкіший: назва choco-пакета чи шлях до `WDK.vsix` можуть відрізнятись
  між версіями. У логах runner-а буде реальне дерево `C:\Program Files (x86)\Windows Kits\10` —
  за ним підправимо крок. Запасний, найнадійніший варіант — **EWDK** (самодостатній тулчейн):
  скачати EWDK-zip у workflow, `LaunchBuildEnv.cmd`, далі `msbuild`. Скажи — перепишу під EWDK.
- KMDF-версію (`1.33`) за потреби знизимо до тієї, що є в встановленому WDK.
- `SignMode=Off` — у CI не підписуємо; підпис робимо вже на Fedora нашим ланцюжком.

## Файли
```
.github/workflows/build.yml   CI: WDK + крос-білд ARM64 + артефакт
driver/driver.c               DriverEntry, PnP/power, ISR/DPC
driver/spb.c                  відкриття SpbCx-таргета + write/read транзакція
driver/hid.c                  VHF: старт + подача HID-звітів
driver/hidreport.h            HID report descriptor (тач-дигітайзер)
driver/device.h               контекст і декларації
driver/nt36532.inf            ACPI\PIPA3653, KMDF, Std.NTARM64
driver/nt36532.vcxproj        проєкт драйвера (ARM64, KMDF)
driver/nt36532.rc             версія
```
