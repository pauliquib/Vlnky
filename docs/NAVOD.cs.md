# Vlnky 1.5 — návod k sestavení, instalaci a použití

Tapeta pro KDE Plasma 6, která vykresluje pohyblivou vlnu z menu PSP (XMB) ze souboru
`system_plugin_bg.rco`. Umí i barvy XMB podle měsíce, vlnu ve stylu PS2 (PSX DESR),
vlnu Svec Studio a postprocesing pro Full HD, 1440p a 4K.

Licence: GPL-2.0-or-later (soubor `COPYING`). Autor: Pavel Švec,
<https://svec-elektro.cz/projekty/psp-xmb-wave/>.

---

## 1. Co archiv obsahuje a co ne

**Obsahuje:** zdrojový kód pluginu (C++/Qt), shadery, Plasma balíček (QML + konfigurace),
testy, náhledový nástroj, instalační a odinstalační skript, skript pro import originální vlny.

**Neobsahuje:** žádný firmware Sony, žádné soubory `system_plugin_bg.rco` ani data z nich
(sítě, textury, animace). Tapeta vykresluje soubory, které si dodáte sami. Originální vlnu si
můžete vytáhnout z oficiální aktualizace PSP (kapitola 5.2). Za legálnost souborů RCO z
komunitních sbírek odpovídá ten, kdo je používá.

---

## 2. Požadavky

- KDE Plasma 6 (Wayland i X11), Qt **6.7 nebo novější** (kvůli `QQuickRhiItem`).
- Grafika s OpenGL 3.3 nebo Vulkanem. Plasma ve výchozím stavu používá OpenGL.
- Pro sestavení: kompilátor C++17, CMake 3.21+, vývojové balíčky Qt.

### Fedora 42+ (vyzkoušeno na Fedoře 44)

```bash
sudo dnf install gcc-c++ cmake zlib-devel \
  qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtdeclarative-devel qt6-qtshadertools-devel \
  kf6-kpackage python3
```

`qt6-qtbase-private-devel` obsahuje hlavičky `rhi/qrhi.h`. Když chybí, sestavení použije
přibalenou kopii z `third_party/rhi` (z Qt 6.6) a CMake vypíše varování. Funguje to, ale
přesná shoda s nainstalovaným Qt je jistější.

### Ostatní distribuce (názvy balíčků, nevyzkoušeno)

| Distribuce | Balíčky |
|---|---|
| Arch / Manjaro | `base-devel cmake zlib qt6-base qt6-declarative qt6-shadertools python` |
| Debian 13 / Ubuntu 24.10+ / KDE neon | `build-essential cmake zlib1g-dev qt6-base-dev qt6-base-private-dev qt6-declarative-dev qt6-shadertools-dev python3` |
| openSUSE Tumbleweed | `gcc-c++ cmake zlib-devel qt6-base-devel qt6-gui-private-devel qt6-declarative-devel qt6-shadertools-devel python3` |

`kpackagetool6` (instalace Plasma balíčku) bývá součástí Plasmy.

---

## 3. Sestavení

