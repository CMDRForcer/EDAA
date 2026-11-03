# CLAUDE.md – Projektkontext für Claude Code

## Wer und wie
- Besitzer: Gerrit (CMDR Forcer, GitHub `CMDRForcer`). **Antworte immer auf Deutsch.**
- Arbeitsweise: Schritt-für-Schritt mit Bestätigung, kopierbares Markdown, direkt und präzise.
- Prompts/Texte immer vollständig liefern, nie nur „Ergänzungen“.
- Releases und Tags legt **Gerrit selbst** in der GitHub-Weboberfläche an (die Claude-Sitzungen
  durften bisher keine Tags/Releases/Repos anlegen). Liefere ihm Tag, Target-Branch, Titel und
  Release-Text zum Kopieren.

## Was das ist
Fork von **EDAA** (Siegfried-Origin/EDAA, MIT-Lizenz): ein ReShade-Add-on für Elite Dangerous
Odyssey (DX11) mit TAA, NVIDIA DLAA und Intel XeSS, eingesetzt *vor* dem Cockpit-HUD.
In diesem Fork kam **echtes DLSS Frame Generation (DLSS-G 2x)** dazu, über eine eigene
D3D12-Bildausgabe und NVIDIA Streamline. Ziel: ein **eigenes Projekt** daraus machen
(**GEN·FRAME**, privates Repo `gen-frame` nach Phase 1), zuerst für Elite, später erweiterbar,
mit eigenem Einrichtungs-Programm und Filter-Set – Entscheidungen in `docs/plan.md`.

Technische Details, Fehlerhistorie und Log-Meldungen: `docs/fg-notes.md`.
Originale EDAA-Doku: `docs/addon.md`, `docs/motion-vectors.md`, `docs/reverse-engineering.md`.

## Branches
- `feature/fg-phase2` – Entwicklung (aktuellster Stand).
- `cmdr/stable` – Gerrits feste Version, aus ihr werden Releases gebaut (Pre-Releases `vX.Y.Z-cmdr1`).
- `builds/<branch>` – von der CI veröffentlichte Builds (nur Binärdateien, nie als Target wählen).
- `master` – unverändertes Upstream-EDAA.
- Upstream: https://github.com/Siegfried-Origin/EDAA (FG-Vorschlag: Issue #6, Stand 03.11. ohne
  Antwort von Siegfried; GEN·FRAME wartet nicht darauf).

## Build
- Lokal nur unter Windows (VS 2022, CMake, fxc). In Cloud-Sitzungen **nicht lokal bauen** –
  per Push bauen lassen:
  - `.github/workflows/build_branch.yml` baut jeden Push auf `feature/**` und `cmdr/**`, lädt das
    Streamline-SDK v2.14.16 und veröffentlicht `EDAA.addon64` + `streamline/*.dll` +
    `source_commit.txt` in den Branch `builds/<branch>`.
  - Fertigen Build holen:
    `gh api -H "Accept: application/vnd.github.raw" "repos/CMDRForcer/EDAA/contents/EDAA.addon64?ref=builds/feature/fg-phase2"`
    und mit `source_commit.txt` prüfen, ob der Build zum eigenen Commit passt.
  - `release_on_tag.yml` baut bei neuem Tag Installer (Inno Setup) + Zip und hängt sie an den Release.
- CMake-Schalter: `EDAA_WITH_STREAMLINE` (Header aus `third_party/streamline`, DLLs zur Laufzeit
  aus dem Add-on-Ordner, kein Import-Lib), `EDAA_WITH_DLSS`, `EDAA_WITH_XESS`, `EDAA_DEVELOPER`.
- Commits mit Co-Authored-By-Zeile; Commit-Messages englisch, kurz und sachlich.

## Gerrits System (zum Testen)
- RTX 4070 Ti (12 GB), i7-14700KF, 32 GB, Samsung Odyssey G5 1440p/144 Hz G-Sync, HAGS an.
- NVIDIA-Treiber 617.42, NVIDIA App mit DLSS-Override: **Super Resolution Preset K,
  Frame Generation Preset C** (im sl.log bestätigt: „ModelPreset, Value: 3 … Applied: Yes“).
- Spielordner: `E:\Steam\steamapps\common\Elite Dangerous\Products\elite-dangerous-odyssey-64`
  (ReShade 6.8 `dxgi.dll`, EDHM/3DMigoto `d3d11.dll`, RenoDX-Add-on, EDAA.addon64, Streamline-DLLs).
- Journal: `C:\Users\Gerri\Saved Games\Frontier Developments\Elite Dangerous`.
- Grafik-Optionen: `%LOCALAPPDATA%\Frontier Developments\Elite Dangerous\Options\Graphics`
  (`Settings.xml`: TerrainCheckerboardRenderingEnabled=false, Supersampling 1.0).
- Controller: Manba One im PC-Modus **über Steam Input → Steam-Overlay muss an bleiben**.
- Läuft außerdem: RivaTuner (RTSSHooks64), NVIDIA-Overlay.
- Wichtige `ReShade.ini`-Werte `[EDAA]` (Stand 03.11.): `aa_method=1` (DLAA), `dlss_preset=10`,
  `taa_before_hud=0`, `fg_present_test=1`, `fg_generate=1`, `fg_factory=1`,
  `fg_fps_limit=140`, `fg_hud_protect=1`. Früher dokumentiert waren `dlss_preset=11` (K) und
  `taa_before_hud=1`; Grund der Änderung unbekannt – beide Varianten werden verglichen
  (Phase 1). Mit `taa_before_hud=0` wirkt der FG-HUD-Schutz nicht voll.
- Aktives ReShade-Preset: `ReshadePreset_ED_SLim.ini` (LSAO, Cinetools LUT, LocalContrastCS,
  CAS 1.0, Vibrance, PPFX Bloom, sdr_trc_fix); RenoDX-Add-on deaktiviert;
  `SkipLoadingDisabledEffects=0` bei ~200 installierten Shadern.
- Tests: Gerrit spielt, Claude liest danach `ReShade.log`, `sl.log`, Journal und – bei Abstürzen –
  den Elite-MiniDump (`*_MiniDump.dmp`, Gerrit lädt ihn hoch; Analyse mit Python `minidump`).
  Dateien im Spielordner nur austauschen, wenn Elite **geschlossen** ist (Journal endet mit
  `"event":"Shutdown"`); neue Builds sonst als `EDAA.addon64.<name>` daneben legen.
  Vor Änderungen an `ReShade.ini`/Grafikdateien Backup anlegen.

## Leitplanken
- Die Grafikkarte darf nie auf die andere API warten (keine GPU-seitigen D3D11↔D3D12-Waits).
- Zwei Bildausgaben im Prozess: Presents nie gleichzeitig (Steam-Overlay-Hook ist nicht threadsicher).
- DLSS-G nicht unnötig aus-/einschalten (Neuaufbau = Ruckler).
- Nichts in fremde Treiberprofile/Registry schreiben (z. B. kein NVAPI-DRS für FG-Presets).
- Ehrlich bleiben: Vermutungen als Vermutungen kennzeichnen, Ergebnisse mit Log belegen.
