# Changelog — porting SMK su Yunzii B68

Registro delle funzioni del porting (branch `port/yunzii-b68`), dalla più recente. Il piano e lo stato dei task stanno in
[docs/porting-plan.md](docs/porting-plan.md); i dettagli hardware in [docs/keyboards/yunzii-b68.md](docs/keyboards/yunzii-b68.md).

## Stato attuale (riferimento per ripartire)

**Cosa funziona sulla tastiera** (verificato dall'utente): matrice 5×16, encoder (volume), livello Fn, Vial desktop (tastiera,
encoder, macro, tap dance), tap-hold, retroilluminazione RGB senza sfarfallio, tasti luce, Caps Lock sul LED del tasto.

**Build di rilascio (una sola)**

```
nix develop --command bash -c 'meson setup build -Dboards=yunzii-b68 --buildtype=release && meson compile -C build'
```

File: `build/yunzii-b68_default_smk.hex`. Flash dalla tastiera già in uso:
`sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/yunzii-b68_default_smk.hex`.
Recupero: Esc+1+2 mentre si inserisce l'USB (entra nel bootloader).

**Test**: `cd tests && python3 -m unittest test_yunzii_b68_matrix test_host_units` (circa 10 minuti; i test della B68 usano
`build/yunzii-b68_default_smk.hex`). Gli altri file (`test_boot*`, `test_usb*`, `test_stack`, `test_source_breakpoints`) sono scritti
per la NuPhy Air60 e si saltano da soli se la NuPhy non è stata compilata (`meson setup build` senza `-Dboards`).

**Mappa della flash**: applicazione fino a 0xDC00; tastiera salvata 2 copie × 4 settori da 0xDC00; impostazioni 0xEC00;
marcatore 0xEE00; bootloader stock 0xF000.

**Da fare / aperto**
- Profili (più tastiere complete selezionabili con un tasto); idea collegata: tempi di tap/hold per profilo nella scheda *Settings* di Vial
  (oggi la scheda è vuota: il firmware risponde "nessuna impostazione QMK").
- Radio 2.4G + BLE (BK3632): serve aprire la tastiera e catturare il bus con un analizzatore ([docs/radio-capture-guide.md](docs/radio-capture-guide.md)).
- LED di stato accanto al Caps: nessun pin trovato lo accende; con il driver nuovo non è chiaro se pulsi ancora.
- Fn+A / S / D sono tre esperimenti sul driver LED (`B68_LED_T1..T3`): da togliere quando il LED di stato è chiarito.
- HUD: l'app dell'utente deve adottare il protocollo 2 ([docs/hud-protocol-v2.md](docs/hud-protocol-v2.md)).
- Programmatore ICP (Arduino Nano) non ancora arrivato: il recupero oggi è solo via ISP/accordo Esc+1+2.

## 2026-11-01

- **LED**: i valori PWM di ogni LED sono calcolati quando l'effetto viene disegnato, non nell'interrupt: l'interrupt del sottoframe costa circa
  6 volte meno (da ~54.000 a ~8.500 cicli nel simulatore). Era la causa probabile della latenza nello scrivere.
- **LED**: il ridisegno dell'effetto avviene a pezzi di 8 LED per giro del ciclo principale; l'animazione avanza ogni 9 cornici (~72 ms).
- **LED**: tre effetti nuovi (diagonale, respiro, pulsazione bianca); in tutto 7 più "spento".
- **Test**: i test scritti per la NuPhy si saltano se la NuPhy non è compilata (prima usavano per errore l'hex della B68 e fallivano).

## 2026-10-31

- **Tap dance** (Vial, 8 voci): il tasto dell'encoder è `TD(0)`: tocco = Mute, tenuto = Play/Pausa, doppio tocco = traccia successiva.
  Modificabile dalla scheda *Tap Dance*. Comandi Vial `dynamic_entry` 0x00/0x01/0x02. Le voci stanno nella tastiera salvata.
- **Tastiera salvata**: formato versione 4. L'intestazione porta l'impronta della tastiera predefinita del firmware (2 byte): se un firmware
  nuovo cambia la predefinita, la copia salvata si scarta. Per questo, dopo l'aggiornamento le modifiche fatte in Vial vanno rifatte una volta.
- **Sfarfallio LED**: `LED_SUBFRAMES_PER_SCAN` da 2 a 4 (cornice ≈ 8 ms).
- **Build**: un solo layout `default` con raw HID (Vial/VIA/HUD) e bcdDevice 0x0200; rimossi `rawhid`, `hudv1`, `probe`, `ledprobe`, `ledprobe2`.
  Opzione `-Dboards=<scheda>` per configurare solo quella scheda.
- **Retroilluminazione RGB**: pilotata a colonne (il pozzo di ogni LED è la linea di scansione della sua colonna), 15 canali PWM, 16 sottoframe
  per cornice. Tasti luce sul livello Fn: Q/W effetto, E/R luminosità, T/Y velocità. Caps Lock = tasto Caps bianco.
- **LED**: mappa canali, forma d'onda PWM (alta tra DUTY1 e DUTY2) e sonde in [docs/led-probe.md](docs/led-probe.md) (le build di sonda
  sono state tolte dal repository: restano nella storia git).
- **HUD**: protocollo v2 per tastiere con più di 8 colonne, comando `GET_MATRIX` 0x04; descrizione in [docs/hud-protocol-v2.md](docs/hud-protocol-v2.md).
- **Macro** (buffer Vial da 512 B, 16 macro, salvate con la tastiera) e **protocollo HUD** (0xF0 su raw HID).
- **Tap-hold**: `LT`, `MT`, `LM`, `TT` sull'orologio al millisecondo (calibrato sul SOF USB); `bcdDevice` per layout.
- **Vial desktop**: il numero di serie contiene `vial:f64c2b3c`; supporto encoder (comandi 0x03/0x04).
- **USB**: interfaccia 0 = tastiera (ID 3), sistema (1), consumer (2), report ISP (5); interfaccia 1 = raw HID 32 B (pagina 0xFF60).
  Spike S1 superato: Vial funziona su raw HID.
- **Encoder**: decoder in quadratura campionato nel tick, mappa per livello, P0.6/P0.7.
- **Tastiera**: layout reale della B68 ricavato con la sonda (66 tasti + click encoder), `vial.json` con i tasti fisici.
- **Tastiera salvata**: copia in RAM di 8 livelli, due copie A/B in flash, scrittura con l'intestazione per ultima (resiste a un'interruzione di corrente).
- **Layer**: `MO`, `TO`, `TG`, `DF`, `OSL`, `OSM` su 8 livelli.
- **Recupero**: accordo Esc+1+2 all'accensione entra nel bootloader.
- **Scheda B68**: matrice 5×16, stato GPIO a riposo dello stock, ganci di scansione; piano e analisi del firmware stock in [docs/](docs/).
