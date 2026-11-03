# Projektplan – eigenes Grafik-Projekt auf EDAA-Basis (Arbeitstitel GEN·FRAME)

Stand: 03.11.2026

## Entscheidungen
- Eigenes Projekt, **EDAA als Basis** (MIT; Copyright-Hinweis von Siegfried Origin bleibt, Credit im README).
- **Elite Dangerous zuerst**, Architektur so, dass später weitere DX11-Spiele dazukommen können.
- **Erst mal nur für Gerrit** (privates Repository), Veröffentlichung später offen.
- Planung ist noch nicht abgeschlossen – offene Punkte unten mit Gerrit klären, bevor gebaut wird.

## Offene Entscheidungen
- [ ] Endgültiger Name (Vorschlag GEN·FRAME, Repo `CMDRForcer/gen-frame`)
- [ ] Neues privates Repo anlegen (Gerrit, Weboberfläche, leer) oder vorerst im Fork weiterarbeiten
- [ ] Umgang mit Upstream: Siegfrieds Antwort auf das FG-Issue abwarten?
- [ ] Umfang der ersten eigenen Version
- [ ] Einrichtungs-App: erweiterter Inno-Installer (a) oder eigenes Programm (b)

## Phase 0 – Projekt aufsetzen
1. Gerrit legt privates, leeres Repo an.
2. Kompletten Stand mit Verlauf übertragen (inkl. `.github/workflows`, Submodule).
3. LICENSE ergänzen (eigene Copyright-Zeile zusätzlich), README neu, „basiert auf EDAA“.
4. Umbenennen: Overlay-Titel, Log-Präfix, ini-Sektion (mit Übernahme alter `[EDAA]`-Werte),
   Dateiname des Add-ons (darf nicht mit einem originalen EDAA kollidieren).
5. CI-Kontingent privater Repos beachten (Windows-Minuten zählen doppelt).

## Phase 1 – Stabil machen
- Steam-Overlay-Fix (serialisierte Presents, Commit 1a8f6a2) über mehrere Abende bestätigen,
  dann nach `cmdr/stable` und Release `v0.5.3-cmdr1`.
- Test-Schlüssel aufräumen: `fg_present_test`/`fg_factory` → ein Schalter; Diagnose begrenzen.
- Carrier-Sprung prüfen („EDAA status: …“-Zeilen im Log).
- Release-Notizen: bekannte Fallen (Supersampling, Checkerboard, RTSS).

## Phase 2 – Umbau: Kern + Spielprofil
| Kern (spielunabhängig) | Spielprofil Elite |
|---|---|
| D3D12-Ausgabe, Streamline, DLSS-G, Reflex, PCL | Kamera-Decoding (c270–c275) |
| Doppelte Eingabepuffer, Present-Serialisierung | Resolve-Punkt, Karten-/Screenshot-Erkennung |
| Overlay-Rahmen, Einstellungen, Diagnose | Bewegungsvektoren, Fahrzeugbewegung, Stencil |
| DLAA/XeSS-Anbindung | HUD-Maske/-Erkennung |
Schnittstelle: Profil liefert pro Frame Tiefe, Bewegungsvektoren, Kamera, HUD-Info, Resolve-Zeitpunkt.

## Phase 3 – Funktionen
- Eigener Auto-Modus (FG aus, wenn GPU das Limit allein schafft; Reflex-GPU-Zeiten, Hysterese,
  `DLSSGFlags::eRetainResourcesWhenOff`). Dynamic MFG gibt es nur auf RTX 50.
- HUD-Kantenglättung nur auf HUD-Pixeln (Treppenstufen an schrägen HUD-Linien bei taa_before_hud=1).
- Diagnose-Paket per Klick (ReShade.log, sl.log, Dumps, Einstellungen → Zip).
- FG-Preset-Auswahl im Overlay, sobald Streamline/DLSS-SDK es anbieten (Stand 2.14.16/310.9.16: nein).
- Einrichtungs-App (siehe offene Entscheidung).

## Phase 4 – Zweites Spiel (Forschung)
- DLSS-G braucht Bewegungsvektoren; ältere DX11-Spiele liefern keine. Optionen prüfen:
  Kamera pro Spiel ermitteln (wie bei Elite), nur Tiefe, oder nur Optical Flow (Qualität?).
- Kandidaten-Spiele mit Gerrit festlegen.
