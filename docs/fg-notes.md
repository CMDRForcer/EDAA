# DLSS Frame Generation in Elite (DX11) – technische Notizen

## Aufbau (addon/src/fg_present.cpp, fg.hpp, Hooks in passes.cpp)
1. **D3D12-Ausgabe:** Bei `reshade_present` (nach ReShade-Effekten und Overlay) wird das fertige
   D3D11-Bild in eine geteilte Textur kopiert (NT-Handle, D3D11 erzeugt, D3D12 öffnet; geteilter
   Fence wie im XeSS-Pfad). Ein D3D12-Flip-Swapchain auf einem Kindfenster über dem Spielfenster
   zeigt sie an. Die DXGI-Factory kommt aus **ReShades `dxgi.dll`** (`fg_factory=1`); mit der
   System-dxgi stürzte Present ab (ReShade-gewrappte Queue). ReShades zweite Effekt-Runtime auf
   dem Kindfenster wird mit `set_effects_state(false)` stillgelegt.
2. **Streamline 2.14.16:** `slInit` vor dem D3D12-Device; Device und Factory per
   `slUpgradeInterface` (eUseDXGIFactoryProxy, eUseFrameBasedResourceTagging); DLLs zur Laufzeit
   per GetProcAddress aus dem Add-on-Ordner. Reflex (eLowLatency + `frameLimitUs`), PCL-Marker,
   DLSS-G eOn, 1 generiertes Bild.
3. **Eingaben:** In `resolve()` (passes.cpp) werden Tiefe (R32F-Kopie) und EDAA-Bewegungsvektoren
   (RG16F, prev−cur UV, ohne Jitter, mvecScale 1) an `fg::capture` gegeben, in **zwei**
   abwechselnde geteilte Sätze kopiert und bei Present mit `eOnlyValidNow` auf der eigenen
   Command-List getaggt. Kameradaten: Reverse-Z unendlich, clipToPrevClip aus EDAA-Matrizen.
4. **HUD-Schutz** (`fg_hud_protect`): EDAAs HUD-Maske (Szene nach AA vs. Szene am Bloom) wird auch
   für FG gebaut; bei Present setzt `ps_fg_motion` die Vektoren auf HUD-Pixeln auf 0.
   Funktioniert voll nur mit `taa_before_hud=1`.
5. **Overlay:** Abschnitt „Frame Generation“ im EDAA-Tab (An/Aus live, HUD-Schutz,
   Frame-Limit live, Status echt→angezeigt, Reflex, HUD).

## Behobene Fehler (chronologisch)
| Symptom | Ursache | Fix |
|---|---|---|
| Absturz beim ersten D3D12-Present | System-dxgi + ReShade-gewrappte Queue | Factory aus ReShades dxgi |
| Starkes Ruckeln im Menü | DLSS-G jedes Frame an/aus (fehlende Eingaben) | Hysterese, letzte Eingaben weiterverwenden |
| Ruckler alle ~2 s im Flug | Zweite ReShade-Runtime setzte „keine Eingaben nötig“ | D3D12-Runtime im reshade_present ignorieren |
| Unruhiger Takt mit framepacer | Zwei Limiter (Reflex-Sleep + externer) | Reflex-Limiter (`fg_fps_limit`) |
| 70 statt 140 FPS | Reflex-Limit gilt mit FG für **angezeigte** Bilder | Limit 140 = 70 echte |
| GPU-Hänger/Absturz an Stationen | GPU-seitiges Warten D3D11↔D3D12 | nur noch begrenzte CPU-Waits |
| An/Aus-Ruckler nach dem Fix | Übersprungene Eingaben zählten als „keine Szene“ | getrennte Zähler, 8-ms-CPU-Wait |
| ~2,5 % übersprungene Bilder | Ein Eingabesatz, Freigabe erst nach Present | doppelte Puffer, Freigabe nach Command-List |
| Absturz in `gameoverlayrenderer64.dll+0x9c130` (Lesezugriff 0xFFFF…) | DLSS-G präsentiert vom eigenen Pacer-Thread, gleichzeitig Elite vom Render-Thread → Steam-Overlay-Hook nicht threadsicher | Presents serialisiert (timed_mutex, `present`/`finish_present`) – in Bestätigung |
| Bild und Eingaben eingefroren nach „Reset to defaults“ oder „Present through D3D11 again…“ bei laufender FG (Ereignis 1002) | `fg_present_test=0` stoppte die D3D12-Ausgabe sofort, das Kindfenster blieb mit dem letzten Bild über dem Spiel, DLSS-G blieb an | Ausgabe läuft bis Spielende weiter, DLSS-G geht über den erprobten Aus-Pfad aus; D3D11 erst beim nächsten Start – in Bestätigung |

Nicht geholfen: Umgebungsvariable `SteamNoOverlayUIDrawing=1` (Steam Input braucht das Overlay ohnehin).

## Log-Meldungen (ReShade.log)
- `EDAA FG: DLSS-G state at frame N: status 0x0, frames presented 2, frames without a scene X, reset R, focus F, busy skips B`
  – presented 2 = Zwischenbild aktiv; focus 0 = Spiel im Hintergrund (DLSS-G pausiert absichtlich);
  frames without a scene = Menü/Karte/Laden; busy skips sollte 0 sein.
- `EDAA FG: DLSS-G on/off` – sollte selten sein (Start, Menüs, Laden).
- `EDAA status: …` – jede Statusänderung (z. B. „waiting for two frames with a main camera“ = Kameraschnitt).
- `Frame rate over 100.00ms` (Streamline) – Hänger/Laden/Fokuswechsel; dauerhaft = GPU überlastet.
- `Wait on gpu fence timed out` (Streamline) – GPU hängt → Absturz folgt.
- sl.log: `Override Reported: ModelPreset, Value: 3` = FG-Preset C über NVIDIA App aktiv.

## Leistung (RTX 4070 Ti, 1440p)
- Supersampling 1.0: ~68–69 echte → ~137–140 angezeigte FPS.
- Supersampling 1.25: Stationen ~57 echte; 2.0 (5120×2880): ~3–6 echte – unbrauchbar.
- Checkerboard-Terrain verursacht flimmernde Linienmuster mit TAA/FG → aus.

## Grenzen
- DLSS-G-Presets lassen sich in Streamline 2.14.16 / DLSS-SDK 310.9.16 nicht per API setzen.
- Dynamic Multi Frame Generation nur auf RTX 50.
- Ein generisches FG für beliebige DX11-Spiele braucht pro Spiel Bewegungsvektoren.