```bash
tar xzf vlnky-1.5.0.tar.gz
cd vlnky-1.5.0
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Výsledek:

- `build/libvlnkywave.so` — QML plugin s rendererem,
- `build/plasma-wallpaper/package/` — hotový Plasma balíček,
- `build/vlnky-preview` — náhled vlny v okně,
- `build/vlnky-extract` — vytažení textury z RCO (pro kontrolu).

Volba `-DVLNKY_COPY_USER_QML_MODULE=OFF` zabrání tomu, aby build kopíroval QML modul do
`~/.local/lib64/qml` (hodí se pro balíčkování).

Volitelně testy nad celou sbírkou vln:

```bash
VLNKY_WAVE_COLLECTION="/cesta/ke/sbirce" cmake -B build
cmake --build build && ctest --test-dir build
```

---

## 4. Instalace (pro jednoho uživatele)

```bash
bash scripts/install.sh
systemctl --user restart plasma-plasmashell.service
```

Skript:

1. sestaví projekt, pokud ještě není sestavený,
2. nainstaluje Plasma balíček do `~/.local/share/plasma/wallpapers/org.psvec.vlnky/`,
3. zkopíruje QML modul do `~/.local/lib64/qml/org/psvec/vlnky/`,
4. když najde sbírku vln, zapíše ji jako výchozí a vytáhne náhledové textury.

Proměnné prostředí:

- `WAVE_COLLECTION=/cesta/ke/sbirce` — výchozí sbírka,
- `SKIP_TEXTURE_EXTRACT=1` — přeskočí vytahování náhledových PNG.

Aktualizace na novější verzi: rozbalte nový archiv a spusťte znovu `scripts/install.sh`
a restart Plasmy.

---

## 5. Kde vzít vlny

Sbírka je obyčejná složka, ve které má každá vlna vlastní podsložku se souborem
`system_plugin_bg.rco`:

```
Moje vlny/
├── PSP Original (OFW 6.61)/system_plugin_bg.rco
├── Moje modrá/system_plugin_bg.rco
└── …
```

### 5.1 Vlastní soubor RCO

Stačí ho vložit do podsložky sbírky, nebo v nastavení použít **Import custom RCO…**.

### 5.2 Originální vlna Sony z oficiální aktualizace

Stáhněte si oficiální aktualizaci PSP (`EBOOT.PBP`, doporučeno 6.61 — archivovaná je
např. na <https://archive.org/details/psp_ofw_firmwares>) a spusťte:

```bash
bash scripts/import-original-wave.sh /cesta/k/EBOOT.PBP "/cesta/ke/sbirce"
```

Skript při prvním spuštění stáhne a sestaví nástroj
[pspdecrypt](https://github.com/John-K/pspdecrypt) (GPL-3.0) do
`~/.cache/vlnky/tools`. Potřebuje k tomu `git`, `gcc-c++`, `zlib-devel`
a `openssl-devel`. Z aktualizace rozbalí `flash0:/vsh/resource/system_plugin_bg.rco` a uloží
ho do sbírky jako `PSP Original (OFW 6.61)`.

Originální vlna je bílá. Barvu, kterou znáte z PSP, jí dá volba **XMB colour** (kapitola 7).

### 5.3 Vlna ve stylu PS2

Nepotřebuje žádný soubor, stačí v nastavení zvolit **Wave style → PS2 – PSX (DESR) XMB wave**.

### 5.4 Vlna Svec Studio

Také nepotřebuje žádný soubor: zvolte **Wave style → Svec Studio wave**. Vykresluje čtyři
vrstvy „hero vln“ z landing page Sencurio (stejné vlny jako na ploše svec-studio): vyplněné
siluety dole na obrazovce, které se pomalu posouvají do stran a houpu se.

Přes **Wave colour → Custom colour** jí (a jemnému násvitu pozadí) dáte libovolnou barvu,
jako v nastavení vlny ve svec-studio. Pozadí samotné se řídí běžnými volbami *Background* —
např. vlastní jednobarevná barva nebo přechod.

---

## 6. Zapnutí tapety

1. Pravým tlačítkem na plochu → **Nastavit plochu a tapetu…**
2. **Typ tapety:** *Vlnky*.
3. **Wave collection:** složka se sbírkou (tlačítko *Browse…*).
4. **Wave:** vyberte vlnu. Náhled se ukáže pod výběrem.
5. **Použít / OK.**

---

## 7. Všechna nastavení

### Vlna a barvy

| Volba | Význam |
|---|---|
| **Wave collection** | Složka se sbírkou vln (v sekci *Advanced*; zobrazí se sama, když se žádné vlny nenajdou). |
| **Wave** | Vybraná vlna (podsložka sbírky). |
| **Wave style** | *PSP* — vlna ze souboru RCO; *PS2* — procedurální vlna PSX (DESR), nepotřebuje RCO; *Svec Studio* — vrstvené hero vlny ze svec-studio, nepotřebuje RCO. |
| **Wave colour** | *Automatic* — výchozí barva podle motivu/stylu; *Custom colour* — libovolná barva vlny pro jakýkoli styl. |
| **XMB colour** | *Off* — pozadí podle voleb níže; *Automatic* — barva aktuálního měsíce jako na PSP/PS3; nebo pevně jeden z 12 měsíců (leden stříbrná … prosinec červená). |
| **Dim at night** | S barvou XMB: plný jas 12–15 h, nejtmavší 22–6 h (o 50 %). |
| **Import…** (vedle výběru vlny) | Použije libovolný soubor `.rco` mimo sbírku. |
| **Background** | Když je *XMB colour* vypnutá: jednobarevné, přechod (začátek, konec, úhel), obrázek, nebo obrázek ze složky vlny (`screen3.bmp`, `preview.png`, `screenshot.png`). Barvy se vybírají tlačítkem s barvou (otevře výběr barvy); *Colour preset* nabízí hotové barevné kombinace a šipky prohodí začátek a konec přechodu. |
| **Opacity** | Síla vlny (0,3–1,0). |
| **Height** | Svislá poloha vlny na obrazovce (platí i pro styl PS2). |
| **Speed** | Rychlost animace vlny, 10–400 % (100 % = originál z PSP). Tlačítko vrátí výchozí rychlost. U každého posuvníku je tlačítko pro návrat na výchozí hodnotu. |

### Render quality

Předvolby nastaví několik voleb najednou:

| Předvolba | Mesh | Supersampling | MSAA | Glow | FPS |
|---|---|---|---|---|---|
| Performance | Medium | 1× | vyp. | vyp. | 30 |
| Balanced | High | 1× | 4× | 30 % | 60 |
| Beautiful (Full HD+) | Ultra | 1,5× | 4× | 35 % | 60 |
| Maximum (4K) | Ultra | 2× | 8× | 40 % | 60 |

| Volba | Význam |
|---|---|
| **Mesh detail** | Hustota tesselace plochy: Low 96×28, Medium 168×48, High 264×80, Ultra 400×120 vzorků. Počítá CPU. |
| **Supersampling** | Vlna se kreslí v 1,5× nebo 2× větším rozlišení a pak se zmenší. Nejvíc zatěžuje GPU (2× na 4K = buffer 7680×4320). |
| **Antialiasing** | MSAA 2×/4×/8× pro hladké hrany pramenů. |
| **Preset** | Rychlé nastavení kvality: Performance, Balanced, Beautiful, Maximum. *Custom* = vlastní kombinace (zobrazí pokročilé volby). |
| **Glow** | Měkká záře kolem jasných pramenů. |
| **Vignette** | Ztmavení rohů. |
| **Smooth (bicubic) reflection texture** | Hladké zvětšení textury 128×128; bez něj jsou na velkých monitorech vidět kosočtverce. |
| **Dithering** | Odstraní barevné pruhy v tmavých přechodech. |
| **Max FPS** | 15 / 30 / 60 / 120 / 144. Na monitoru se 120–144 Hz je animace plynulejší, ale víc zatěžuje CPU. |

Doporučení: Full HD → *Balanced* nebo *Beautiful*; 1440p → *Beautiful*;
4K s výkonnou grafikou → *Maximum*, na integrované grafice *Balanced*.

### Napájení a výkon

| Volba | Význam |
|---|---|
| **Pause animation on battery** | Na baterii se animace zastaví. |
| **Battery saver** | Max 15 FPS, nižší mesh, bez supersamplingu, MSAA a bloomu. |
| **Adaptive quality** | Když výpočet snímku trvá déle než 6 ms, automaticky sníží mesh. |

### Diagnostika

| Volba | Význam |
|---|---|
| **Show debug overlay** | Vlevo nahoře ukáže vlnu, snímek, FPS, tesselaci a čas CPU. Také `VLNKY_WAVE_DEBUG=1`. |
| **Wireframe mesh** | Zobrazí drátěný model plochy. |
| **Capture debug screenshots (F12)** | Uloží snímek do `~/.cache/vlnky/screenshots/`. |

---

## 8. Náhled mimo Plasmu

```bash
build/vlnky-preview -r "/cesta/ke/sbirce/Vlna/system_plugin_bg.rco" --size 1920x1080
build/vlnky-preview --size 1920x1080 --set waveStyle=1 --set backgroundMode=2 --set backgroundTop=#0c76c0
build/vlnky-preview -r vlna.rco --set renderScale=2 --set msaaSamples=8 --set bloom=0.4 -g snimek.png
```

`--set vlastnost=hodnota` nastaví libovolnou vlastnost položky `XmbWaveItem`
(`tessLevel`, `renderScale`, `msaaSamples`, `bloom`, `bicubicTexture`, `dither`, `vignette`,
`waveStyle`, `backgroundMode`, `backgroundTop`, `backgroundBottom`, `ps2WaveColor`,
`svecWaveColor`, `svecWaveCustom`, `waveTint`, `waveOpacity`, `waveCenterY`, `speed`).
`-g soubor.png` uloží snímek a skončí.

---

## 9. Odinstalace

```bash
bash scripts/uninstall.sh            # ponechá cache
bash scripts/uninstall.sh --purge    # smaže i cache, snímky a pomocné nástroje
systemctl --user restart plasma-plasmashell.service
```

Před odinstalací přepněte tapetu na jinou, jinak Plasma ukáže prázdnou plochu.

---

## 10. Řešení problémů

| Problém | Řešení |
|---|---|
| Místo vlny je jen pozadí | Zkontrolujte cestu k RCO v nastavení. Zapněte *Show debug overlay* a podívejte se do logu (níže). |
| Vlna vypadá po aktualizaci divně | `rm -rf ~/.cache/vlnky/` a restart Plasmy. |
| Seká se nebo zatěžuje CPU | Předvolba *Performance* nebo nižší *Mesh detail* a *Max FPS*. |
| Seká se GPU na 4K | Snižte *Supersampling* na 1× nebo 1,5×. |
| Plugin se nenačte | Byl po instalaci restartován plasmashell? Existuje `~/.local/share/plasma/wallpapers/org.psvec.vlnky/contents/ui/org/psvec/vlnky/libvlnkywave.so`? |
| V nastavení je jen „Colour“ | plasmashell drží ve paměti starý plugin. Spusť `scripts/install.sh` a `systemctl --user restart plasma-plasmashell.service`. |
| CMake varuje „rhi/qrhi.h not found“ | Doinstalujte `qt6-qtbase-private-devel` (Fedora) a sestavte znovu. |

Log:

```bash
QT_LOGGING_RULES="xmb.*=true" journalctl --user -f -t plasmashell
```

Kategorie: `xmb.rco`, `xmb.mesh`, `xmb.anim`, `xmb.render`, `xmb.perf`.

---

## 11. Licence

Program je svobodný software pod GNU GPL verze 2 nebo (dle vaší volby) jakékoli pozdější
(`COPYING`). Přehled cizích částí a atribucí je v souboru `NOTICE`. PlayStation, PSP, PS2,
PSX a XMB jsou ochranné známky Sony Interactive Entertainment; projekt se Sony nesouvisí.
