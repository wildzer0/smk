# SMK — Architettura e design di firmware e bootloader

> Documento di studio per chi deve mettere mano al codice (porting su Yunzii B68 + VIAL).
> Scritto in italiano; i termini tecnici restano in inglese. Tutto ciò che è affermato qui è stato
> ricavato leggendo il sorgente alla revisione `dbd5c54` (master) o misurato con una build `release`
> di `nuphy-air60` (SDCC 4.5.0). Dove un fatto **non** è verificabile dal repository lo dico
> esplicitamente ("da verificare").
>
> Documenti collegati: [usb-hid.md](usb-hid.md) (stack USB/HID), [recovery-icp.md](recovery-icp.md) (recupero),
> [tasks/](tasks/README.md), [porting-plan.md](porting-plan.md) (piano di sviluppo),
> [REFERENCE.md](REFERENCE.md) (materiale di studio), [bootloader.md](bootloader.md) (documento
> upstream del bootloader), [platforms/sh68f90.md](platforms/sh68f90.md),
> [keyboards/nuphy-air60.md](keyboards/nuphy-air60.md).

---

## Indice

1. [Panoramica](#1-panoramica)
2. [Mappa del repository](#2-mappa-del-repository)
3. [Piattaforma hardware: SH68F90A / BYK916](#3-piattaforma-hardware-sh68f90a--byk916)
4. [Toolchain e modello di compilazione (SDCC)](#4-toolchain-e-modello-di-compilazione-sdcc)
5. [Sistema di build (Meson)](#5-sistema-di-build-meson)
6. [Avvio e main loop](#6-avvio-e-main-loop)
7. [Scheduling: Timer2 e tick](#7-scheduling-timer2-e-tick)
8. [Pipeline dei tasti: dalla matrice all'host](#8-pipeline-dei-tasti-dalla-matrice-allhost)
9. [Stack USB](#9-stack-usb)
10. [Wireless: BK3632](#10-wireless-bk3632)
11. [Persistenza (settings / NVM)](#11-persistenza-settings--nvm)
12. [Indicatori / retroilluminazione](#12-indicatori--retroilluminazione)
13. [Sleep e power-down](#13-sleep-e-power-down)
14. [Il contratto di una scheda (board port interface)](#14-il-contratto-di-una-scheda-board-port-interface)
15. [Bootloader ISP di SMK](#15-bootloader-isp-di-smk)
16. [Test e simulatore](#16-test-e-simulatore)
17. [Budget di risorse (misurato)](#17-budget-di-risorse-misurato)
18. [Osservazioni rilevanti per il lavoro che ti aspetta](#18-osservazioni-rilevanti-per-il-lavoro-che-ti-aspetta)

---

## 1. Panoramica

SMK è un firmware per tastiere con MCU SinoWealth a core 8051, compilato con SDCC. Il
progetto ha **due immagini indipendenti** che convivono nella flash da 64 KiB:

```
          ┌───────────────────────────── flash 64 KiB ─────────────────────────────┐
 0x0000   │ LJMP bootloader (3 byte, scritti dal comando di erase del bootloader)  │
 0x0003   │ APPLICAZIONE (firmware): vettori interrupt + codice                    │
 0xEBFF   │   └─ limite del codice: 60416 B (--code-size = FLASH_CFG_ADDR)         │
 0xEC00   │ settings sector (512 B)  ← FLASH_CFG_ADDR                              │
 0xEE00   │ "marker" sector (512 B)  ← contiene LJMP <entry app> a 0xEFFB          │
 0xF000   │ BOOTLOADER ISP (4 KiB)                                                 │
 0xFF00   │   └─ entry stub (ISP_ENTRY, `ljmp` di ingresso da applicazione)        │
          └─────────────────────────────────────────────────────────────────────────┘
```

(indirizzi per SH68F90A; vedi [§3](#3-piattaforma-hardware-sh68f90a--byk916) e [§15](#15-bootloader-isp-di-smk))

Struttura logica del firmware applicativo, a strati:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ src/main.c   init() + super-loop                                            │
├───────────────┬───────────────────────────┬─────────────────────────────────┤
│ src/smk/      │ src/kb/  src/user/        │ src/keyboards/<board>/          │
│ core generico │ hook "weak" di default    │ codice specifico della scheda   │
│ (matrix,      │ (kb_*, indicators_*,      │ (kbdef.h, pin, layout, LED,     │
│  report, usb, │  user_*) sovrascritti     │  sleep, kb.c)                   │
│  settings,    │  dalla scheda             │                                 │
│  sleep, tick) │                           │                                 │
├───────────────┴───────────────────────────┴─────────────────────────────────┤
│ src/peripherals/  (bk3632 = radio, eeprom_24cxx)   src/nvm/flash            │
├─────────────────────────────────────────────────────────────────────────────┤
│ src/sino51lib/    HAL generica + un sottodirectory per MCU (sh68f90, ...)   │
└─────────────────────────────────────────────────────────────────────────────┘
```

Principio di design ricorrente (importante per capire dove mettere il tuo codice): **il core non
conosce la scheda**. Il core chiama funzioni dichiarate in header (`user_*.h`, `kb.h`,
`indicators.h`); il progetto fornisce implementazioni vuote di default in `src/user/` e `src/kb/`,
impacchettate in una **libreria statica** (`user.lib`). Il linker risolve prima i simboli
dagli `.rel` della scheda e solo dopo dalla libreria: se la scheda definisce la funzione, vince
quella della scheda, altrimenti si usa il default. È l'equivalente dei simboli `weak` di GCC, ottenuto
con l'ordine di link (vedi `lib_user` in [meson.build](../meson.build)).

---

## 2. Mappa del repository

| Percorso | Contenuto |
| --- | --- |
| [src/main.c](../src/main.c) | `init()` e super-loop |
| [src/smk/](../src/smk) | core: `matrix`, `report`, `host`, `usb`, `settings`, `sleep`, `tick`, `led_effect`, `console`, `keycodes.h` (tabella keycode compatibile QMK) |
| [src/kb/](../src/kb) | default vuoti di `kb_init`, `kb_update`, `kb_update_switches`, `kb_process_record` |
| [src/user/](../src/user) | default vuoti di indicatori, matrix hooks, `layout_process_record`, `user_init`, `settings_save_pre/post` |
| [src/keyboards/](../src/keyboards) | una cartella per scheda (+ `example/` come scheletro) |
| [src/sino51lib/](../src/sino51lib) | HAL: `gpio.h` (macro), `delay`, `flash` (SSP), `usbdef.h`/`usbhw.h`, `uart`, `spi`, `twi`, `pwm`, `pca`, `bb_i2c`, `startup.c`, … ; `sh68f90/`, `sh68f89/`, `sh68f881/` = SFR e driver per-MCU; `examples/` = firmware minimi che esercitano la libreria |
| [src/peripherals/bk3632/](../src/peripherals/bk3632) | driver radio: `bb_spi.c` (SPI bit-bang), `rf_controller.c` (protocollo a frame) |
| [src/peripherals/eeprom_24cxx/](../src/peripherals/eeprom_24cxx) | store alternativo (EEPROM I²C esterna), usato solo da ciy-x77 |
| [src/nvm/flash/](../src/nvm/flash) | store dei settings su un settore di flash programma |
| [src/boot/](../src/boot) | **bootloader ISP** (immagine separata, vedi [§15](#15-bootloader-isp-di-smk)) |
| [utils/](../utils) | script di build: `sdcc_compile.py` (wrapper con depfile), `check_interrupts.py`, `boot_check.py`, `boot_image.py`, `boot_chord_gen.py`, `led_geometry_gen.c` |
| [tests/](../tests) | test su simulatore uCsim patchato (Python `unittest`) |
| [tools/](../tools) | `smk-console` (Rust, console di debug via HID), patch di uCsim |
| [docs/](.) | documentazione |

File di build: [meson.build](../meson.build), [meson.options](../meson.options), `src/**/meson.build`,
[flake.nix](../flake.nix) (ambiente riproducibile Nix: sdcc, meson, sinowisp, sinodude, ucsim).

---

## 3. Piattaforma hardware: SH68F90A / BYK916

Il BYK916 è il package/marcatura del SinoWealth **SH68F90A** (l'upstream li tratta come lo
stesso silicio: `docs/keyboards/nuphy-air60.md` riporta "BYK916 (SH68F90A)"). **Non** possediamo il
datasheet ufficiale nel repo: i dati sotto vengono dalle definizioni SFR e dai driver
(`src/sino51lib/sh68f90/`), e sono quindi quelli *usati* dal firmware, non tutto ciò che il chip può fare.

### 3.1 Memorie

| Risorsa | Valore | Fonte |
| --- | --- | --- |
| Core | 8051 **1T** (1 machine cycle = 1 clock), SYSCLK **24 MHz** da PLL | `delay.c`, `clock.c`, `spec['freq_sys']` |
| Flash programma | 65536 B; sector erase = **512 B** | `meson.build` platform spec |
| IRAM | 256 B (stack ≈ 222 B liberi nell'immagine applicativa) | `.mem` della build |
| XRAM | 4096 B per codice utente (`--xram-size 4096`) | spec |
| XRAM "USB window" | buffer endpoint a `0x1100`–`0x11FF` | `sh68f90.h` righe 824-837 |
| SFR extended (PWM) | `0xFF80`–`0xFFFF` in spazio XDATA (`SFRX`) | `sh68f90.h` |
| Data flash (EEPROM-like) | **assente** su questo MCU | `docs/platforms/sh68f90.md` |
| Information block | `0x1000+` nella finestra FAC, sola lettura | idem |

Nota importante sul `startup.c`: `P2` è una porta GPIO, ma SDCC inizializza la XRAM con `MOVX @Ri`
(paged, usa `P2` come byte alto): per questo c'è una `__sdcc_external_startup` riscritta con DPTR
([startup.c](../src/sino51lib/startup.c)). È un classico dei port 8051 con P2 riusato: **non toccarla**.

### 3.2 Periferiche usate dal firmware

| Blocco | Uso nel progetto |
| --- | --- |
| GPIO P0…P7 | matrice (colonne in uscita, righe in input con pull-up), righe RGB, switch, SPI radio bit-bang. Macro in [gpio.h](../src/sino51lib/gpio.h) (`GPIO_OUTPUT/INPUT/HIGH/LOW/PULLUP_ON/OFF`, `GPIO_DIR_WRITE`, drive strength via `DRVCON`/`PxDRV`) |
| PWM (5 banchi) | duty dei LED per colonna (nuphy: `PWM00…PWM42`) |
| **Timer2** | tick di sistema (scan matrice + sub-frame LED), vedi [§7](#7-scheduling-timer2-e-tick) |
| **USB device** | 3 endpoint: EP0 (control, 8 B), **EP1** (16 B), **EP2** (64 B) — vedi [§9](#9-stack-usb) |
| INT4 | wake da power-down |
| SSP (flash self-programming) | `IB_CON1..5` + `XPAGE`/`IB_OFFSET`/`IB_DATA`; chiavi fisse. `flash.c` |
| Watchdog | `RSTSTAT = WATCHDOG_PERIOD` (`watchdog_kick()` ovunque nei loop lunghi) |
| UART (EUART) | solo debug (`-Ddebug_sink=uart`); **inutilizzabile su nuphy-air60** perché `TXD` = P5.5 = `CONN_MODE_SWITCH` (c'è un `#error` in `user_init.c`) |
| SPI hardware | **non usato** dal radio: pin occupati, quindi SPI **bit-bang** |

SFR paging: sul SH68F90 gli SFR USB stanno su "page 1" (`INSCON`); `flash.c` e `usbhw.c` salvano/ripristinano
la pagina. `usb_interrupt_handler` salva `INSCON` e `FLASHCON` perché il prologo ISR di SDCC non lo fa.

### 3.3 Code option (fuse)

Il firmware **dipende** dalle code option programmate in fabbrica (oscillatore interno 24 MHz,
livelli I/O, drive, ISP abilitato su pin, …). Il documento della NuPhy Air60 elenca quelle richieste
(`Code Option String: A4E063C00F000088`). Le code option si scrivono **solo via ICP**: sono
parte del rischio di porting, vedi [porting-plan.md](porting-plan.md).

### 3.4 Interrupt reclamati (release, nuphy-air60)

`check_interrupts.py` dichiara 14 vettori reclamati (gli "unused" puntano a handler che mascherano la sorgente
e, in debug, loggano il numero del vettore: un IRQ spurio non può rientrare in loop fino al watchdog). Usati davvero: **USB**, **Timer2**, **INT4**, **PWM0** (+ EUART0 se
debug UART). Header: [sh68f90/interrupts.h](../src/sino51lib/sh68f90/interrupts.h).

---

## 4. Toolchain e modello di compilazione (SDCC)

Argomenti principali (da [meson.build](../meson.build)):

```
-mmcs51 --std-c23 --Werror
--model-large --stack-auto        # variabili in XRAM, funzioni reentrant con stack
--xram-loc 0x0000 --xram-size 4096
--code-size <FLASH_CFG_ADDR>      # il linker rifiuta di superare l'inizio del settings sector
--opt-code-speed
```

Conseguenze pratiche che devi avere sempre in testa:

* **Tutto è in XRAM** per default (`--model-large`). L'accesso a XRAM costa (MOVX + DPTR): le variabili
  "calde" in ISR meritano `__data`/`__idata` solo se sai cosa fai.
* **`--stack-auto`**: le variabili locali stanno sullo stack (IRAM, ~222 B liberi). *Stack profondo + printf
  = rischio*: c'è `tests/test_stack.py`.
* **Overlay statico di SDCC**: con funzioni non reentrant le locali possono condividere memoria tra
  funzioni diverse. Se una funzione raggiungibile da una ISR condivide l'overlay con una del main loop si
  ha corruzione silenziosa. [check_interrupts.py](../utils/check_interrupts.py) analizza gli `.asm` e **fa
  fallire la build** in quel caso. Quando aggiungi codice richiamato da ISR (es. gestione EP2 OUT per Raw
  HID) questa è la rete di sicurezza: se scatta, marca la funzione `__reentrant` o spostala fuori dalla ISR.
* **`__critical`** disabilita `EA` per il blocco: usato in `flash.c` (finestra FAC attiva = MOVC va nella
  data flash) e in `bb_spi.c` per la sezione temporizzata.
* **Indirizzi assoluti** (`__at`) per registri USB (`0x1100…`) e per il magic della richiesta ISP.
* **Header "pericolosi"**: [src/utils/8051_inc_error_hide.h](../src/utils/8051_inc_error_hide.h) nasconde errori
  di `8051.h`/`stdbool` interni a SDCC.
* Il **bootloader** usa un modello diverso: `--model-small`, niente `--stack-auto`, `--opt-code-size`, niente ISR
  (polling). Deve stare in 3840 B.

Standard C: `--std-c23` (SDCC 4.5 lo supporta); il progetto usa `enum : uint16_t` (vedi `keycodes.h`).

---

## 5. Sistema di build (Meson)

Meson non conosce SDCC come compilatore: il progetto lo invoca da `generator()` tramite il wrapper
[sdcc_compile.py](../utils/sdcc_compile.py), che produce anche il **depfile** (così una modifica a un
header ricompila gli `.rel` giusti). Poi `sdcc` fa il link, `packihx` converte `.ihx → .hex`.

### 5.1 Registro dichiarativo

Ogni sottodirectory aggiunge voci a dizionari globali (nel `meson.build` radice):

* `platforms` — una voce per MCU (`src/sino51lib/sh68f90/meson.build`):
  `dir`, `src`, `boot_core`, `example_*`, `flash_size`, `bootloader_size`, `sector_size`, `isp_page_size`,
  `ea_bit`, `xram_size`, `freq_sys`, `settings_store` (`'flash'` o altro).
* `wireless_modules` — `bk3632` → sorgenti + `-DRF_ENABLED=1`.
* `nvm_modules` — `flash` / `eeprom_24cxx`.
* `keyboards` — una voce per scheda (`src/keyboards/<board>/meson.build`):
  `dir`, `platform`, `icp_part`, `src`, `boot_src`, `layouts{ nome:{dir,src} }`, `options{…}`.

Per scheda × layout il `meson.build` radice genera: `<board>_<layout>_smk.ihx/.hex`,
`…_full.hex` (app + bootloader), `…_flash` (sinowisp), `…_full_flash` (ICP via sinodude),
`<board>_boot.hex`, `<board>_boot_flash`.

### 5.2 Opzioni (meson.options)

`nkro`, `isp`, `sleep` (feature), `debug_sink` (`console|uart|both`), `usb_manufacturer/product/serial`,
`boot`, `boot_chord`, `icp_programmer`, `icp_power`, `icp_port`. `buildtype=debug` aggiunge `-DDEBUG=1` e
`--debug` (simboli `.cdb` per i test su simulatore).

### 5.3 Calcolo automatico della mappa flash

Da platform spec (nuphy, `flash_size=65536`, `bootloader_size=4096`, `sector_size=512`):

```
bootloader_addr = 65536 - 4096      = 0xF000
marker_addr     = 0xF000 - 512      = 0xEE00
settings_addr   = marker_addr - 512 = 0xEC00        (solo se settings_store == 'flash')
--code-size 0xEC00 = 60416 B         -DFLASH_CFG_ADDR=0xEC00u  -DFLASH_CFG_SIZE=512u

Se la scheda ha l'opzione `keymap_sectors = N` (keymap modificabile a runtime), sotto le impostazioni si riservano
due copie A/B da N settori ciascuna:
keymap_addr     = settings_addr - 2·N·512                 (B68, N = 3: 0xE000)
--code-size 0xE000 = 57344 B         -DFLASH_KEYMAP_ADDR=0xE000u  -DFLASH_KEYMAP_SECTORS=3u
```

**Nota per VIAL:** oggi esiste *un solo* settore di settings. Per la keymap dinamica servono più settori:
dovrai parametrizzare questo calcolo (vedi piano, Milestone 4).

### 5.4 Geometria LED generata

[led_geometry_gen.c](../utils/led_geometry_gen.c) è compilato **dall'host** al `meson setup` e genera
(per scheda/layout) un header e un `.c` con tabelle ROM per gli effetti (radiale/assi). Opzioni di
scheda `led_rows`, `led_cols`, `led_cx2`, `led_cy2` (centro ×2). Una `_Static_assert` verifica che
coincida con `MATRIX_ROWS×MATRIX_COLS`.

---

## 6. Avvio e main loop

### 6.1 `init()` ([main.c](../src/main.c))

Ordine esatto:

```
reset_init → ldo_init → clock_init → peripherals_init → [uart_init]
→ user_init()            ← la scheda configura GPIO e PWM
→ matrix_init → keyboard_init → usb_init → indicators_init
→ tick_init()            ← parte Timer2 (da qui la matrice viene scandita in ISR)
→ EA = 1
```

### 6.2 `main()`

```
init();  banner debug
kb_init();                       ← la scheda legge gli switch (conn/os) e sceglie default layer
rf_init();            (RF_ENABLED) ← ~1,5 s di attesa bloccante all'avvio, poi nome BT e link 2.4G
restore_settings();              ← settings_load() oppure indicators_apply_defaults()
usb_wait_for_enumeration();      ← fino a 4 s, 500 ms se nessun host
indicators_start();
restore_rf_link();
sleep_init();
for (;;) {
    watchdog_kick();
    kb_update_switches();        ← debounce slider (256 iterazioni) → cambia modalità USB/RF e OS
    kb_update();                 ← supervisor radio, flush report pendenti
    matrix_task();               ← consuma gli eventi di matrice → keycode → report
    indicators_render();         ← rigenera il framebuffer LED
    usb_task();                  ← es. salto in ISP richiesto dalla ISR
    settings_task();             ← salva i settings se "dirty"
    sleep_task();
    [debug] diag_task(); interrupts_task(); console_task();
}
```

Il main loop **non è temporizzato**: la cadenza è quella di scansione/ISR; qualsiasi cosa lunga qui
(erase flash, ritardi radio) ritarda tastiera, sleep e USB. Regola d'oro: niente `delay_ms` lunghi nel
super-loop senza `watchdog_kick()` (la `delay_*` lo fa già internamente).

---

## 7. Scheduling: Timer2 e tick

[tick.c](../src/smk/tick.c) + [sh68f90/systick.c](../src/sino51lib/sh68f90/systick.c).

Un unico timer (Timer2) serve **due compiti mutuamente esclusivi**: scandire la matrice e emettere
sub-frame PWM dei LED (la corrente dei LED si accoppia nel sense delle righe e falsa la lettura). Per
questo i tick si **alternano**:

```
 tick N   : matrix_scan_full()     → reload ≈ 100 µs ("RELOAD_MATRIX_SCAN", sweep reale ≈ 320 µs)
 tick N+1…: led subframe ×LED_SUBFRAMES_PER_SCAN  → reload 400 µs ("RELOAD_LED_SUBFRAME")
```

Dettagli:

* `matrix_scan_full()` gira **in ISR**: spegne il PWM LED (`indicators_pwm_disable`), mette i sink a zero,
  pilota una colonna alla volta a livello basso, legge le righe due volte (10 µs di distanza) e accetta il
  campione solo se i due coincidono (debounce elettrico), poi riabilita il PWM.
* Il risultato è in `matrix[]` (un `uint8_t` per colonna → **massimo 8 righe**!). `matrix_updated` avvisa il
  main loop; `matrix_task()` fa lo snapshot e il diff con `matrix_previous[]`.
* Non c'è debounce "per tasto" a livello software (solo la doppia lettura): il debounce vero è
  implicitamente la cadenza di scan.
* `systick_pause/resume` sospendono `ET2` durante lo sleep.

**Implicazione per te:** qualunque operazione che blocchi le interrupt a lungo (`__critical` esteso,
flash erase) perde scan e produce glitch. Il programma di un byte (SSP) è breve; l'erase di un settore da 512 B
è l'operazione più lunga: vedi osservazioni in [§18](#18-osservazioni-rilevanti-per-il-lavoro-che-ti-aspetta).

---

## 8. Pipeline dei tasti: dalla matrice all'host

```
matrix_scan_full()  (ISR Timer2)
        │  matrix[col] = ~righe (bit=1 → premuto)
        ▼
matrix_task()  (main loop)           ← diff con matrix_previous; sleep_note_activity()
        ▼
process_key_state(row,col,pressed)
   qcode = layer_key_down(row,col)   (pressione: cerca nei layer attivi, dal più alto; KC_TRANSPARENT scende;
                                      l'ultimo è il default layer; il risultato viene ricordato per posizione)
         | layer_key_up(row,col)     (rilascio: restituisce il keycode ricordato alla pressione)
   ├─ layer_process(qcode)           ← MO/TO/TG/DF/OSL/OSM: gestiti qui, [return]
   ├─ layer_key_pressed()            ← (solo alla pressione) consuma gli one-shot armati
   ├─ kb_process_record(qcode, pressed)          ← hook di scheda (false = consumato)
   ├─ layout_process_record(qcode, pressed)      ← hook di layout (false = consumato)
   ▼
send_keycode(qcode, pressed)
   ├─ modificatore  → add_mods/del_mods  + send_keyboard_report()
   ├─ keycode base  → add_key/del_key    + send_keyboard_report()
   ├─ system        → host_system_send()
   └─ consumer      → host_consumer_send()
   └─ layer_key_released()          ← (solo al rilascio) toglie i modificatori one-shot
        ▼
report.c: send_6kro_report() / send_nkro_report()   (solo se il report è cambiato — memcmp con l'ultimo)
        ▼
host.c: host_keyboard_send / host_nkro_send / host_extra_send
        ▼
kb_send_report / kb_send_nkro / kb_send_extra   ← **della scheda**: instrada per modalità
        ├─ USB  → usb_send_report / usb_send_nkro / usb_send_extra  (EP1 / EP2)
        └─ RF   → rf_send_report / rf_send_nkro / rf_send_extra     (SPI → BK3632)
```

### 8.1 Keymap

* `keymaps[layer][MATRIX_ROWS][MATRIX_COLS]` è un `const uint16_t[]` **in flash codice**, definito nel
  `layout.c` della scheda con una macro `LAYOUT_xx(...)` che mappa la geometria fisica sulla matrice
  elettrica (celle vuote = `KC_NO`).
* Lo spazio dei keycode è [keycodes.h](../src/smk/keycodes.h): **numerazione QMK ≥ 0.19** (es.
  `QK_MOMENTARY = 0x5220`, `QK_KB = 0x7E00`, `QK_USER = 0x7E40`). È una buona notizia: VIAL usa gli stessi
  valori numerici (da confermare contro la versione di vial-gui target, vedi piano).
* Layer: i layer della nuphy sono 4 (`_MAC_BL`, `_WIN_BL`, `_MAC_FL`, `_WIN_FL`); gli slider hardware
  selezionano il *default layer* (OS) — non è un concetto VIA/VIAL standard.

### 8.2 Cosa il motore dei tasti **non** fa (oggi)

Solo `MO(n)` è implementato nel core. **Non esistono**: `TO`, `TG`, `LT`, `MT`, `OSM/OSL`, `DF`, `TT`,
macro, combo, tap dance, mouse keys, auto-repeat dei layer a più livelli. I keycode custom della scheda
(`LNK_BT1`, `FX_NEXT`, …) passano per `kb_process_record`. Tutto ciò è rilevante per VIAL
([§18](#18-osservazioni-rilevanti-per-il-lavoro-che-ti-aspetta)).

### 8.3 Report

[report.c](../src/smk/report.c) / [report.h](../src/smk/report.h): stato `real_mods`/`weak_mods` +
`keyboard_report` (8 B, 6KRO) o `nkro_report` (`NKRO_REPORT_BITS = 20` → 22 B con ID e mods; il limite di 20
è dettato dal descrittore del dongle wireless). `host_nkro_active()` = protocollo "report" **e**
`keymap_config.nkro`.

---

## 9. Stack USB

[smk/usb.c](../src/smk/usb.c) (~1170 righe): stack USB device scritto a mano, state machine per EP0, sopra al
driver di piattaforma [sh68f90/usbhw.c](../src/sino51lib/sh68f90/usbhw.c) e agli header
[usbdef.h](../src/sino51lib/usbdef.h) / [usbhw.h](../src/sino51lib/usbhw.h).

### 9.1 Topologia attuale (wired)

```
Device  VID:PID (per scheda; nuphy 05ac:024f)   bMaxPacketSize0 = 8,  1 configurazione
 ├─ Interface 0  HID, subclass BOOT, bInterfaceProtocol = 1 (keyboard; nel codice `USB_IFACE_PROTOCOL_REPORT`)
 │     EP1 IN  (0x81)  interrupt, wMaxPacketSize 16, bInterval 1 ms
 │     Report descriptor: tastiera 6KRO (8 B) + LED OUT (1 B)
 └─ Interface 1  HID, subclass none, bInterfaceProtocol = 0 (none; nel codice `USB_IFACE_PROTOCOL_BOOT`, nome fuorviante)
       EP2 IN  (0x82)  interrupt, wMaxPacketSize 64, bInterval 1 ms
       Report descriptor (con Report ID):
         ID 1 System Control (3 bit)      ID 2 Consumer (16 bit)
         ID 5 Vendor FF00 Feature (ISP command, 5 B)
         ID 7 Console FF31 Input (32 B)  [solo DEBUG]
         ID 6 NKRO keyboard [solo NKRO_ENABLE]
```

Buffer hardware (XRAM `0x1100…`): `EP0_OUT/IN` 8 B, `EP1_OUT/IN/COM` 16 B, `EP2_OUT/IN/COM` 64 B.
Il blocco USB espone quindi **OUT** oltre a IN su EP1 ed EP2 (registri `OEP1*/OEP2*`, flag `_OEP2IF`, …).
Oggi nessun endpoint OUT è usato: tutto l'host→device passa da **EP0** (SET_REPORT: LED, ISP, console).
Cosa fa esattamente il buffer "COM" e se ci sia ping-pong (`_IEP1BUFSEL/_OEP1BUFSEL`) è **da verificare** sul datasheet.

### 9.2 Flusso interrupt

* `usb_interrupt_handler` (vettore USB) salva `INSCON`/`FLASHCON` e chiama `usb_irq_dispatch()`.
* `usb_irq_dispatch` gestisce: reset bus (SW reset del blocco + `usb_init()`), SOF (conta il "silenzio" per
  `usb_wait_for_enumeration`), suspend/resume, **SETUP**, `OEP0IF`/`IEP0IF`, completamento IN EP1/EP2
  (vuoto: si fa polling su `IEPxRDY`).
* **`USBIE2` abilita solo `OEP0IE|IEP0IE`.** Gli IN EP1/EP2 sono "fire and poll": `usb_hw_epN_in_send`
  attende fino a ~10 ms (255×40 µs) che l'IN precedente sia consumato.
* EP0: macchina a stati `IDLE → DATA_IN → STATUS_OUT`, `DATA_OUT → STATUS_IN` (riscritta di recente,
  commit `f0ee2e0`). I trasferimenti IN sono spezzati in pacchetti da 8 B.
* Descrittori composti al volo in `scratch[512]` (buffer comune); stringhe da `USB_STR_*`.

### 9.3 Canali host→device esistenti

| Canale | Dove | Uso |
| --- | --- | --- |
| SET_REPORT Output, iface 0, 1 B | `EP0_OUT_LED` | stato LED lock → `keyboard_set_led_state` |
| SET_REPORT Feature, iface 1, ID 5 | `EP0_OUT_ISP` | comando `0x75` → `usb_isp_requested` → il main loop chiama `isp_jump()` |
| SET_REPORT Feature, iface 1, ID 7 | `EP0_OUT_CONSOLE` | solo DEBUG |

**`isp_jump()` non viene chiamata dalla ISR** (commento in `usb_task`): il bootloader non esegue mai `RETI`
e lascerebbe il latch di interrupt-in-servizio attivo, uccidendo USB. È un pattern da copiare per Raw HID:
**la ISR accoda, il main loop elabora**.

### 9.4 Ingresso in ISP dall'applicazione

`isp_jump()` → `LJMP ISP_ENTRY` (`0xFF00`) con `A=0x5A, B=0xA5` → lo stub scrive il magic in XRAM
(`BOOT_REQUEST_ADDR`, 4 byte in fondo alla XRAM) e fa `LJMP BOOT_BASE`. Al "boot" successivo il
bootloader trova il magic e resta in modalità ISP. Questo è l'unico accoppiamento app↔bootloader.

---

## 10. Wireless: BK3632

Il BK3632 (Beken, SoC BT+2.4G) è un chip **con firmware proprio**: l'MCU non ne controlla il radio, gli
manda comandi di alto livello ("invia questo report HID", "usa il link BT2", "entra in pairing"). Il
protocollo di SMK è stato ricavato per reverse engineering del firmware stock **NuPhy**; non c'è
documentazione pubblica del protocollo (vedi rischi nel piano).

### 10.1 Livello fisico — [bb_spi.c](../src/peripherals/bk3632/bb_spi.c)

SPI **bit-bang open-drain** a 5 linee (nuphy-air60, `kbdef.h`):

| Segnale | Pin MCU | Verso | Ruolo |
| --- | --- | --- | --- |
| `RF_BB_SPI_CS` | P7.4 | MCU→radio | chip select (open-drain) |
| `RF_BB_SPI_SCK` | P4.7 | MCU→radio | clock (open-drain) |
| `RF_BB_SPI_MOSI` | P0.7 | MCU→radio | dati |
| `RF_BB_SPI_MISO` | P0.6 | radio→MCU | dati (campionato con SCK basso) |
| `RF_BB_SPI_MOT` | P0.5 | MCU→radio | "attenzione/wake": portata bassa ~3 µs prima del CS |
| `RF_BB_SPI_ACK` | P4.2 | radio→MCU | cambia livello quando il radio ha accettato il frame (poll max 50×3 µs) |
| `KB_WAKE` | P4.1 | — | pin di wake usato nel parcheggio sleep |

`bb_spi_xfer` = burst + attesa del toggle di ACK; `bb_spi_recv` = lettura senza ACK, **con interrupt
disabilitati** (`__critical`).

### 10.2 Livello applicativo — [rf_controller.c](../src/peripherals/bk3632/rf_controller.c)

Frame: `0xAA | len-3 | cmd | payload… | checksum`, con `checksum = 0x55 − Σ(byte precedenti)`; la risposta di
stato ha header `0xBB`.

| cmd | Funzione nel codice | Significato (ricavato dal codice) |
| --- | --- | --- |
| `0x01` | `rf_set_link_mode(mode, pairing)` | seleziona link (2.4G, BT1-3) e pairing on/off |
| `0x02` | `rf_send_kro_report` / `rf_send_nkro_report` | report tastiera (6KRO: mods + 6 key; NKRO: mods + 20 B bitmap) |
| `0x03` | `rf_cmd_03(2)` | wipe dei bond BT (factory reset) |
| `0x04` | `rf_cmd_04` | commit dopo set nome |
| `0x05` | `rf_send_consumer_system` | consumer/system usage a 16 bit |
| `0x06` | `rf_cmd_06(1)` | "modalità USB" (radio non attivo) |
| `0x07` | `rf_sleep` | sleep del radio |
| `0x08` | `rf_set_bt_name` | nome BT (BT3.0 / BT5.0) |
| `0x0A` | `rf_query_status` | richiesta stato (2 byte: batteria 3 bit, LED lock, connected, paired, link) |
| `0x0B` / `0x0C` | `rf_wake` / `rf_wake_nudge` | risveglio |

Meccaniche non ovvie: ritrasmissione fino a 5 volte; "blanking" dopo il rilascio dell'ultimo tasto
(report fantasma `0x01` alternati a vuoti) per evitare stuck-key sull'host; il byte 9 del report ha una
semantica da scoprire (`compute_byte9`: 0 su Mac-mode o a tasti premuti); supervisor
(`rf_link_supervisor`) che interroga lo stato ogni 2000 iterazioni e ri-asserisce il link comandato.

### 10.3 Integrazione

`kb.c` decide in base a `CONN_MODE_SWITCH` (P5.5; 1=USB, 0=RF) dove instradare i report. Pressione lunga
(`LINK_PAIRING_HOLD_TICKS`) di `LNK_BTx`/`LNK_24G` → pairing. Il **ritorno LED lock** dal radio
(`status_bytes[1] & 7`) alimenta `keyboard_state.led_state`.

> **Per il tuo port:** l'intera §10 va assunta come *ipotesi da validare* sulla B68. È lo stesso chip
> (BK3632) ma il *firmware del radio* e il cablaggio possono differire.

---

## 11. Persistenza (settings / NVM)

* [settings.c](../src/smk/settings.c): `user_settings_t` (9 byte: effetti LED, luminosità, velocità, colore,
  underglow, indicatore batteria, `rf_link`) caricata in `restore_settings()`; `settings_mark_dirty()` imposta
  un flag, `settings_task()` (main loop) chiama `settings_save()`, che a sua volta chiama `nvm_save`.
* [nvm/flash/nvm.c](../src/nvm/flash/nvm.c): **un record** per settore:
  `[0x5A 0xA5 len][payload][checksum]`. Il salvataggio è **no-op se il contenuto è identico**
  (`flash_matches`); altrimenti **erase dell'intero settore da 512 B** + riprogrammazione byte a byte.
  Capacità: `NVM_CAPACITY = 512 − 4 = 508` B.
* Su SH68F90 lo store sta in un settore di flash programma riservato (`FLASH_CFG_ADDR`); il linker è tenuto
  sotto (`--code-size`). **Non sopravvive a un riflash** (l'erase del bootloader cancella l'area applicativa).
* Variante `eeprom_24cxx`: I²C bit-bang su EEPROM esterna (ciy-x77).
* `settings_save_pre/post` sono hook di scheda (es. spegnere LED/PWM durante la scrittura).

`flash_read_into` lavora in `__critical` perché con `FLASHCON.FAC` impostato **ogni MOVC** (anche il fetch di
costanti `__code`) legge la data flash anziché il codice: un'ISR in quella finestra leggerebbe spazzatura.

---

### 11.x Keymap modificabile (`keymap_store.c`)

Solo per le schede con `keymap_sectors` (oggi la B68, 8 layer).

* **RAM**: copia di tutti i layer (`KEYMAP_DYNAMIC_LAYERS × righe × colonne × 2 B`; B68: 1280 B). `keymap_get()` legge da
  qui, perciò la scansione dei tasti non tocca la flash.
* **Flash**: due slot A/B da N settori. Formato di uno slot (versione 2): intestazione di 11 B (`'K' 'M'`, versione, layer, righe,
  colonne, sequenza a 16 bit, CRC-16/CCITT del payload, numero di encoder) seguita dal payload (parole a 16 bit little-endian: prima
  i tasti, riga per riga e layer per layer, poi gli encoder, `{antiorario, orario}` per encoder e layer).
* **Salvataggio** (`keymap_store_flush`): cancella lo slot **non** in uso, scrive il payload, scrive l'intestazione **per
  ultima**. Fino a quel momento lo slot non è valido; al boot vince lo slot valido con la sequenza più recente (confronto
  con segno, quindi sicuro al wrap). Una caduta di tensione a metà lascia la keymap precedente.
* **Quando si scrive** (`keymap_task`): dopo ~2000 scansioni senza modifiche e con nessun tasto premuto, mai durante la
  digitazione. `tick_sweeps()` è il contatore di scansioni (non è ancora un orologio in ms: vedi M4.19b).
* **Default**: se nessuno slot è valido si usa la keymap compilata (`keymap_default`), senza scrivere nulla in flash.
* Il riflash dell'applicazione cancella la keymap salvata (come le impostazioni).



### 11.y Protocollo VIA / Vial (`via.c`, `vial.c`, `timer.c`)

Solo per le schede con l'opzione `vial_json` (oggi la B68). Il livello del protocollo è completo e provato su host; **il
trasporto USB (Raw HID su EP2, milestone M4.1–M4.4) non c'è ancora**: finché non c'è, nessuno chiama `via_receive()`.

* `via_receive(data, 32)` riscrive la richiesta nel pacchetto di risposta, come `raw_hid_receive()` di QMK. Comandi
  implementati: protocollo (`0x01`), valori della tastiera (`0x02/0x03`: uptime, opzioni di layout, stato della matrice solo
  se sbloccata), `get/set_keycode` (`0x04/0x05`), reset (`0x06`), conteggio layer (`0x11`), `get/set_buffer` (`0x12/0x13`,
  keycode big-endian, layer per layer), macro vuote (`0x0C/0x0D`), `bootloader_jump` (`0x0B`, solo sbloccata). Tutto il resto
  risponde `0xFF` ("non gestito"), compresi i valori personalizzati (`0x07–0x09`) e il reset EEPROM (`0x0A`).
* `vial_receive()` gestisce il prefisso `0xFE`: identificatore e versione (6), dimensione e pagine da 32 B della definizione
  (`vial.json` minificato e compresso LZMA, come fa `vial_generate_definition.py` di vial-qmk), stato/avvio/poll dello
  sblocco, blocco, e le richieste "nessuna impostazione QMK / nessun tap dance, combo, override".
* **Sblocco**: si tengono premuti i tasti `vial_unlock_keys` per 50 passi da 100 ms. Un poll più ravvicinato di 100 ms azzera il
  conteggio (così fa vial-qmk). Mentre lo sblocco è in corso si risponde solo alle domande di sblocco; gli altri comandi vengono
  rimandati indietro invariati.
* **Dati di scheda**: `utils/vial_gen.py` produce, da `vial.json` e dalle opzioni meson `vial_uid` e `vial_unlock`, il file
  `<scheda>_vial_data.c` con definizione, identificatore e tasti di sblocco.
* **`timer.c`**: orologio in millisecondi. Conta le scansioni della matrice e le converte con la durata misurata di una
  scansione; con USB attivo la durata viene ri-misurata ogni 256 SOF (1 ms l'uno), perciò l'errore è sotto lo 0,5 %; senza
  USB mantiene l'ultima misura. Parte da 750 µs per scansione.

## 12. Indicatori / retroilluminazione

* Interfaccia: [user/indicators.h](../src/user/indicators.h) — `init`, `start`, `render`, `pre_update`,
  `update_step`, `post_update`, `pwm_enable/disable`, `apply_defaults`, `validate_settings`. I default sono
  vuoti: una scheda senza LED compila lo stesso.
* nuphy-air60 ([layouts/default/indicators.c](../src/keyboards/nuphy-air60/layouts/default/indicators.c),
  604 righe): RGB per-key 5×16, framebuffer `led_fb[5][3][16]` + `led_ul_fb[3][16]` (underglow); ogni sub-frame
  accende **una riga (+ un colore)** e imposta il duty PWM di ciascuna colonna (il LED conduce a livello
  basso: "non invertire", c'è un commento esplicito nel codice). Effetti: radiale, orizzontale, verticale, solido.
  Indicatore batteria sull'underglow destro; lampeggi di stato (pairing, ecc.).
* [led_effect.c](../src/smk/led_effect.c): ruota colore e lookup di geometria (`user_led_axis_x/y/radial`,
  generate).
* `indicators_pre_update/post_update` e `indicators_pwm_enable/disable` incorniciano lo scan di matrice
  (vedi [§7](#7-scheduling-timer2-e-tick)).

---

## 13. Sleep e power-down

[smk/sleep.c](../src/smk/sleep.c) (abilitato da `SLEEP_ENABLE`), [sh68f90/power.c](../src/sino51lib/sh68f90/power.c):

* `SLEEP_TIMEOUT = 21000` frame LED completi di inattività (conteggiati nel tick) → `sleep_requested`.
* `user_sleep_supported()` (scheda) dice la modalità: `USER_SLEEP_RF` (timeout di inattività) oppure
  `USER_SLEEP_USB` (segue `usb_suspended`).
* Sequenza: `tick_pause` → `indicators_pwm_disable` → `user_sleep_prepare()` (la scheda **parcheggia ogni
  pin**: righe/colonne/LED a livelli sicuri, e arma INT4) → `power_enter_powerdown()` (ferma PLL/oscillatore
  HF, `SUSLO=0x55; PCON|=PD`) → al risveglio `clock_wake_restart()`, `user_sleep_wake()`, `usb_init()`.
* Il commento nel sorgente è chiaro: la sequenza di `park_panel()` è **trascritta dal firmware stock** e *non è
  verificabile senza la tastiera fisica*. Sulla B68 questa parte va riderivata dal dump del firmware stock.

---

## 14. Il contratto di una scheda (board port interface)

Per aggiungere `yunzii-b68` servono (modello: [src/keyboards/nuphy-air60/](../src/keyboards/nuphy-air60), scheletro:
[src/keyboards/example/](../src/keyboards/example)):

| File | Cosa deve fornire |
| --- | --- |
| `meson.build` | voce `keyboards += {…}`: `platform:'sh68f90'`, `icp_part:'sh68f90a'`, `src`, `boot_src` (matrice + `user_init`), `layouts`, `options` (`vendor_id`, `product_id`, `wireless:'bk3632'`, `led_rows/cols/cx2/cy2`, stringhe USB) |
| `kbdef.h` | `MATRIX_ROWS/COLS`; macro dei pin (righe, colonne, bit `_Pn_m`); registri PWM per colonna; pin LED; switch; pin SPI radio; `enum custom_keycodes` che parte da `SAFE_RANGE` e termina con `KB_SAFE_RANGE` |
| `user_init.c` | `user_init()`: direzione GPIO, pull-up, drive strength (`DRVCON`/`PxDRV`), init PWM; esporta `user_gpio_init()` (richiamata al wake) |
| `user_matrix.c` | `user_matrix_cols_deselect_all/col_select/col_deselect/read_rows/scan_pre/scan_post/sinks_off` — **usate anche dal bootloader** (`boot_src`) |
| `user_sleep.c` | `user_sleep_supported/prepare/wake` |
| `kb.c` | `kb_init`, `kb_update_switches`, `kb_update`, `kb_process_record`, `kb_send_report/nkro/extra` (routing USB↔RF) |
| `layouts/<n>/layout.c` | `keymaps[][ROWS][COLS]`, `layout_os_base_layer()`, `layout_process_record()` |
| `layouts/<n>/indicators.c` | implementazione LED (se la scheda ha LED) |

Vincoli:

* **`boot_src` dovrebbe essere minimale**: il bootloader ha solo ~67 B liberi ([§17](#17-budget-di-risorse-misurato)),
  quindi `user_init` per il bootloader deve restare piccola (nuphy riusa lo stesso `user_init.c`, che include il setup PWM).
* La **catena `boot_chord`** (default `S,M,K`) viene risolta *a build time* dal `layout.c` della scheda:
  `boot_chord_gen.py` deve riuscire a parsare la tua macro `LAYOUT_xx` (formato compatibile con
  `LAYOUT_60` di nuphy) — un tasto assente fa fallire la build, non genera un bootloader irraggiungibile.
* `MATRIX_COLS` e `MATRIX_ROWS` finiscono nelle `_Static_assert` del generatore di geometria LED.

---

## 15. Bootloader ISP di SMK

Documento upstream: [bootloader.md](bootloader.md). Qui il design lato codice.

### 15.1 Obiettivo

Sostituire il bootloader ISP stock (4 KiB in cima alla flash) con uno che:

1. parla **lo stesso protocollo HID** (VID:PID `0603:1020`, feature report 5 = comandi, 6 = pagine) →
   `sinowisp` lo pilota senza modifiche;
2. **non può essere "brickato" dall'applicazione**: si entra in ISP tenendo premuto un chord (default
   **S+M+K**) all'accensione, oppure su richiesta dell'app, oppure se non c'è un'app valida;
3. enumera correttamente (endpoint dichiarato, una sola collection, stringhe, bcdUSB 2.00).

### 15.2 Layout flash e vettori

Il reset vector è a `0x0000`, quindi l'app **non può possederlo**:

| Indirizzo | Contenuto |
| --- | --- |
| `0x0000–0x0002` | `LJMP <bootloader>`; scritto dal comando di erase, *saltato* da ogni scrittura di pagina |
| `0x0003…` | vettori interrupt e codice dell'applicazione |
| `firmware_size − 5` (`0xEFFB`) | `LJMP <entry app>`: nel settore "marker"; la sua presenza (`0x02`) = "app avviabile" |
| `0xF000…` | bootloader |
| `0xFF00` (`ISP_ENTRY`) | stub di ingresso, linkato in un segmento dedicato `BOOTISP` |

Il vettore reale dell'app viene spostato a `0xEFFB` durante la scrittura; le letture di `0x0001-0x0002` sono
rimappate lì così un dump rilegge byte-identico (è il formato "JTAG payload" di sinowisp). Il vettore viene
**committato solo dal comando `ENABLE_FIRMWARE` (0x55)**, dopo che sinowisp ha riletto e verificato tutto.

Due indirizzi sono un'**ABI** fra app e bootloader (linkati separatamente): `APP_VECTOR_ADDR` e `ISP_ENTRY`;
[boot_check.py](../utils/boot_check.py) li verifica contro la mappa del linker e **rompe la build** se si spostano.

### 15.3 Flusso di `main()` ([boot/main.c](../src/boot/main.c))

```
sfr_page_0(); FLASHCON = 0;           (l'app può saltare qui con pagina SFR / finestra flash arbitrarie)
watchdog_kick();
requested = boot_isp_requested();     (magic XRAM = richiesta dall'app, poi lo azzera)
ldo_init; clock_init; peripherals_init; user_init();   (riuso del codice scheda)
IEN0 = IEN1 = 0;                      (nessuna ISR: il bootloader fa polling)
if (!requested && !chord_held() && boot_app_enabled())  boot_enter_app();   ← LJMP APP_VECTOR_ADDR
isp_init(); boot_usb_start();
for(;;){ watchdog_kick(); boot_usb_poll(); isp_task(); }
```

`chord_held()` = 6 campioni a 10 ms (≈60 ms) con scan della matrice riusando le stesse `user_matrix_*` della
scheda. Costo a freddo ≈60 ms.

### 15.4 Protocollo ISP (comandi in `isp.c`)

`0x55` ENABLE_FIRMWARE (commit del vettore app), `0x52` INIT_READ, `0x57` INIT_WRITE, `0x45` ERASE,
`0x5A` REBOOT, `0x75` ISP_MODE; trasferimenti pagina `0x72`/`0x77` (pagina da **2048 B**, `ISP_PAGE_SIZE`).

Scelte di design:

* **Streaming, niente page buffer**: la pagina da 2049 B passa a pacchetti da 8 B sul control pipe EP0 e
  ogni byte è programmato subito in flash.
* **Erase e reboot differiti** al main loop (erase dell'area app > timeout dello status stage).
* Il bootloader **non scrive nella propria regione** (`addr >= BOOT_BASE` ignorato) — da qui la necessità di ICP
  per il primo install.
* `boot_stock_scramble` (solo ciy-x77): offset sui byte di pagina (−0x5A in scrittura, +0x5E in lettura dopo i
  primi 6) per restare compatibile con tool/preset esistenti.

### 15.5 Build del bootloader

Un'immagine **per scheda** (non per layout): `-DFLASH_FW_SIZE`, `--code-loc 0xF000`, `--code-size 0xF00`, link con
`-Wl-bBOOTISP=0xFF00`. Output: `<board>_boot.hex`. L'immagine completa `…_full.hex` è fusa da
[boot_image.py](../utils/boot_image.py). Il **primo** install richiede ICP (`*_full_flash` via sinodude); poi
`sinowisp write` aggiorna la sola app.

### 15.6 Budget

Misurato: **3773 / 3840 B** di codice, **96 B** di XRAM, **174 B** di stack. Margine di codice ≈ **67 B**:
praticamente nessuno. Qualsiasi modifica al bootloader (anche `user_init` più grande) va misurata.

---

## 16. Test e simulatore

* [flake.nix](../flake.nix) compila **uCsim** (simulatore 8051 di SDCC) con una patch che modella il
  SH68F90 (vettore USB `0x3B`, SIE USB abbastanza fedele da enumerare). Eseguibile `ucsim-sh68f90`.
* [tests/](../tests): `sim.py` (harness: carica `.hex`, breakpoint a livello sorgente via `.cdb`),
  `test_usb*.py` (enumerazione, EP0), `test_boot*.py` (bootloader e chord a livello di pin), `test_stack.py`
  (profondità stack), `test_source_breakpoints.py`.
* Registrati in `meson test` come `sim-tests`. Limite noto: il SIE simulato smette di completare IN EP0 dopo 16
  pacchetti.
* CI: [.github/workflows/build.yml](../.github/workflows/build.yml) — matrice `nkro × buildtype` + simulatore.

Il simulatore **non** modella radio, matrice reale né flash timing. Per Raw HID/VIAL i test su sim servono per la
logica USB/protocollo; il resto va su hardware.

---

## 17. Budget di risorse (misurato)

Build `release`, `nuphy-air60_default`, SDCC 4.5.0:

| Risorsa | Usato | Disponibile | Residuo |
| --- | --- | --- | --- |
| Flash applicazione | 21 415 B | 60 416 B (fino a `0xEC00`) | **≈ 39 000 B** |
| XRAM applicazione | 1 053 B | 4 096 B | ≈ 3 040 B |
| Stack IRAM (app) | — | 222 B | (stack parte da `0x22`) |
| IRAM "data" libera (app) | — | 22 B dal `0x0A` | 22 B |
| Bootloader codice | 3 773 B | 3 840 B | **≈ 67 B** |
| Bootloader XRAM | 96 B | 4 096 B | ampio |
| Settings | 9 B | 508 B | 499 B |

Sono numeri della build *release*; il debug (console HID + `dprintf`) pesa di più. Tienine conto quando misuri
l'impatto delle tue funzionalità: confronta sempre con lo stesso `buildtype`.

---

## 18. Osservazioni rilevanti per il lavoro che ti aspetta

Non sono bug confermati: sono i punti in cui il design attuale **non è pronto** per VIAL o per una scheda
nuova, ricavati leggendo il codice. Il piano ([porting-plan.md](porting-plan.md)) li trasforma in task.

1. **Endpoint insufficienti per un Raw HID "puro".** VIAL-GUI parla con un'interfaccia HID *senza Report ID*,
   usage page `0xFF60` / usage `0x61`, report da **32 B** in/out. L'MCU ha **3 endpoint** (EP0, EP1=16 B,
   EP2=64 B). EP1 è troppo piccolo per 32 B; EP2 oggi ospita extra+NKRO+console con Report ID, che sono
   incompatibili con un'interfaccia senza ID. Serve una **ridistribuzione degli endpoint** (vedi piano, decisione
   D1).
2. ~~Il motore dei layer è minimale~~ **Risolto (M4.17)**: `src/smk/layer.c` gestisce `MO/TO/TG/DF/OSL/OSM`, fino a
   8 layer. Restano `LT/MT` (serve la timebase al ms, M4.19b) e `LM`/`TT`. Vial può assegnare solo ciò che il
   motore esegue; gli altri keycode vanno rifiutati o ignorati (M4.19).
3. ~~**Possibile stuck-key sul cambio layer**~~ **Risolto (M4.17)**: il keycode risolto alla pressione viene ricordato per posizione (`down_keycode`, ROWS×COLS×2 B) e usato al rilascio; `clear_keys()` al rilascio di `MO` non serve più. Descrizione precedente: il rilascio di un tasto risolve il keycode con
   `action_layer` *corrente*; se rilasci `Fn` prima del tasto, il keycode risolto in rilascio è quello base.
   `clear_keys()` al rilascio di `MO` mitiga per i tasti normali; con più layer/`LT`/`MT` serve una cache
   "keycode premuto per posizione".
4. **Keymap in flash codice (`const`)**: va resa modificabile (cache in XRAM + persistenza in flash). Costo
   RAM: `ROWS×COLS×2 B` per layer (nuphy: 160 B/layer).
5. **Un solo settore di settings, salvataggio "eager"**: `settings_task` salva a ogni `dirty`, erase da 512 B.
   Con VIAL (un tasto cambiato = una scrittura) servono **write-coalescing** (debounce di qualche secondo +
   flush in idle/USB suspend) e più settori. L'endurance di questa flash va letta sul datasheet (da trovare).
6. **Erase flash e tempo reale**: durante erase/program SSP la CPU è ferma; nessuna misura nel repo di
   quanto duri un erase di settore. Va misurato (task dedicato) perché blocca scan, USB e SPI radio.
7. **`isp_jump()` e preset `sinowisp`**: l'ISP feature report è sull'interfaccia 1. Spostare le interfacce
   cambia il flusso `sinowisp write` per una scheda nuova (serve un preset `-d` o i flag espliciti).
8. **Custom keycode**: SMK parte da `SAFE_RANGE = QK_USER (0x7E40)`; VIAL espone i keycode custom da
   `QK_KB_0 (0x7E00)`. Per mostrarli nella GUI vanno rimappati.
9. **`matrix_col_t` è `uint8_t`**: massimo 8 righe. La B68 (65%) ne ha verosimilmente ≤ 6, ma verificalo sul
   PCB.
10. **Bootloader senza margine** (≈67 B): evita di toccarlo salvo necessità; se la B68 richiede un
    `user_init` più grande nel `boot_src`, potrebbe non starci.
11. **Radio**: il protocollo `0xAA…` è NuPhy-specifico per ricostruzione; vedi rischio R1 nel piano.
