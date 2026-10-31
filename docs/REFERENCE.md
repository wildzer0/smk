# REFERENCE — Materiale di studio

> Tutto ciò che serve per capire l'architettura 8051, i chip della B68 (SinoWealth SH68F90A / BYK916 e
> Beken BK3632), USB HID, il protocollo VIA/Vial e il reverse engineering del firmware stock. Organizzato
> **per argomento** e con un **percorso di studio ordinato** (§1). Ogni voce indica *perché* serve e *dove*
> la ritrovi nel codice di SMK.
>
> Documenti collegati: [architecture.md](architecture.md) · [porting-plan.md](porting-plan.md) · [usb-hid.md](usb-hid.md) · [recovery-icp.md](recovery-icp.md).

**Legenda dello stato dei link** (onestà sulla verifica):

| Simbolo | Significato |
| --- | --- |
| ✔ | Aperto e letto in questa sessione: il contenuto corrisponde alla descrizione |
| ◐ | Comparso nei risultati di ricerca ma **non aperto**: controlla prima di fidarti |
| ○ | URL non verificato (indirizzo noto di norma, ma non controllato): se non si apre, cerca per titolo |
| ✘ | **Non sono riuscito a trovarlo** pubblicamente: devi procurartelo (vedi §3 e §4) |
| 📁 | Si trova già nel repository |

---

## 1. Percorso di studio consigliato

Ordine pensato per metterti in condizione di scrivere codice nelle milestone ([porting-plan.md](porting-plan.md)).
I tempi sono indicativi per chi ha già esperienza di C embedded.

| Passo | Argomento | Risorse (sezione) | Serve per | Tempo |
| --- | --- | --- | --- | --- |
| 1 | Architettura 8051: memorie (DATA/IDATA/XDATA/CODE), SFR, bit-addressable, stack, interrupt, registri banchi | §2.1 | tutto; leggere il `.mem` della build | 1–2 giorni |
| 2 | SDCC per MCS-51: memory model, `__at`, `__critical`, `__interrupt`, `--stack-auto`, reentrancy, overlay | §2.2 | capire `meson.build`, `check_interrupts.py` | 1 giorno |
| 3 | Il MCU: SH68F90A (SFR, PWM, USB, SSP, power-down, code option) leggendo `sh68f90.h` e i driver | §3.1, 📁 `src/sino51lib/sh68f90/` | M2, M3, M5 | 1–2 giorni |
| 4 | USB device: enumerazione, control transfer, endpoint, HID class, report descriptor | §5 | M4 (trasporto) | 2–3 giorni |
| 5 | Il progetto: leggere [architecture.md](architecture.md) con il codice aperto; seguire un tasto dalla matrice all'host | 📁 §6 | tutto | 1 giorno |
| 6 | VIA/Vial: protocollo, keycode, definizione `vial.json`, unlock | §4 | M4 (protocollo) | 2 giorni |
| 7 | Strumenti di laboratorio: ICP/ISP, analizzatore logico, disassemblaggio MCS-51 | §7, §8 | M0, M1 | 1–2 giorni |
| 8 | Il radio: BK3632 e il protocollo SMK/NuPhy | §3.2, 📁 `rf_controller.c` | M1.5, M6 | variabile |

---

## 2. Architettura 8051 e toolchain

### 2.1 Core 8051 / MCS-51

