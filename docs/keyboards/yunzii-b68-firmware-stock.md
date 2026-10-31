# Yunzii B68 — analisi del firmware stock (M1.6, prima passata)

> Fonte: `backups/dump/r1.bin` (64 KiB, MD5 `4aea8bec…`). Le disassemblazioni **non** sono nel repo (firmware
> proprietario): qui ci sono solo fatti e indirizzi. Stato: **prima passata**, i punti marcati *TBD* sono aperti.

## Come riprodurre

Ghidra 12 (`~/ghidra`) in headless con JDK 21 (`nix build nixpkgs#jdk21`; Ghidra rifiuta JDK 17), lingua `8051:BE:16:default`,
`BinaryLoader` base 0. Il loader non crea punti d'ingresso: lo script deve fare `disassemble()` + `createFunction()` su
`0x0000`, sui vettori `0x03 + 8n` e su `0xF000`, poi `analyzeAll()` (≈ 8600 istruzioni, 134 funzioni). I nomi dei SFR si
ricavano da `src/sino51lib/sh68f90/sh68f90.h` (`SFR(...)`/`SFRX(...)`). REA non supporta l'8051 (solo 6502/x86/arm/m68k).

## Struttura generale

* Compilato con **Keil C51** (non SDCC): startup che azzera IRAM (`0x9A26`) e XRAM (4 KiB), poi copia/azzera dati da una
  tabella di init a `0x9F42`; `SP=0x7D` (lo stack parte più in alto che in SMK).
* Mappa: reset `0x0000: LJMP 0x9A26`; app in `0x0000–0xEFFF`; bootloader `0xF000–0xFFFF`.
* Vettori (offset `0x03+8n`): `0x03→0x0181`, `0x0B→0xEDB6`, `0x13→0xEDBC`, `0x1B→0xEDC2`, `0x3B→0xAB30`, `0x43→0x0F3A`,
  `0x4B→0x30DC`, `0x53→0xEC41`, `0x63→0x8C5B`, `0x6B→0xA43C`; gli altri sono vuoti/dati. Corrispondenza con le
  sorgenti di interrupt del SH68F90 (`_INT_TIMER2`, `_INT_USB`, …): *TBD* assegnazione precisa.
* Orologio: `PLLCON=0x03`, `CLKCON=0x0C` (PLL acceso, sistema a 24 MHz, come SMK); sequenza di spegnimento
  `CLKCON&=~…; PLLCON&=~3` in più punti (sleep).

## GPIO a riposo (funzione a `0xA627`)

| Porta | `PxCR` (1=uscita) | `PxPCR` (pull-up) | valore `Px` iniziale |
| --- | --- | --- | --- |
| P0 | `0x04` (P0.2) | `0xF8` | `0x04` |
| P1 | `0x3F` (P1.0–5) | `0x3F` | `0x00` |
| P2 | `0x3F` (P2.0–5) | `0x3F` | `0x00` |
| P3 | `0x3F` (P3.0–5) | `0x3F` | `0x00` |
| P4 | `0x2F` (P4.0–3, P4.5) | `0x6F` | `0x2D` |
| P5 | `0x87` (P5.0–2, P5.7) | `0x9F` | `0x87` |
| P6 | `0xFF` | `0xFF` | `0xFF` |
| P7 | `0x70` (P7.4–6) | `0xFF` | `0x00` |

* Pilotaggio: `DRVCON` sblocca e azzera `P1DRV`, `P2DRV`, `P3DRV`, `P5DRV` (come SMK, che imposta 25 mA: *TBD* se lo
  stock usa il default).
* **Nota:** il SH68F90A esposto qui ha anche **P6 e P7** (SMK/NuPhy usano solo P0–P5). Se il package della B68 ha tutti
  quei pin, la matrice può usare porte che nelle altre tastiere non si usano.
* Un'altra funzione (`0x9C9A–0x9D0F`) imposta dopo il boot un secondo insieme di pin (P0.5–7, P4.0/2/3/5/6, P5.0–2/7,
  P6.3–7, P7.4) in uscita e abilita pull-up su P5.3/P5.4: probabile configurazione di **sleep/wake** o dei pin radio.
  *TBD*.

## Altri dati nella flash

* `0xB15A`: stringhe `"B68 3.0"` e `"B68 5.0"` → **nomi Bluetooth** annunciati (BT 3.0/5.0 → BK3632 in modalità
  Classic e BLE). Utile per il comando "set BT name" del BK3632.
* `0x5A59`: descrittore dispositivo USB (`258A:010C`, bcdDevice `0x2002`); descrittori di configurazione subito prima.
* `0xC480–0xC4FF`: due tabelle simmetriche (0→255→0), tipicamente **curve di luminosità/respirazione dei LED**.
* `0xC500–0xC5FF`: tabelle di mappatura a passi regolari: *TBD* (indici LED?).
* `0xC600–0xC67F`: **blocco impostazioni** con marker `5A A5` a `0xC67E`; contiene i 2 byte che cambiano fra le
  letture (`0xC60A`, `0xC65A`): stato corrente (modalità/effetto LED). È l'equivalente delle impostazioni NVM di SMK.
* **Keymap:** *non* trovata come tabella di codici HID contigua (cercati i pattern QWERTY/ASDF in chiaro). Probabilmente
  codificata in un formato indicizzato; *TBD*, richiede di seguire il codice che costruisce il report tastiera.

## Cosa manca per chiudere M1.6

1. Funzione di scansione matrice: quali pin sono righe/colonne e il ritardo di settling (porta il tempo di slot).
2. Keymap e layer (formato, dimensione, dove si salva la personalizzazione fatta dal software del produttore).
3. Protocollo verso BK3632 (linee, frame, comandi): confronto con `rf_controller.c`.
4. Driver LED (PWM, pin, frequenza) e sequenza di sleep/wake (INT4).

Le voci 1–3 si possono chiudere più in fretta con una **cattura con analizzatore logico** o misure sui pin
(M1.2/M1.5); la voce 4 è analisi statica. Il Nano non serve per questo.
