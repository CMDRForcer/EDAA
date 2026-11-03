# Projektplan – eigenes Grafik-Projekt auf EDAA-Basis: GEN·FRAME

Stand: 03.11.2026 – Planung abgeschlossen (mit Gerrit durchgegangen).

## Entscheidungen
- **Name:** GEN·FRAME (Anzeige/Overlay). Technisch: Repo `CMDRForcer/gen-frame`, Add-on
  `GenFrame.addon64`, ini-Sektion `[GenFrame]`, Log-Präfix `GenFrame:`. Klar getrennt vom
  bestehenden Repo `CMDRForcer/ED-Frame`.
- **Basis:** EDAA mit **allen** Funktionen (TAA, DLAA, XeSS, Bewegungsvektoren, Kamera-Decoding,
  HUD-Maske) – FG braucht Tiefe, Vektoren und Resolve-Punkt daraus. DLSS-G kommt darauf.
- **Lizenz:** EDAA steht unter MIT (Copyright (c) 2026 Siegfried Origin, seit 13.10.2026).
  Seine Zeile bleibt in `LICENSE`, eigene Copyright-Zeile für neue Teile dazu, Credit im README,
  `THIRD_PARTY_NOTICES.md` (NVIDIA, Intel) bleibt.
- **Upstream:** nicht auf Siegfrieds Antwort zum FG-Issue (Siegfried-Origin/EDAA#6) warten.
  GEN·FRAME ist eigenständig; Beiträge an Upstream optional, kein Plan-Punkt.
- **Elite Dangerous zuerst**, Architektur offen für weitere DX11-Spiele.
- **Privat:** erst nur für Gerrit, Veröffentlichung später offen.
- **Repo-Zeitpunkt:** Phase 1 (Stabilisierung) noch im öffentlichen Fork `CMDRForcer/EDAA`
  (Forks öffentlicher Repos können nicht privat sein; dafür unbegrenzte CI-Minuten).
  Danach legt Gerrit das private, leere `gen-frame` an, Umbenennung erst dort.
- **Erste GEN·FRAME-Version:** nur Umzug + Umbenennung (Stand `v0.5.3-cmdr1`, neue Namen,
  Übernahme alter `[EDAA]`-Werte, aufgeräumte Test-Schalter). Muss sich verhalten wie
  `v0.5.3-cmdr1`. Kern/Profil-Umbau kommt danach als eigene Version.
- **Einrichtung:** eigenes Programm (ersetzt den Inno-Ansatz), C# / .NET 10 / WPF, im Repo
  `gen-frame` unter `tool/`, gemeinsame Releases, CI baut es nur bei Änderungen in `tool/`.
- **Bild:** SDR mit kuratiertem Filter-Set in drei Varianten, **keine Fake-HDR-Filter**.
  Echtes HDR später (Phase 3).
- **Standardwerte** (Tool und neue Installationen; Nutzer kann sie im Overlay ändern):
  `aa_method=1` (DLAA), `dlss_preset=11` (Transformer-Modell K), `taa_before_hud=1` und
  `fg_hud_protect=1` (voller FG-HUD-Schutz). Neben dem Schalter „Resolve before the HUD pass“
  ein kurzer Erklärtext: an = FG-HUD-Schutz voll, HUD aber nicht geglättet (Treppenstufen an
  schrägen Linien); aus = HUD geglättet, FG-HUD-Schutz schwächer.

## Phase 1 – Stabil machen (im Fork)
- Steam-Overlay-Fix (serialisierte Presents, Commit 1a8f6a2) über mehrere Abende bestätigen,
  dann nach `cmdr/stable` und Release `v0.5.3-cmdr1`.
- Test-Schlüssel aufräumen: `fg_present_test`/`fg_factory` → ein Schalter; Diagnose begrenzen.
- Carrier-Sprung prüfen („EDAA status: …“-Zeilen im Log).
- Release-Notizen: bekannte Fallen (Supersampling, Checkerboard, RTSS).
- Zwei ReShade-Laufzeiten mit FG (D3D11 = `ReShade.ini`, D3D12-FG-Fenster = `ReShade2.ini`):
  nach „Neu laden“ im Overlay „Failed to create graphics pipeline“ für alle Techniken (03.11.).
  Vermutung: das sichtbare Overlay gehört zur D3D12-Laufzeit. Übergangslösung im Tool:
  `ReShade2.ini` mit leerem Preset `GenFrame_FGFenster.ini`. Dauerhaft im Add-on lösen
  (Overlay der D3D11-Laufzeit zeigen bzw. D3D12-Laufzeit ohne Effekte/Overlay).

## Phase 0 – Projekt aufsetzen (nach Phase 1)
1. Gerrit legt privates, leeres Repo `CMDRForcer/gen-frame` an (Weboberfläche).
2. Kompletten Stand mit Verlauf übertragen (inkl. `.github/workflows`, Submodule).
3. LICENSE ergänzen (eigene Copyright-Zeile zusätzlich), README neu, „basiert auf EDAA“.
4. Umbenennen: Overlay-Titel, Log-Präfix, ini-Sektion (mit Übernahme alter `[EDAA]`-Werte),
   Dateiname des Add-ons (darf nicht mit einem originalen EDAA kollidieren).
5. CI-Kontingent privater Repos beachten (Free: 2.000 Min./Monat, Windows zählt doppelt);
   Pfadfilter, damit Doku-Änderungen keinen Build auslösen.

## Phase 2 – Umbau: Kern + Spielprofil
| Kern (spielunabhängig) | Spielprofil Elite |
|---|---|
| D3D12-Ausgabe, Streamline, DLSS-G, Reflex, PCL | Kamera-Decoding (c270–c275) |
| Doppelte Eingabepuffer, Present-Serialisierung | Resolve-Punkt, Karten-/Screenshot-Erkennung |
| Overlay-Rahmen, Einstellungen, Diagnose | Bewegungsvektoren, Fahrzeugbewegung, Stencil |
| DLAA/XeSS-Anbindung | HUD-Maske/-Erkennung |
Schnittstelle: Profil liefert pro Frame Tiefe, Bewegungsvektoren, Kamera, HUD-Info, Resolve-Zeitpunkt.

## Einrichtungs-Programm (`tool/`, eigenes Teilprojekt nach der ersten Version)
Ziel: **Knopf drücken, konfigurieren, spielen** – alles über das Tool, ohne NVIDIA App,
Profile Inspector oder Handarbeit in ini-Dateien.

**Schnellwahl** (ein Klick, setzt alle nötigen Werte):
- DLAA + FG
- nur DLAA
- nur FG (Kantenglättung mit TAA) – prüfen, ob FG ohne DLAA sauber läuft
- aus (Spiel ohne Kantenglättung und FG; ReShade-Filter bleiben)

Der Nutzer entscheidet frei: Kantenglättung (DLAA/TAA/XeSS/aus), FG an/aus, Presets und
Filter-Variante lassen sich unabhängig kombinieren; die Schnellwahl ist nur eine Abkürzung.
Die letzte Wahl bleibt gespeichert.

Erste Tool-Version mit allen vier Funktionen:
1. **Installation + Wiederherstellung:** ReShade (Add-on-Version) von reshade.me laden,
   GEN·FRAME + Streamline-DLLs installieren, Backups (`ReShade.ini`, Preset, Grafikdateien inkl. `Custom.4.4.fxcfg`),
   Deinstallation/Zurücksetzen. EDHM/3DMigoto (`d3d11.dll`) und RenoDX nie anfassen.
2. **Filter-Set:** nur die benötigten Shader aus den **Original-Quellen** laden (nicht
   mitliefern), Preset-Varianten einspielen, `SkipLoadingDisabledEffects=1`.
3. **Elite-Check:** Supersampling ≠ 1.0, Checkerboard an, RTSS-/Doppel-Limiter, doppelte
   Schärfe (NVIDIA-Treiber/App), alte Test-Builds im Spielordner erkennen und korrigieren.
4. **Diagnose + Einstellungen:** Diagnose-Zip per Klick (ReShade.log, sl.log, Dumps,
   Einstellungen), GEN·FRAME-Einstellungen bei geschlossenem Spiel bearbeiten.
5. **Preset-Unlocker:** DLSS-Override für Elite im NVIDIA-Treiberprofil freischalten und
   Presets für DLAA (z. B. K) und FG (z. B. C oder neuestes) direkt im Tool wählen – ersetzt
   NVIDIA App/Profile Inspector. Bedingungen: nur das Elite-Profil, nur die DLSS-/FG-Preset-
   Einträge, vorher Backup, „Wiederherstellen“-Knopf, nur auf Knopfdruck; nach Treiber-Updates
   erkennen, ob die Werte weg sind, und „Erneut anwenden“ anbieten.
   Vor dem Bau recherchieren: genaue DRS-Einträge (NVAPI) und ob Administratorrechte nötig sind.
6. **Preset-Anzeige:** im Elite-Check aus `sl.log` zeigen, welche Presets wirklich aktiv sind
   (`Override Reported: ModelPreset … Applied`) und ob es die neuesten sind (`Is Latest`).
   Stand 02.11.: DLAA K (neuestes), FG C (nicht neuestes).

Spielordner über Steam und Epic finden. Dateien nur bei geschlossenem Spiel ändern.

## Filter-Set (SDR) – Vorschlag, noch zu testen
Reihenfolge = Ausführungsreihenfolge. Startwerte sind Ausgangspunkte.

| # | Baustein | Shader | Startwert | Zweck |
|---|---|---|---|---|
| 0 | GEN·FRAME | DLAA + FG | – | Kantenglättung, flüssiges Bild |
| 1 | Deband | `Deband.fx` (ReShade-Standard) | schwach | Banding in All/Nebeln/Korona; Unschärfe prüfen |
| 2 | Farbe | `PD80_02_Cinetools_LUT` (prod80) | 30–50 % | Stimmung, EDHM-HUD nicht verfälschen |
| 3 | Klarheit | `LocalContrastCS` | 0.30–0.40 | Tiefe in Cockpit, Station, Planeten |
| 4 | Vibrance | `Vibrance.fx` | 0.10–0.15 | kräftigere Farben |
| 5 | Schärfe | `CAS.fx` (AMD) | 0.30–0.50 | Detail nach DLAA, immer zuletzt |

Varianten:
- **Natürlich:** 1 + 5
- **Lebendig:** 1–5 (Standard)
- **Leistung:** nur 5

Nicht im Set: ReShade-AO (Elite hat eigene; teuer, wirkt aufs HUD), zusätzlicher Bloom
(HUD-Glühen, vermutlich FG-Schlieren am HUD-Rand), Fake-HDR, Film Grain, Tiefenunschärfe,
Motion Blur, Lens Dirt.

Offen:
- Vergleichstest jeder Variante gegen Gerrits Preset `ReshadePreset_ED_SLim.ini`
  (gleiche Orte: Station, Planet, Nebel, Supercruise; Screenshots + FPS, mit FG und HUD).
- Lizenz jeder Shader-Quelle prüfen (Download durch das Tool erlaubt?) – vor dem Tool-Bau.
- Optional später: `GraphicsConfigurationOverride.xml`-Feintuning (Sterne, Nebel, Schatten).

## Phase 3 – Funktionen
- Eigener Auto-Modus (FG aus, wenn GPU das Limit allein schafft; Reflex-GPU-Zeiten, Hysterese,
  `DLSSGFlags::eRetainResourcesWhenOff`). Dynamic MFG gibt es nur auf RTX 50.
- HUD-Kantenglättung nur auf HUD-Pixeln (Treppenstufen an schrägen HUD-Linien bei taa_before_hud=1).
- FG-Preset-Auswahl im Overlay, sobald Streamline/DLSS-SDK es anbieten (Stand 2.14.16/310.9.16: nein).
  Bis dahin übernimmt das Tool die Preset-Wahl über das Treiberprofil (Preset-Unlocker).
- **HDR im FG-Pfad:** geteilte Textur, D3D12-Swapchain, Farbraum und DLSS-G auf HDR umstellen,
  damit RenoDX (echtes HDR) mit FG läuft; Tool richtet RenoDX dann optional mit ein.
  Lohnt sich erst mit HDR-tauglichem Monitor (OLED/Mini-LED).

## Phase 4 – Zweites Spiel (Forschung)
- DLSS-G braucht Bewegungsvektoren; ältere DX11-Spiele liefern keine. Optionen prüfen:
  Kamera pro Spiel ermitteln (wie bei Elite), nur Tiefe, oder nur Optical Flow (Qualität?).
- Kandidaten-Spiele mit Gerrit festlegen.
- Tool um Spielauswahl erweitern.