| Risorsa | Perché | Stato |
| --- | --- | --- |
| **MCS-51 Microcontroller Family User's Manual** (Intel, order no. 272383, feb 1994) — la bibbia: architettura, set di istruzioni, timer, seriale, interrupt, power-down | Riferimento canonico dell'ISA e dei blocchi standard. Mirror: [bitsavers](https://ftp.mirrorservice.org/sites/www.bitsavers.org/components/intel/8051/MCS-51_Users_Manual_Feb94.pdf), [MIT 6.115](https://ee6115b.mit.edu/document/8051.pdf) | ◐ |
| Altre copie dello stesso manuale: [NTHU OCW (versione ridotta)](https://ocw.nthu.edu.tw/ocw/upload/news_attach/692/Intel%20MCS51%20Microcontroller%20Family%20User_s%20Guide(short)%20(1).pdf), [CU Boulder](https://cos.colorado.edu/Documents/DCE/BOOT/8051_Manual.pdf), [plit.de (HTML)](https://plit.de/asem-51/mcs51man.htm) | Alternativa se un mirror non risponde | ◐ |
| Catalogo Intel delle pubblicazioni 8051 su [bitsavers](https://ftp.mirrorservice.org/sites/www.bitsavers.org/components/intel/8051/) | Per cercare datasheet e note applicative Intel | ◐ |

**Cosa estrarre dal manuale** (e dove lo vedi nel progetto):

* Spazi di indirizzamento (DATA 0x00–0x7F, IDATA 0x80–0xFF, SFR 0x80–0xFF diretti, XDATA, CODE) → [`.mem` della build](architecture.md#17-budget-di-risorse-misurato), `__xdata`/`__code`/`__data` nel sorgente.
* Registri banchi R0–R7, PSW, bit-addressable (0x20–0x2F) → perché SDCC dichiara `B` e `T` nella mappa IRAM.
* Stack in IRAM, `SP` → [test_stack.py](../tests/test_stack.py).
* Interrupt (vettori a 0x03, 0x0B, …, priorità, `IE`, `IP`, `RETI`) → [interrupts.h](../src/sino51lib/sh68f90/interrupts.h), perché `isp_jump()` non può partire da ISR.
* Timer 0/1/2 (modi, reload) → [systick.c](../src/sino51lib/sh68f90/systick.c) (Timer2 con reload automatico `RCAP2`).
* Istruzioni `MOVX`, `MOVC`, `LJMP`, `DJNZ` → [startup.c](../src/sino51lib/startup.c), [boot/isp_entry.c](../src/boot/isp_entry.c), [delay.c](../src/sino51lib/delay.c).
* **Core 1T** (qui) vs 12T classico: i cicli macchina del manuale vanno divisi per il fattore del chip; [delay.c](../src/sino51lib/delay.c) spiega il conteggio per 1 µs a 24 MHz.

Risorse didattiche supplementari (da usare *dopo* il manuale):

* Qualunque corso universitario 8051 che usi SDCC o Keil (il MIT 6.115 sopra è uno): utile per i timer e la seriale.

### 2.2 SDCC (Small Device C Compiler)

| Risorsa | Perché | Stato |
| --- | --- | --- |
| [SDCC User Manual (PDF)](https://sdcc.sourceforge.net/doc/sdccman.pdf) — sezioni: *MCS51 Memory Models* (3.15.1), *MCS51/DS390 Interrupt Service Routines* (3.8.2), *Critical Functions and Critical Statements* (3.9.1), *Functions using private register banks* (3.10), *MCS51/DS390 intrinsic named address spaces* (3.5.1), *Absolute Addressing* (3.5.10), *Common interrupt pitfall: use of non-reentrant functions* (3.8.1.4) | Regole di `--model-large`, `--stack-auto`, `__interrupt`, `__critical`, `__at`, overlay | ✔ (indice verificato) |
| SDCC `sdas`/`sdld`, formato `.ihx`/`.rel`/`.map`/`.mem` | Leggere `*.map` e `*.mem` della build per capire dove finisce il codice | ✔ vedi `build/*.map` e `.mem` |
| 📁 [utils/sdcc_compile.py](../utils/sdcc_compile.py), [utils/check_interrupts.py](../utils/check_interrupts.py) | Come il progetto aggira i limiti di SDCC (dipendenze, overlay) | 📁 |

Letture mirate per te: *memory models* → *interrupts* → *reentrant functions* → *named address spaces*.

---

## 3. I chip della B68

### 3.1 SinoWealth SH68F90A (marcato BYK916)

> **Situazione onesta:** il datasheet ufficiale del SH68F90A **non è liberamente scaricabile da una ricerca
> pubblica**: le mie ricerche sul web non l'hanno trovato (solo datasheet di MCU SinoWealth "cugini", che non
> valgono per questo chip). Le informazioni disponibili pubblicamente passano dal reverse engineering della community
> e dai header SFR dentro questo repo.

**Dove ottenerlo / come procedere** (in ordine):

1. **Il repository stesso** 📁 — la fonte più affidabile che hai:
   * [src/sino51lib/sh68f90/sh68f90.h](../src/sino51lib/sh68f90/sh68f90.h) (856 righe): SFR, bit, buffer USB, vettori.
   * [src/sino51lib/sh68f90/](../src/sino51lib/sh68f90) driver: `clock.c`, `usbhw.c`, `power.c`, `systick.c`, `extint.c`.
   * [docs/platforms/sh68f90.md](platforms/sh68f90.md), [docs/keyboards/nuphy-air60.md](keyboards/nuphy-air60.md) (code option decodificate).
   * [src/sino51lib/examples/](../src/sino51lib/examples) — firmware minimi che esercitano ogni blocco (GPIO, UART, SPI, PWM, USB, flash info, power-down): sono i **tuoi "use case"** (vedi §6).
2. **Sito del produttore**: SinoWealth (中颖电子) — cerca la pagina prodotto per "SH68F90A" o per la famiglia "SH68F" (MCU con USB). Dal fetch della home non ho potuto confermare l'URL né trovare link ai datasheet: ○ [sinowealth.com](http://www.sinowealth.com/).
3. **Distributori** (LCSC, Digikey cinesi, forum): cerca "SH68F90A datasheet" / "SH68F90" / "SH68F89" (parte parente, già supportata da SMK). ◐ I datasheet degli MCU SinoWealth della serie SH79F su [LCSC](https://www.lcsc.com/product-detail/C182000.html) esistono ma sono per **altre** famiglie.
4. **Community**: il Discord di SMK (badge nel README) e le issue dei repository `karstx/sinowisp` e `karstx/sinodude` — gli autori hanno analizzato code option e bootloader.
5. **Tool SinoWealth**: *SinoLink* (programmatore/debugger) e *ProWriter* (software Windows) citati nel README di SMK; la documentazione del pacchetto di tool a volte include il manuale dei registri.

**Cosa devi trovare nel datasheet** (checklist per M0/M1/M4/M6):

* Mappa registri SFR/XFR e pin multiplexing (porte P0–P7, PWM, SPI, UART, INT).
* **Code option** (byte 0–7): significato bit per bit (la tabella decodificata della Air60 è un buon punto di partenza).
* **USB**: endpoint, dimensioni buffer, ping-pong "COM", registri `EPxCON`/`IEPxCNT`/`OEPxCNT`, interrupt per direzione (per **S1** del piano).
* **Flash**: dimensione settore, **endurance (cicli di erase/program)**, tempi di erase/program, regole SSP (per **S2/S4** e rischio R4).
* **Power-down**: consumi, sorgenti di wake (INT4, USB), registro `SUSLO`.
* **ICP/ISP**: pin di programmazione, protocollo ICP, come si entra in ISP (code option `OP_ISP`, `OP_ISPPIN`).

### 3.2 Beken BK3632 (radio BT + 2.4G)

> **Situazione onesta:** non ho trovato un datasheet pubblico di "BK3632". Anche qui il protocollo tra MCU e radio
> **non è il pin-out del chip**: il BK3632 nelle tastiere esegue un **firmware dedicato**; l'MCU lo comanda con frame
> `0xAA …` (vedi [architecture.md §10](architecture.md#10-wireless-bk3632)). La conoscenza utile è quindi *il protocollo
> del firmware del radio*, non il datasheet del silicio.

| Risorsa | Perché | Stato |
| --- | --- | --- |
| 📁 [src/peripherals/bk3632/rf_controller.c](../src/peripherals/bk3632/rf_controller.c), [bb_spi.c](../src/peripherals/bk3632/bb_spi.c) | Implementazione del protocollo ricavata dal firmware NuPhy | 📁 |
| 📁 [docs/keyboards/nuphy-air60.md](keyboards/nuphy-air60.md) | Descrittori HID stock lato USB e lato dongle wireless | 📁 |
| Famiglia Beken (parenti): [BK3231 datasheet (21ic)](https://dl.21ic.com/download/bk3231_datasheet-381819.html), [BK3431 datasheet (PDF)](https://git.nefarius.at/nefarius/USB-Bluetooth-Specs/raw/branch/master/BEKEN%20BK3431.pdf) | Serie correlate (HID BT, BLE): solo per capire concetti (HID over BT, pairing); **non** descrivono il BK3632 | ◐ |
| [Beken Corporation (sito ufficiale)](https://www.bekencorp.com) | Punto di partenza per prodotti e contatti | ○ |
| Cattura SPI col firmware stock (task **M1.5**) | È **la** fonte primaria per la B68 | — |

Concetti da studiare per capire cosa fa il radio: Bluetooth HID over GATT (HOGP), pairing/bonding, dongle 2.4G
proprietario con HID report, "stato" batteria/connessione. Non servono per scrivere codice SMK, ma aiutano a
interpretare le catture.

### 3.3 Yunzii B68 (e parenti già noti)

* [karstx/sinowisp](https://github.com/karstx/sinowisp) ✔ — README elenca **Yunzii AL66 e AL71** con SH68F90A (la B68 non appare nell'elenco).
* Famiglia hardware identica alla NuPhy Air60 (BYK916 + BK3632): 📁 [docs/keyboards/nuphy-air60.md](keyboards/nuphy-air60.md).
* Foto/teardown del PCB della B68: ✘ non trovati; vanno prodotti da te (task M0.4/M1.1).

---

## 4. VIA / Vial

> **Come funziona in breve.** La GUI (Vial) apre l'interfaccia HID *raw* (usage page `0xFF60`, usage `0x61`,
> report da **32 B**) e scambia comandi. Il firmware risponde con: ID/versione, la **definizione del layout**
> (JSON compresso, a pagine da 32 B), le **keymap dinamiche** per layer e, se abilitato, macro, tap dance, ecc.
> Le operazioni sensibili richiedono **unlock** (tenere premuti dei tasti). I valori numerici sotto sono quelli
> letti dai sorgenti `vial-qmk` in questa sessione.

### 4.1 Documentazione ufficiale

| Risorsa | Perché | Stato |
| --- | --- | --- |
| [Porting to Vial](https://get.vial.today/docs/porting-to-vial.html) — keymap `vial`, `VIA_ENABLE`/`VIAL_ENABLE`, `vial.json`, `VIAL_KEYBOARD_UID`, unlock combo (`VIAL_UNLOCK_COMBO_ROWS/COLS`, o `VIAL_INSECURE`) | Il percorso "tipico" QMK→Vial: per SMK lo **reimplementiamo a mano** ma la struttura (UID, json, unlock) è la stessa | ✔ |
| [Security](https://get.vial.today/docs/security.html) · [Firmware size](https://get.vial.today/docs/firmware-size.html) · [Manuale utente](https://get.vial.today/manual/) · [Porting to VIA (JSON)](https://get.vial.today/docs/porting-to-via.html) | Modello di sicurezza (unlock), consumo di flash delle feature, uso della GUI, formato JSON del layout | ◐ (link letti dalla pagina, non aperti) |
| [QMK Raw HID](https://docs.qmk.fm/features/rawhid) — default usage page `0xFF60` / usage `0x61`, **report sempre da 32 B** in entrambe le direzioni, API `raw_hid_receive` / `raw_hid_send` | È **il canale fisico** di Vial/VIA | ✔ |
| [Can I use VIA — Configuring QMK](https://caniusevia.com/docs/configuring_qmk/) | Cosa abilita VIA (keymap dinamica, raw HID, bootmagic lite) lato QMK | ◐ |

### 4.2 Sorgenti da leggere (sono la specifica de facto)

| Risorsa | Cosa contiene | Stato |
| --- | --- | --- |
| [vial-kb/vial-qmk](https://github.com/vial-kb/vial-qmk) (branch `vial`) | Il firmware di riferimento | ✔ |
| [`quantum/via.h`](https://github.com/vial-kb/vial-qmk/blob/vial/quantum/via.h) | `VIA_PROTOCOL_VERSION = 0x0009`; ID dei comandi: `get_protocol_version=0x01`, `get_keyboard_value=0x02`, `set_keyboard_value=0x03`, `dynamic_keymap_get_keycode=0x04`, `set_keycode=0x05`, `dynamic_keymap_reset=0x06`, `custom_set/get/save=0x07/08/09`, `eeprom_reset=0x0A`, `bootloader_jump=0x0B`, `macro_get_count/buffer_size/get_buffer/set_buffer/reset=0x0C…0x10`, `dynamic_keymap_get_layer_count=0x11`, `get_buffer=0x12`, `set_buffer=0x13`, **`id_vial_prefix=0xFE`**, `id_unhandled=0xFF` | ✔ |
| [`quantum/vial.h`](https://github.com/vial-kb/vial-qmk/blob/vial/quantum/vial.h) | `VIAL_PROTOCOL_VERSION = 6`; sotto-comandi Vial (dopo il prefisso `0xFE`): `get_keyboard_id=0x00`, `get_size=0x01`, `get_def=0x02`, `get_encoder=0x03`, `set_encoder=0x04`, `get_unlock_status=0x05`, `unlock_start=0x06`, `unlock_poll=0x07`, `lock=0x08`, `qmk_settings_query/get/set/reset=0x09…0x0C`, `dynamic_entry_op=0x0D` | ✔ |
| [`quantum/vial.c`](https://github.com/vial-kb/vial-qmk/blob/vial/quantum/vial.c) | Formati di risposta: `get_keyboard_id` = versione protocollo (u32 LE) + UID 8 B (+ flag VialRGB al byte 12); `get_size` = u32 LE; `get_def` = pagina (u16 in byte 2–3) da 32 B; unlock: contatore da 50, tick di ~100 ms, combo di righe/colonne restituita dallo status; pacchetti di lunghezza ≠ `VIAL_RAW_EPSIZE` ignorati | ✔ |
| `quantum/via.c`, `quantum/dynamic_keymap.c`, `quantum/vial.c` (resto) | Dispatch completo, layout in EEPROM, endianness del buffer, macro | ○ (path attesi, da leggere interamente in **S3**) |
| [vial-kb/vial-gui](https://github.com/vial-kb/vial-gui) | La GUI: come rileva il dispositivo, sequenza di connessione, **versione keycode (`keycodes_v6`?)**, decompressione della definizione. Il README non descrive il rilevamento: serve leggere `src/` | ✔ (repo) · ○ (dettagli) |
| [the-via/keyboards](https://github.com/the-via/keyboards/tree/master/src) | Esempi di JSON di layout VIA | ◐ |
| `vial-qmk/keyboards/vial_example` | Esempi di configurazione per più MCU | ◐ (link letto) |

### 4.3 Concetti da padroneggiare

* **Pacchetto**: 32 B, `msg[0]` = comando (`0xFE` = prefisso Vial, `msg[1]` = sotto-comando), risposta scritta nello stesso buffer.
* **Keycode**: formato QMK a 16 bit (`QK_*` in 📁 [keycodes.h](../src/smk/keycodes.h)); la GUI e il firmware devono usare la **stessa tabella** (verificare in S3 la versione).
* **Layout**: `vial.json` (nome, VID/PID, matrice righe×colonne, `layouts.keymap` in stile KLE con coordinate `riga,colonna`, `customKeycodes`, eventuali menu). Il firmware lo conserva **compresso** e lo serve a pagine; è la GUI a decomprimere (formato esatto: ○ da leggere in vial-qmk/vial-gui, S3).
* **Unlock**: stato bloccato di default; combo di tasti per N secondi; protegge `bootloader_jump`, reset EEPROM e simili.
* **Persistenza**: in QMK la keymap vive in EEPROM; su SMK diventa una regione di flash ([porting-plan D2](porting-plan.md#d2--dove-e-come-salvare-la-keymap)).

---

## 5. USB e HID

| Risorsa | Perché | Stato |
| --- | --- | --- |
| [Pagina HID di usb.org](https://usb.org/hid) — *Device Class Definition for HID 1.11*, *HID Usage Tables* | La specifica del report descriptor, SET/GET_REPORT, boot protocol, idle | ◐ |
| [USB HID Usage Tables 1.12 (PDF)](https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf) | Tabelle degli usage (tastiera 0x07, consumer 0x0C, generic desktop 0x01); **già citata nei commenti** di 📁 [report.h](../src/smk/report.h) | 📁 (link nel codice) |
| USB 2.0 Specification (usb.org → Document Library), cap. 5 (modelli di trasferimento), 8 (protocollo), 9 (device framework, descrittori, richieste standard) | Enumerazione, control transfer a 3 fasi, DATA toggle, endpoint | ○ |
| 📁 [src/smk/usb.c](../src/smk/usb.c), [usbdef.h](../src/sino51lib/usbdef.h), [usbhw.h](../src/sino51lib/usbhw.h) | Implementazione di riferimento *per questo chip* | 📁 |
| 📁 [src/sino51lib/examples/usb_minimal/](../src/sino51lib/examples/usb_minimal) | SIE minimale: enumerazione + EP1/EP2 IN — base per lo **spike S1** | 📁 |
| 📁 [tests/test_usb.py](../tests/test_usb.py), [test_usb_ep0.py](../tests/test_usb_ep0.py) | Come si testa l'enumerazione sul simulatore | 📁 |
| 📁 [src/smk/usbhidreport.h](../src/smk/usbhidreport.h) | Macro `HID_RI_*` (da LUFA) per scrivere report descriptor | 📁 |
| QMK `tmk_core/protocol/` (descrittori `KEYBOARD_SHARED_EP`, boot vs report protocol) | Come QMK fa **un solo endpoint** per tastiera+extra+mouse, rilevante per D1 | ◐ |

**Strumenti per ispezionare:**

* Linux: `lsusb -v -d VID:PID`, `usbhid-dump`, `/dev/hidraw*` (serve regola udev), `hid-tools` / `hid-recorder` ○.
* Windows: USBPcap + Wireshark ○; USBView / Device Manager.
* macOS: *System Information → USB*; `ioreg -p IOUSB -l` ○.
* Python: `hidapi` (modulo `hid`) per script di test di Raw HID (S1) — [libusb/hidapi](https://github.com/libusb/hidapi) ○.

---

## 6. Il codice di SMK come materiale di studio (use cases)

SMK stesso è la miglior raccolta di casi d'uso funzionanti per questo MCU. Leggi in quest'ordine:

| Per capire… | Leggi |
| --- | --- |
| GPIO, delay, watchdog, init clock | `examples/gpio_blink/main.c`, [delay.c](../src/sino51lib/delay.c), [gpio.h](../src/sino51lib/gpio.h) |
| Timer2 come tick | `examples/systick_tick/`, [systick.c](../src/sino51lib/sh68f90/systick.c) |
| PWM | `examples/pwm_duty/`, [pwm.h](../src/sino51lib/pwm.h) |
| USB minimo | `examples/usb_minimal/` |
| Flash info / SSP | `examples/flash_info/`, [flash.c](../src/sino51lib/flash.c) |
| Power-down e wake | `examples/isp_powerdown/` (⚠ build-only), [power.c](../src/sino51lib/sh68f90/power.c) |
| UART di debug | `examples/uart_hello/` |
| SPI bit-bang | [bb_spi.c](../src/peripherals/bk3632/bb_spi.c) |
| Una scheda completa | [src/keyboards/nuphy-air60/](../src/keyboards/nuphy-air60) |
| Bootloader | [src/boot/](../src/boot), [docs/bootloader.md](bootloader.md) |
| Console di debug via HID | [tools/smk-console](../tools/smk-console), [console.c](../src/smk/console.c) |

Schede esistenti da confrontare (per capire la variabilità fra MCU/layout): [ciy-x77](keyboards/ciy-x77.md),
[eyooso-z11](keyboards/eyooso-z11.md), [genesis-thor-300](keyboards/genesis-thor-300.md),
[nuphy-air60](keyboards/nuphy-air60.md); piattaforme: [sh68f90](platforms/sh68f90.md),
[sh68f89](platforms/sh68f89.md), [sh68f881](platforms/sh68f881.md).

---

## 7. Programmazione e recupero del chip (ICP / ISP)

| Risorsa | Perché | Stato |
| --- | --- | --- |
| [karstx/sinowisp](https://github.com/karstx/sinowisp) — legge/scrive la flash via **ISP bootloader** (HID); elenco MCU e dispositivi supportati; note: legge/scrive con LJMP a `<firmware_size-5>` e rimappa `0x0001-0x0002` | Tool che userai per `…_flash` e per il dump parziale | ✔ |
| [karstx/sinodude](https://github.com/karstx/sinodude) — lettura/scrittura via **ICP**; programmatori: *sinodude-serial* (Arduino Nano ATmega328P/PB, "recommended") e *SinoLink*. Link nel repo: `firmware/README.md` (sinodude-serial), `docs/sinolink.md` | Tool per dump completo e primo install del bootloader. Il pinout ICP del SH68F90A **non** è nel README principale: cercalo in quei due documenti | ✔ (README) · ○ (sotto-doc) |
| 📁 [docs/bootloader.md](bootloader.md) | Layout flash, entry, comandi ISP | 📁 |
| 📁 [utils/boot_image.py](../utils/boot_image.py), [utils/boot_check.py](../utils/boot_check.py) | Come si compone e controlla l'immagine completa | 📁 |

Concetti: **ICP** (In-Circuit Programming, via pin dedicati, accede a tutto incluso bootloader e code option) vs
**ISP** (via USB, tramite il bootloader residente; non scrive il bootloader).

---

## 8. Reverse engineering e strumenti di laboratorio

| Strumento / risorsa | Uso nel piano | Stato |
| --- | --- | --- |
| **Disassemblaggio MCS-51**: Ghidra ha un modulo 8051 incluso; plugin di terze parti [amtal/i8051](https://github.com/amtal/i8051) (usato da altri su Binary Ninja/IDA/Cutter) | M1.6: analisi del dump stock (init, sleep, descrittori, tabella keymap) | ◐ |
| **Ghidra** — sito ufficiale | idem | ○ (https://ghidra-sre.org) |
| **Toolkit REA** (MCP "rea" disponibile in questo ambiente: `extract_firmware`, `binary_overview`, `procedure_assembly`, …) e la skill `reverse-engineer-anything` | Alternativa a Ghidra per un primo giro sul dump (`.hex`/`.bin`); posso guidarti io | strumento locale |
| **sigrok + PulseView** (decoder SPI, analizzatori economici con driver `fx2lafw`; regola pratica: campiona ≥ 2× la frequenza del segnale — usa 8–16 MS/s per bus tipici) — [tutorial SparkFun](https://learn.sparkfun.com/tutorials/using-the-usb-logic-analyzer-with-sigrok-pulseview) | M1.5: cattura dei frame SPI MCU↔BK3632 | ◐ |
| Analizzatore logico commerciale (es. Saleae) + decoder SPI | idem | ○ |
| Multimetro (continuità/diodo), alimentatore con limite di corrente, **misuratore di corrente** (µA) | M1.2/M1.3 (mappatura), M6.6 (corrente di sleep) | — |
| `sinowisp read` / `sinodude read` come "sorgente" di binari da analizzare | M0.5 | ✔ |
| Documentazione hardware: 📁 `docs/keyboards/*.md` | Modello di come documentare pinout, code option, descrittori | 📁 |

**Metodo consigliato per la matrice** (M1.2): con la tastiera aperta, in modalità diodo, misura dal pad di ogni
switch ai pin MCU candidati; verifica la direzione dei diodi; annota in una tabella `(riga, colonna) → pin`.
Conferma poi col firmware stock e un analizzatore logico che scandisce le colonne.

---

## 9. Altro materiale utile

| Argomento | Risorsa | Stato |
| --- | --- | --- |
| Codice QMK (design di riferimento di keymap/layer/processing) | [qmk/qmk_firmware](https://github.com/qmk/qmk_firmware), [docs.qmk.fm](https://docs.qmk.fm) | ✔ (docs rawhid aperte) |
| Keycode QMK (tabelle numeriche) | 📁 [keycodes.h](../src/smk/keycodes.h) (numerazione ≥ 0.19, già allineata) | 📁 |
| Debounce, ghosting, matrici di tastiere | Articoli generali su "keyboard matrix scanning" e ghosting | ○ |
| Embedded in generale | *Making Embedded Systems* (E. White); *The Art of Designing Embedded Systems* (J. Ganssle) | ○ |
| Concorrenza ISR/main | Manuale SDCC (sezioni 3.8–3.9) + [architecture.md §4](architecture.md#4-toolchain-e-modello-di-compilazione-sdcc) | ✔ |
| Meson | [mesonbuild.com](https://mesonbuild.com) per `generator()`, `custom_target()`, `run_target()` | ○ |
| Nix | `flake.nix` del repo come esempio | 📁 |

---

## 10. Cosa manca e come lo recuperiamo

| Mancante | Impatto | Piano |
| --- | --- | --- |
| Datasheet ufficiale **SH68F90A** (USB EP, flash endurance, code option, ICP) | Blocca S1/S2/S4, M0.7 | Cerca al punto §3.1 (sito, distributori, community); nel frattempo usiamo il header SFR e misure sperimentali |
| Datasheet / protocollo **BK3632** | R1, M6 | Cattura SPI sul firmware stock (M1.5) |
| Schema/teardown della **B68** | M1 | Misure dirette (M1.2–M1.4) |
| Preset **sinowisp** per la B68 | M3.5 | Verificare nel repo sinowisp; altrimenti flag espliciti |
| Versione esatta di Vial GUI e keycode v6 | M4 | Spike S3 |
