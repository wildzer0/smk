# Keyboard HUD sul firmware SMK

L'overlay (`tools/hud/`, scritto per vial-qmk) parla con la tastiera con il comando `0xF0` sull'interfaccia raw HID di VIA/Vial. Su SMK
è implementato in [`src/smk/hud.c`](../src/smk/hud.c), adattato da `tools/hud/hud.c`: risponde soltanto, non manda nulla di sua
iniziativa, non scrive in flash. Si attiva con `#define HUD_ENABLE` in `kbdef.h` (B68: attivo) e richiede la build con raw HID.

## Cosa risponde

| Comando | Contenuto | Su SMK |
| --- | --- | --- |
| `0x01` GET_INFO | protocollo, righe, colonne, layer, capacità, keycode di HUD_TOG, versione della keymap | invariato; in più `byte[14]` = byte per riga (protocollo 2) |
| `0x02` GET_STATE | maschera layer, layer di default, tasti premuti, mods, mods deboli, one-shot, flag, contatore HUD_TOG, versione keymap | invariato, vedi differenze |
| `0x03` GET_KEYMAP | keycode effettivi, fino a 12 per richiesta | invariato |
| `0x04` GET_MATRIX | **nuovo**: tutta la matrice dei tasti premuti | solo protocollo 2 |

Il resto del formato è identico a quello di `tools/hud/hud.c`: pacchetto da 32 byte `[0xF0, comando, stato, sequenza, ...]`, stato `0` ok,
`1` comando sconosciuto, `2` argomento non valido; l'eco di comando e sequenza è invariata.

## Perché il protocollo 2

La B68 ha **5 righe × 16 colonne**, mentre il protocollo 1 prevede al massimo 8×8 (un byte per riga in `GET_STATE`). Per una tastiera più larga
di 8 colonne SMK annuncia **protocollo 2**:

* `GET_INFO` porta in `byte[14]` il numero di byte per riga (`ceil(colonne / 8)`: 2 sulla B68).
* `GET_STATE` riempie `byte[10..17]` con le **prime 8 colonne** di ogni riga: un lettore di protocollo 1 resta corretto per quelle colonne.
* `GET_MATRIX` (`0x04`) restituisce da `byte[4]` in poi `righe × byte_per_riga` byte, riga per riga, primo byte = colonne 0-7, bit 0 = colonna 0.
  Sulla B68: 10 byte.

**Cosa deve fare l'app:** se `GET_INFO` dice protocollo ≥ 2, chiedere anche `GET_MATRIX` e usare quella per i tasti premuti. Per le tastiere fino a 8×8
non cambia nulla (protocollo 1).

## Differenze rispetto al modulo QMK

| Funzione | QMK | SMK |
| --- | --- | --- |
| Caps Word, Layer Lock, rilevamento OS, Colemak | sì (se abilitati) | **no**: flag e capacità a zero |
| mods one-shot bloccati | sì | sempre 0 |
| versione della keymap | hash periodico della keymap | contatore incrementato a ogni modifica fatta da Vial (più preciso) |
| maschera layer | 32 bit | 8 bit (8 layer) |
| tastiera split | sì | non applicabile |

## Tasto HUD_TOG

Il keycode personalizzato `HUD Toggle` (`0x7E00`, il primo `QK_KB`) è elencato in `vial.json` (`customKeycodes`): in Vial compare tra i keycode personalizzati
e si assegna come un tasto qualsiasi. SMK non lo manda al PC: incrementa il contatore di `GET_STATE` (`byte[27]`), che l'overlay usa per mostrarsi o nascondersi.

## Dove sta il codice

`src/smk/hud.c` e `hud.h` (protocollo), `via.c` (inoltra i comandi `0xF0`), `layer.c` (`layer_get_state`, `layer_oneshot_*`), `keymap.c` (`keymap_version`),
`report.c` (`get_weak_mods`), `src/keyboards/yunzii-b68/kb.c` (tasto HUD_TOG). Test: `tests/host/test_hud.c` (protocollo 1 e 2) e `TestB68Hud` sul simulatore.
