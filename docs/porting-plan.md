# Piano di sviluppo — SMK su Yunzii B68 + VIAL

> Piano di lavoro per portare SMK sulla Yunzii B68 (BYK916 + BK3632) ed estenderlo con il protocollo
> VIAL. Si legge insieme a [architecture.md](architecture.md) (come funziona oggi il codice) e a
> [REFERENCE.md](REFERENCE.md) (cosa studiare). Il piano è scritto per essere **eseguito a task**:
> ogni task ha un ID, un risultato verificabile e dipendenze. Nelle prossime sessioni io genero i task
> operativi (uno alla volta, con specifica e criteri di review) a partire da questa lista; tu implementi,
> io faccio review.
>
> Documenti operativi: [tasks/](tasks/README.md) (schede dei task) · [recovery-icp.md](recovery-icp.md)
> (programmatore Arduino Nano) · [usb-hid.md](usb-hid.md) (stack USB/HID, spike S1).
>
> **Decisioni prese (rev. 2):** programmatore ICP **opzionale** ma consigliato (§4 D7, gate G0) · USB **6KRO** ·
> **8 layer** · **Vial come adattatore sottile su un nucleo nostro** (D8), con piano B su EP0 · wireless
> (**2.4G e BLE**) **richiesto** ma dopo VIAL.
>
> Convenzione: **[F]** = fatto verificato nel repo o in una fonte consultata · **[I]** = ipotesi / da
> verificare sul tuo hardware · **[D]** = decisione aperta.

---

## Indice

1. [Obiettivo e definizione di "fatto"](#1-obiettivo-e-definizione-di-fatto)
2. [Punto di partenza: cosa sappiamo e cosa no](#2-punto-di-partenza-cosa-sappiamo-e-cosa-no)
3. [Registro dei rischi](#3-registro-dei-rischi)
4. [Decisioni architetturali aperte](#4-decisioni-architetturali-aperte)
5. [Roadmap a milestone](#5-roadmap-a-milestone)
6. [M0 — Preparazione e sicurezza](#m0--preparazione-e-sicurezza)
7. [M1 — Reverse engineering della B68](#m1--reverse-engineering-della-b68)
8. [M2 — Bring-up USB (scheda "wired only")](#m2--bring-up-usb-scheda-wired-only)
9. [M3 — Bootloader SMK sulla B68](#m3--bootloader-smk-sulla-b68)
10. [M4 — VIAL](#m4--vial)
11. [M5 — LED / indicatori](#m5--led--indicatori)
12. [M6 — Wireless (BK3632) e sleep](#m6--wireless-bk3632-e-sleep)
13. [M7 — Estensioni VIAL (opzionali)](#m7--estensioni-vial-opzionali)
14. [M8 — Hardening, documentazione, upstream](#m8--hardening-documentazione-upstream)
15. [Modalità di lavoro e review](#15-modalità-di-lavoro-e-review)
16. [Domande aperte per te](#16-domande-aperte-per-te)

---

## Stato avanzamento

Aggiornato da Claude a ogni chiusura di task (ultimo aggiornamento: 2026-10-31).

| Milestone | Stato | Note |
| --- | --- | --- |
| M0 Preparazione | **in corso** | M0.1 e M0.2 chiusi. M0.5 e M0.9 chiusi (G0 non superato). Dump verificato (M0.10 chiuso). Pronto: M0.4 (pinout connettore). M0.3 (acquisto Nano) ora **obbligatorio** prima del primo flash |
| M0 | **chiusa a metà** | Restano M0.3, M0.4, M0.6, M0.7: si chiudono all'arrivo del Nano |
| M1 | **in corso** | M1.7 chiuso (258a:010c, EP1 8 B + EP2 16 B IN, nessun EP OUT, feature ID6 da 519 B). M1.6 (analisi del firmware stock, Claude): prima passata fatta, vedi [yunzii-b68-firmware-stock.md](keyboards/yunzii-b68-firmware-stock.md); restano matrice, keymap, radio, LED. M1.2–M1.5 attendono M0.4 |
| M2 | **in corso** | M2.1, M2.3 e M2.9 chiusi (scaffold, GPIO a riposo come lo stock, hook di matrice; verificati in simulatore). Prossimi (senza hardware): nucleo di M4 (layer, storage keymap, VIA/Vial) nel simulatore. Layout fisico, bootloader B68 (M3.1) e chord definitivo aspettano la mappa dei tasti dal firmware sonda. Il lavoro senza hardware (M2.1, poi M4.7/M4.13–M4.17 nel simulatore) procede; i pin reali e il flash aspettano M0.4/M1.2 e il Nano |
| M3–M8 | non iniziate | |

Registro delle decisioni/rilievi emersi durante i task:

* M0.9: **G0 non superato**: l'ISP stock si raggiunge solo tramite l'applicazione (258a:010c, iface 1, report 5). Nano/ICP obbligatorio prima del primo flash (R11). 
* **Prima prova su hardware (2026-10-31):** scrittura via ISP dell'immagine stock identica (OK, rilettura uguale), poi firmware sonda: matrice 5×16 dello stock **confermata** (67 celle su 68 tasti; ne manca una in riga 4). La tastiera ha 66 tasti + encoder rotativo (pressione su c15r0, rotazione ancora da trovare con la sonda v2). Layout `default` e `vial.json` ora reali (65% ANSI), chord di recupero Esc+1+2, sblocco Esc+Invio. G0 resta non superato, ma il percorso ISP→SMK funziona.
* **Spike S1 (build pronta, non ancora provata su hardware):** layout `rawhid` della B68 (`-DRAW_HID_ENABLE`): interfaccia 1 = raw HID (EP2 IN+OUT da 32 B) collegata a `via_receive()`; NKRO/extra/console disattivati su EP2, niente ISP dall'applicazione. Provato nel simulatore (richieste VIA/Vial, due richieste di fila). Strumenti: `tools/rawhid-test.html` (WebHID) e `tools/rawhid_test.py`. Il simulatore alza l'interrupt USB solo per un SETUP, per questo i test lo accoppiano a un GET_DESCRIPTOR innocuo.
* **S1 superato su hardware (2026-10-31):** EP2 OUT da 32 B funziona; il piano principale (Vial su Raw HID) è confermato. Build `rawhid` completata (D1): interfaccia 0 = tastiera (con ID, senza in boot protocol) + media/sistema + ISP, interfaccia 1 = raw HID; da provare su hardware.
* **Guide per la radio (2026-10-31):** [radio-capture-guide.md](radio-capture-guide.md) (cattura con l'analizzatore, da fare quando si potrà aprire la tastiera) e [radio-software-probe.md](radio-software-probe.md) (sonda senza aprire). LED dichiarati: RGB sotto i tasti a sud + LED di stato vicino al Caps.
* **HUD non rilevato dall'app (2026-10-31):** Vial funziona, l'app dell'utente no. Sospetto principale: l'app rifiuta il protocollo 2 (matrice 16 colonne). Build diagnostica `yunzii-b68_hudv1_smk.hex` (annuncia protocollo 1 con 8 colonne) e pulsanti HUD in `tools/rawhid-test.html`.
* **Radio e LED, analisi statica (2026-10-31):** la radio dello stock è l'EUART in modo 0 (non SPI a bit-bang) con `P0.2` come selezione e `P4.7` come attenzione; i LED hanno 18 colonne PWM su P1/P2/P3 e righe ancora da trovare. Dettagli e cosa serve dal banco in [yunzii-b68-firmware-stock.md](keyboards/yunzii-b68-firmware-stock.md).
* **Macro (2026-10-31):** buffer da 512 B / 16 macro nello store (formato versione 3, sul B68 `keymap_sectors = 4`, regione da `0xDC00`), comandi VIA completi (scrittura solo da sbloccata), esecutore `macro.c` (testo US, tap/down/up anche a 16 bit, ritardi; i tasti `M0…`). Test host (esecutore, store, comandi) e 2 sul simulatore con uno slot costruito byte per byte. Il riflash cambia il formato dello store: la keymap salvata prima viene ignorata.
* **HUD (2026-10-31):** protocollo `0xF0` dell'overlay dell'utente integrato in `src/smk/hud.c`; per matrici più larghe di 8 colonne protocollo 2 con `GET_MATRIX`. Vedi [hud.md](hud.md).
* **Vial desktop (2026-10-31):** l'app desktop elenca solo le tastiere con `vial:f64c2b3c` nel numero di serie USB (vial.rocks invece sonda la tastiera): la B68 ora ha `serial = vial:f64c2b3c`. Encoder in Vial: `vial_get/set_encoder` (direzione 0 = antiorario, 1 = orario), mappa dell'encoder nello store (versione 2) e tre tasti virtuali nel `vial.json`.
* **Tap-hold (2026-10-31):** `LT`, `MT`, `LM`, `TT` supportati. `tapping.c` decide tap/hold (release prima di 200 ms = tap, timeout = hold), tiene in coda gli eventi che arrivano mentre il tasto è indeciso e li ripropone dopo la decisione (come QMK, senza "permissive hold"); `TT` si blocca con 5 tocchi. Test host (tapping 10 gruppi, layer con LM/TT) + 2 test sul simulatore con la keymap assegnata via VIA. Nota: i test sul simulatore usano gli `.hex` già compilati: dopo una modifica va ricompilata la build (`meson compile -C build-b68`) prima di lanciare `unittest` a mano (`meson test` ricompila da solo).
* **bcdDevice per layout (2026-10-31):** contro il rifiuto di Windows dopo un riflash che cambia i descrittori: `-DUSB_BCD_DEVICE`, `rawhid` = 0x0200.
* **Encoder (2026-10-31):** A=P0.6, B=P0.7 trovati con la sonda; decoder a quadratura in `src/smk/encoder.c` (campionato nel tick, mappa per layer `encoder_map` come ENCODER_MAP di QMK). Test: 14 controlli host + 2 sul simulatore (senso orario = Vol+). Il senso orario è un'ipotesi (primo senso girato = orario). Non ancora editabile da Vial: servono i comandi `vial_get/set_encoder` e il blocco encoder nello store.
* Layout `probe` per la B68 (digita "cNNrR " per ogni cella) e sequenza sul banco in [hardware-bringup.md](hardware-bringup.md). Provato nel simulatore (colonne a 1 e 2 cifre).
* M4.6/M4.8–M4.12 + M4.19b (protocollo): `via.c`/`vial.c` (comandi VIA, buffer keymap BE, identificazione Vial, definizione LZMA a pagine, sblocco con tasti, bootloader jump protetto), `utils/vial_gen.py`, `timer.c` (ms da scansioni, auto-calibrato su SOF). Formati verificati leggendo `via.c`/`vial.c`/`vial_generate_definition.py` di vial-qmk (branch `vial`). 133 controlli host + 2 mutazioni individuate; orologio testato (±0,5 %). **Mancano:** trasporto Raw HID (M4.1–M4.5, serve lo spike S1), keycode custom `QK_KB` (M4.18), `vial.json` con il layout fisico (dopo la sonda), LT/MT sopra il timer. I tasti di sblocco della B68 sono provvisori (celle 0,0 e 0,1).
* M4.13–M4.16 (meccanismo): `keymap_store.c` (RAM 8 layer + 2 slot A/B in flash, header scritto per ultimo, CRC16, sequenza), opzione `keymap_sectors` in meson (B68: 3 → riserva 0xE000–0xEBFF), `keymap_task()` col flush dopo ~2000 scansioni a riposo. Test host con flash simulata che perde alimentazione a ogni passo del salvataggio (664 controlli, 2 mutazioni individuate). Non ancora collegato a Vial. Costo sulla B68: +1356 B XRAM, +2.3 KB flash.
* M4.17 (parte 1): nuovo `src/smk/layer.c` (MO/TO/TG/DF/OSL/OSM, 8 layer, cache del keycode per posizione) con `keymap.c` come unico accesso alla keymap. Costo sulla Air60: +167 B XRAM, +1 KB flash. Test host `tests/test_layer_host.py` (46 controlli, provato con una mutazione) e suite sim invariata. Cambio di comportamento: al rilascio di `MO` non si chiama più `clear_keys()`, perché il rilascio usa il keycode della pressione.
* M2.9: chord di recupero implementato in `src/smk/recovery.c` (si abilita per scheda con `RECOVERY_CHORD_KEYS` in `kbdef.h`; costo 677−661=16 B XRAM e ~200 B di flash sulla B68, nullo sulle altre). Sulla B68 le celle sono provvisorie (0,0)(1,0)(2,0): **da sostituire con tre tasti reali prima del primo flash** (dipende dalla sonda).
* M2.3: i GPIO a riposo letti nel simulatore coincidono con la tabella dello stock (16/16 registri). `GPIO_DRIVE_25MA` vale 0, quindi il drive dello stock (0) è lo stesso della Air60.
* M1.6: dal firmware stock la matrice è 16 strobe (P6.0–7, P5.0/1/2/7, P4.0/2/3/5) × 5 righe (P7.1–3, P5.3/4); P7.0 è un ingresso extra di funzione ignota; la mappa tasto→(col,riga) si ottiene con un **firmware sonda** dopo il primo flash (niente multimetro). L'ingresso ISP dall'app è identico a SMK.
* M1.7: lo stock configura via feature report ID 6 (519 B) su EP0 e non usa EP OUT ⇒ piano B di D8 confermato fattibile; D1 (EP2 OUT) resta da provare con S1.
* M0.10: riletture identiche; il primo dump differiva di 2 byte di stato (0xC60A, 0xC65A). Backup affidabile: `backups/dump/r1.bin`.
* M0.5: reset vector stock = `LJMP 0x9A26` (applicazione); il bootloader è a 0xF000. Raccolta del report mancante; doppia lettura da confermare in M0.9.
* M0.1: la suite `meson test` su build release ha 1 errore noto (`console_buf` esiste solo in debug); non è un difetto del firmware.


## 1. Obiettivo e definizione di "fatto"

**Obiettivo.** Sulla Yunzii B68 gira firmware SMK; la tastiera si configura da **Vial** (GUI ufficiale) senza
ricompilare: keymap per layer modificabile, persistente, con sblocco di sicurezza.

**Fatto quando** (criteri verificabili, in ordine di priorità):

| # | Criterio |
| --- | --- |
| C1 | Tutti i tasti della B68 funzionano in USB; nessun tasto fantasma/bloccato; NKRO (se mantenuto) verificato |
| C2 | La scheda è **recuperabile senza aprire il case** (bootloader SMK con chord) e il firmware stock è **ripristinabile** (dump verificato, ripristino provato) |
| C3 | Vial GUI rileva la tastiera, mostra il layout, modifica i keycode di ≥ 4 layer; le modifiche **sopravvivono** a reset e replug |
| C4 | Operazioni distruttive (salto in bootloader, reset della configurazione) protette dall'unlock di Vial |
| C5 | Wireless 2.4G/BT funzionante *oppure* decisione documentata di non supportarlo (vedi R1) |
| C6 | Sleep/wake e retroilluminazione equivalenti allo stock, o differenze documentate |
| C7 | Build `debug` e `release`, `check_interrupts` OK, test su simulatore verdi, dimensioni misurate e riportate |

**Non obiettivi** (per ora): portare SMK su altre schede; supportare ogni feature QMK (combo, tap dance,
key override, …); VialRGB completo; OTA/aggiornamenti firmware via radio.

---

## 2. Punto di partenza: cosa sappiamo e cosa no

### 2.1 Fatti [F]

* La NuPhy Air60 v1 usa **lo stesso accoppiamento** MCU + radio (BYK916/SH68F90A + BK3632) ed è la scheda
  SMK più vicina alla tua: è il modello da copiare ([architecture.md §14](architecture.md#14-il-contratto-di-una-scheda-board-port-interface)).
* Spazio disponibile sulla Air60 (release): ≈ 39 KB di flash e ≈ 3 KB di XRAM liberi — ampio per Vial.
* L'upstream `sinowisp` elenca fra i dispositivi supportati **Yunzii AL66 e AL71 con SH68F90A**: Yunzii usa
  questa famiglia di MCU e i suoi bootloader sono noti al tool. La **B68 non è elencata** (esito della
  lettura del README di sinowisp, non verificato su hardware).
* Il bootloader stock è sostituibile solo via **ICP**; sinowisp da solo non scrive l'area bootloader.
* `sinowisp read` ha l'opzione `-s firmware|bootloader|full` e preset `yunzii-al66`/`yunzii-al71`: un backup
  completo **via ISP, senza programmatore**, è plausibile **[I]** (verificare in M0.5).
* Il programmatore ICP (Arduino Nano + MOSFET P + resistore) è descritto in [recovery-icp.md](recovery-icp.md);
  `sinodude` conosce la parte `sh68f90a`.

### 2.2 Ignoti sulla B68 [I] — tutto da misurare in M1

| Ignoto | Perché conta |
| --- | --- |
| Dimensioni matrice (righe × colonne), direzione diodi | `MATRIX_ROWS/COLS`, tutta `user_matrix.c`; il core ammette ≤ 8 righe |
| Pin di righe/colonne/LED/switch/SPI radio | `kbdef.h`, `user_init.c`, `user_sleep.c` |
| Tipo di retroilluminazione (RGB per-key? monocromatica? solo status?) | `indicators.c`; i canali PWM sono legati a pin precisi del chip |
| Presenza di slider connessione/OS, pulsante di pairing | `kb.c` |
| **Protocollo del BK3632 nella B68** | l'intero `rf_controller.c` è ricavato dal firmware NuPhy (R1) |
| Code option programmate nel tuo chip | il firmware assume oscillatore/IO/ISP specifici |
| Se il bootloader stock è presente/abilitato e con che VID:PID | strategia di primo dump e di flash |
| Dimensione bootloader stock / ISP size | `bootloader_size`, `firmware_size` |
| Endurance della flash del chip | strategia di scrittura della keymap (R4) |
| Corrente di sleep e pin di wake | M6 |

---

## 3. Registro dei rischi

Probabilità/impatto: A/M/B. "Gate" = va chiuso prima di proseguire.

| ID | Rischio | P | I | Mitigazione / gate |
| --- | --- | --- | --- | --- |
| **R1** | Il BK3632 della B68 non parla il protocollo NuPhy (firmware radio diverso, comandi/byte di stato diversi) | M | **A** | Cattura SPI col firmware stock (M1.5) **prima** di scrivere codice radio. Se diverge: reimplementare dal capture oppure rinviare il wireless (C5 → "USB only"). Non blocca Vial. |
| **R2** | Brick durante il primo flash o con code option errate | M | A | M0: due dump identici + prova di ripristino **prima** di sovrascrivere; **chord di recupero in `main()`** (M2.9); gate G0 (M0.9); programmatore ICP pronto (D7); ordine: prima dump, poi tutto il resto |
| **R11** | Senza ICP, un'app che non si avvia *e* nessun ingresso hardware in ISP ⇒ scheda irrecuperabile (hai una sola B68) | M | **A** | Gate G0: prova l'ingresso in ISP indipendente dall'app **prima** del primo flash; acquista comunque il Nano (D7) |
| **R3** | Endpoint USB insufficienti per Raw HID di Vial senza rompere NKRO/extra/ISP | **A** | A | Decisione D1 + spike S1 su hardware prima di scrivere il resto |
| **R4** | Usura della flash per scritture della keymap; endurance non nota | M | M | Write-coalescing + log append-only (D2); leggere endurance dal datasheet (da trovare) |
| **R5** | Erase/program della flash blocca CPU → glitch di scan, USB, SPI radio | M | M | Spike S2: misura durata; esegui solo da idle; flush dopo inattività |
| **R6** | Pinout/PWM dei LED vincolato dal chip: non tutti i pin hanno PWM | M | M | Mappa pin↔funzione in M1.3 dal `sh68f90.h` e dal firmware stock |
| **R7** | Overlay statico SDCC corrompe dati fra ISR e main (nuovo codice USB OUT) | M | A | `check_interrupts.py` come gate; ISR minime (solo copia + flag) |
| **R8** | Bootloader SMK senza margine (≈ 67 B) non entra con il `user_init` della B68 | M | M | Misura subito in M3.1; ridurre `boot_src` (init solo dei pin matrice) |
| **R9** | Vial-GUI/vial-qmk cambiano versione di protocollo/keycode | B | M | Fissa la versione target in S3; test con quella GUI |
| **R10** | Licenza/firmware stock: dump proprietario | B | B | **Non committare** il dump nel repo; tienilo fuori (`.gitignore`) |

---

## 4. Decisioni architetturali aperte

Ogni decisione ha una **raccomandazione**; le confermiamo con uno spike dove indicato.

### D1 — Come far coesistere Raw HID con tastiera/extra/NKRO/ISP (endpoint)

Vincoli [F]: l'MCU ha **EP0 (8 B)**, **EP1 (16 B)**, **EP2 (64 B)**, con buffer IN e OUT su EP1/EP2.
Vial-GUI vuole un'interfaccia HID **senza Report ID**, usage page `0xFF60`, usage `0x61`, **report da 32 B**
in entrambe le direzioni ([QMK Raw HID](https://docs.qmk.fm/features/rawhid)).

Stato attuale: iface 0 = tastiera boot su EP1 IN; iface 1 = extra/NKRO/ISP/console **con Report ID** su EP2 IN.

| Opzione | Descrizione | Pro | Contro |
| --- | --- | --- | --- |
| **A (raccomandata)** | **EP1 IN condiviso** per tastiera+system+consumer+NKRO (con Report ID, come "KEYBOARD_SHARED_EP" di QMK; in boot protocol i report vanno **senza** ID). **EP2 IN+OUT** = interfaccia Raw HID senza ID. Il feature report ISP passa sull'iface 0 | Funziona con Vial standard; schema già usato da QMK | EP1 è 16 B ⇒ NKRO ≤ 15 B totali (ID+mods+13 B bitmap = usage 0–103, sufficiente per i tasti standard); console di debug (33 B) va spezzata o spostata; cambia l'iface dell'ISP report ⇒ preset sinowisp |
| B | Solo 6KRO in USB, EP1 condiviso, EP2 = Raw HID | Più semplice | Perdi NKRO in cavo |
| C | Raw HID a 32 B su EP1 (16 B) in due pacchetti | Non cambia le interfacce | Trasferimenti multi-pacchetto su interrupt: comportamento dell'host non affidabile — **scartata** |
| D | Configurazione via feature report su EP0 con tool proprio | Nessun endpoint | **Non è Vial**; scartata |

**Decisione presa:** **6KRO via cavo** (opzione **B** del precedente elenco, schema A per gli endpoint). Il build ha già
`-Dnkro` (da lasciare disabilitato). Limite concreto: più di 6 tasti non modificatori simultanei (i modificatori
sono a parte). Dettagli e design in [usb-hid.md §10](usb-hid.md#10-design-di-destinazione-d1).

**Azione:** spike S1 (esempio standalone che prova EP2 OUT+IN a 32 B) per confermare che il blocco USB e
la logica del SIE supportano l'OUT su EP2 come richiesto. Poi decidere A vs B in base al valore che dai al NKRO.

### D2 — Dove e come salvare la keymap

* Cache in **XRAM** (accesso veloce nel loop di scan) alimentata all'avvio dalla flash; i default restano la
  `const keymaps[][][]` del `layout.c` (i "factory defaults").
* Dimensione: `ROWS × COLS × 2 B × layer` (es. 5×15×2 = 150 B/layer). **Layer massimi: 8** ⇒ ≈ 1,2 KB ⇒ **3 settori**
  da 512 B (+ header/CRC). La lettura avviene **direttamente dalla flash** (spazio `__code`): non serve una cache in
  XRAM; serve solo un buffer da 512 B per riscrivere un settore.
* Persistenza:
  * **MVP (A):** regione di N settori dedicati + header (magic, versione, layer count, CRC), riscrittura
    dell'intera regione con settori A/B alternati (power-fail safe); scrittura **dopo ≥ 2 s di inattività**.
  * **Evoluzione (B):** log append-only di `(indice, valore)` con compattazione quando il settore è pieno
    (1 modifica = pochi byte programmati, 1 erase ogni centinaia di modifiche): minimizza usura.
* Raccomandazione: parti da **A** dietro un'interfaccia stabile (`keymap_store_*`) così passare a B è locale.
* Va **generalizzato il calcolo della mappa flash** in `meson.build` (oggi un solo settore, [§5.3](architecture.md#53-calcolo-automatico-della-mappa-flash)).

### D3 — Quali keycode di Vial supportare

Il core oggi implementa `MO` e i keycode base/consumer/system ([§8.2](architecture.md#82-cosa-il-motore-dei-tasti-non-fa-oggi)).
**Il motore dei layer va scritto comunque, con o senza Vial.** Fasi: **v1** = base, modificatori, `MO/TO/TG/DF`,
**`OSL/OSM`** (one-shot), trasparente, consumer/system, custom di scheda (BT/LED), `QK_BOOT`; **v2** = `LT/MT`
(richiedono un **orologio al millisecondo**: oggi il Timer2 alterna slot da 100/400 µs e non c'è un `timer_read()`);
**v3** = macro, tap dance, combo (M7). I keycode non
supportati devono degradare in modo sicuro (trattati come `KC_NO`), non crashare.

### D7 — Programmatore ICP: opzionale ma consigliato

Si lavora in ISP (stock bootloader intatto). Protezioni: (1) **gate G0** (M0.9): dimostrare che la B68 entra in ISP
senza l'applicazione; (2) **chord di recupero in `main()`** (M2.9): prima di qualsiasi init pesante il firmware controlla
un chord e chiama `isp_jump()` (sopravvive anche ai riavvii del watchdog); (3) regola: mai scrivere oltre
`bootloader_addr`. Il bootloader SMK (M3) diventa **opzionale**. Il Nano si compra comunque (pochi euro): vedi
[recovery-icp.md](recovery-icp.md).

### D8 — Vial: adattatore sottile su un nucleo nostro

Il **nucleo** (motore dei layer, store della keymap, persistenza, timebase) è indipendente dal protocollo. Vial è un
**adattatore** (~300–500 righe: `get_keyboard_id`, definizione, keymap dinamiche, unlock) sopra il nucleo. Le
impostazioni LED/radio passano dal canale custom di VIA (`0x07–0x09`) così non serve una UI nostra. **Punto di
decisione:** dopo lo spike **S1** (EP2 IN+OUT a 32 B, [usb-hid.md §11](usb-hid.md#11-cosè-uno-spike-e-cosè-lo-spike-s1))
e **S3** (sorgenti Vial). **Piano B** se S1 fallisce: protocollo nostro via **feature report su EP0** con webapp
WebHID (riusa lo stesso nucleo). Da verificare in S3: come Vial-GUI riconosce il dispositivo (numero di serie?),
supporto di `menus`/UI custom in `vial.json`.

### D4 — Vial solo in USB

Raw HID esiste solo quando il cavo è collegato (`conn_mode = USB`). In modalità RF la configurazione non è
disponibile (comportamento comune alle tastiere wireless). Va scritto nella documentazione.

### D5 — Allineamento dei keycode custom

I keycode custom di SMK partono da `SAFE_RANGE = QK_USER (0x7E40)`; Vial mostra i "custom keycode" a partire da
`QK_KB_0 (0x7E00)` e li descrive in `vial.json`. Raccomandazione: spostare i custom della B68 in `QK_KB_x` e
descriverli in `customKeycodes`.

### D6 — USB VID:PID

Il repo usa gli ID stock di ciascuna scheda (es. NuPhy `05ac:024f`). Per uso personale si può mantenere
l'ID stock della B68; **per qualunque distribuzione** non puoi usare VID altrui. Per Vial l'ID non conta (usa il
`VIAL_KEYBOARD_UID`). Decisione rimandata a M8.

---

## 5. Roadmap a milestone

```
M0 Preparazione ──► M1 Reverse eng. ──► M2 Bring-up USB ──► M3 Bootloader ──► M4 VIAL ─┬─► M7 Estensioni
  (sicurezza)         (pin, radio)        (tasti ok)        (recuperabile)    (obiettivo) │
                           │                                                              └─► M8 Hardening
                           ├──────────────► M5 LED ───────────────────────────────────────────┘
                           └──────────────► M6 Wireless + sleep (gate R1) ────────────────────┘
```

Ordine consigliato e motivazione: **VIAL (M4) prima del wireless (M6)**. Vial dipende solo da USB+keymap,
ha rischio tecnico basso-medio e dà subito il valore che cerchi; il wireless dipende dall'esito di R1 e può
richiedere reverse engineering consistente. M5 e M6 possono procedere in parallelo a M4 se hai ore di
laboratorio.

Stime di effort per task: **S** ≤ 2 h · **M** ≤ 1 giorno · **L** più giorni (con hardware di mezzo).

---

## M0 — Preparazione e sicurezza

**Obiettivo:** avere ambiente funzionante e la certezza di poter tornare indietro prima di toccare la tastiera.

| ID | Task | Verifica (acceptance) | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M0.1 | Ambiente **e baseline**: `nix develop`, build `debug`+`release`, `meson test`, dimensioni di nuphy-air60 | scheda: [tasks/M0.1](tasks/M0.1-ambiente-e-baseline.md) | — | S |
| M0.2 | Scheletro di `docs/keyboards/yunzii-b68.md` (sezioni vuote da riempire nelle milestone) | file con indice e tabelle vuote | M0.1 | S |
| M0.3 | Hardware: **Arduino Nano ATmega328P + MOSFET P + 10 kΩ + breadboard** ([recovery-icp.md](recovery-icp.md)), clip/pogo-pin, multimetro, analizzatore logico ≥ 8 ch (M1.5), cavo USB-C dati | elenco; signature del Nano letta (`0x1e950f`); sinodude-serial caricato | — | S |
| M0.4 | Individuare i punti ICP sul PCB (foto fronte/retro, test pad, schema di collegamento dal README di sinodude) | foto annotate in `docs/keyboards/yunzii-b68.md` | M0.3 | M |
| M0.5 | **Dump completo flash**: prima via **ISP** (`sinowisp read -s full`, senza saldare); poi via ICP (×2) se il Nano è pronto | `sha256` identici fra letture; ISP e ICP coerenti; file **fuori dal repo** | M0.4 | M |
| M0.6 | **Prova di ripristino**: riscrivi il dump stock sulla tastiera e verifica che funzioni come prima | tastiera stock funzionante dopo riscrittura; procedura annotata | M0.5 | M |
| M0.7 | Lettura delle **code option** del chip e confronto con quelle richieste da SMK ([§3.3](architecture.md#33-code-option-fuse)) | tabella byte-per-byte con differenze evidenziate | M0.4 | M |
| M0.9 | **Gate G0**: con firmware stock, dimostrare che la B68 entra in **ISP** (VID:PID `0603:1020` o quello del suo bootloader) **senza l'aiuto dell'applicazione** (pin hardware/chord) — oppure annotare che non è possibile | `sinowisp list` mostra il dispositivo in ISP; procedura scritta. **Se non possibile ⇒ l'ICP diventa obbligatorio prima del primo flash (R11)** | M0.5 | M |
| M0.8 | Git: fork/branch `port/yunzii-b68`, regole di commit (stile del repo: messaggio breve minuscolo), `.gitignore` per dump | branch creato; nessun binario proprietario tracciato | — | S |

**Exit M0:** C2 *parzialmente* soddisfatto (recuperabilità hardware), nessun brick possibile senza avere un backup.

---

## M1 — Reverse engineering della B68

**Obiettivo:** documentare l'hardware abbastanza da poter scrivere `kbdef.h`, `user_init.c`, `user_matrix.c`,
`user_sleep.c` senza indovinare. Output: `docs/keyboards/yunzii-b68.md` (stesso formato di
[nuphy-air60.md](keyboards/nuphy-air60.md)).

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M1.1 | Identificare marcature MCU/radio, confrontare con le foto della Air60 | foto + conferma "BYK916 = SH68F90A" (stesso footprint/pin count) | M0.4 | S |
| M1.2 | **Matrice**: n° righe/colonne, direzione dei diodi, mappa pin → riga/colonna. Metodi: continuità/diodo dal pad dello switch al pin MCU; oppure cattura col logic analyzer mentre il firmware stock scandisce | tabella `(riga, colonna) → pin` + tasto fisico; ogni tasto verificato | M0.4 | L |
| M1.3 | **LED**: tipo, driver (GPIO/PWM diretto), pin, quali pin hanno canale PWM ([`sh68f90.h`](../src/sino51lib/sh68f90/sh68f90.h)), sink/source | tabella pin↔funzione; confronto con lo schema della Air60 (colonne su PWM, righe RGB su GPIO) | M1.2 | L |
| M1.4 | **Switch e ingressi**: slider connessione/OS, pulsante pairing/reset, rilevamento USB/charger, ADC batteria (se esiste) | tabella pin ↔ funzione ↔ livello logico | M1.2 | M |
| M1.5 | **Radio BK3632**: individua le 6 linee verso l'MCU (CS, SCK, MOSI, MISO, MOT/attention, ACK) + pin di wake; **cattura SPI** col firmware stock in scenari: boot, cambio link (2.4G/BT1-3), pairing, pressione tasto, rilascio, query stato, sleep/wake | file di cattura + documento che confronta i frame con [`rf_controller.c`](../src/peripherals/bk3632/rf_controller.c) (header `0xAA`, checksum `0x55−Σ`, stato `0xBB`): **verdetto go/no-go sul riuso** | M1.4 | L |
| M1.6 | **Analisi del firmware stock** (disassembler MCS-51: Ghidra o il toolkit REA): sequenza di init (clock/LDO), descrittori USB, tabella keymap, periodo PWM, **sequenza di parcheggio sleep**, uso di INT4 | note con indirizzi; documento descrittori HID come nella pagina Air60 | M0.5 | L |
| M1.7 | USB stock: `lsusb -v`, `usbhid-dump` (VID:PID, stringhe, interfacce, report descriptor); VID:PID e interfaccia del **bootloader stock** (per sinowisp) | output archiviati in `docs/keyboards/yunzii-b68.md` | M0.5 | S |
| M1.8 | Controllare se `sinowisp` ha già un preset `-d` per la B68 o un cugino (AL66/AL71) | esito annotato; se manca, serve preset o flag espliciti (M3) | M1.7 | S |

**Exit M1:** pinout completo; verdetto su R1; elenco hardware della scheda (switch, LED, batteria).

---

## M2 — Bring-up USB (scheda "wired only")

**Obiettivo:** SMK gira sulla B68 con **tastiera, layer Fn, indicatori lock**; senza LED, radio, sleep.

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M2.1 | Scaffold `src/keyboards/yunzii-b68/` da [`example/`](../src/keyboards/example) + registrazione `keyboards += {…}` (`platform:'sh68f90'`, `icp_part:'sh68f90a'`) | `meson setup` ok; target `yunzii-b68_default_smk.hex` compila (anche con stub) | M0.1 | M |
| ~~M2.2~~ | (unita a M2.1) `kbdef.h`: `MATRIX_ROWS/COLS`, macro pin riga/colonna, stringhe USB | compila; `_Static_assert` coerenti | M1.2 | M |
| M2.3 | `user_init.c`: direzione/pull-up/drive dei pin (solo ciò che serve alla matrice per ora); WDT | compila; nessun pin LED/radio in stato indefinito | M2.2 | M |
| ~~M2.4~~ | (unita a M2.3) `user_matrix.c`: 7 hook (`cols_deselect_all`, `col_select/deselect`, `read_rows`, `scan_pre/post`, `sinks_off`) | log di debug (`smk-console`) mostra ogni tasto alla posizione corretta; verifica "ghosting" e tempi di assestamento | M2.3 | L |
| M2.5 | `layouts/default/layout.c`: macro `LAYOUT_xx` + layer base + Fn (media, F-row) | tutti i tasti producono il keycode atteso; formato compatibile con `boot_chord_gen.py` | M2.4 | M |
| M2.6 | `kb.c` minimo: `kb_send_*` → USB; switch se presenti | digitazione OK; Fn funziona; `CapsLock` LED via SET_REPORT | M2.5 | M |
| M2.9 | **Chord di recupero nell'applicazione**: prima di ogni init pesante (USB, radio, LED) `main()` legge la matrice e, se il chord è premuto, chiama `isp_jump()`. Va riusato lo scan del bootloader (`boot/main.c`, `chord_held`) | con il chord premuto all'accensione la scheda entra in ISP; test anche dopo un reset da watchdog; margine di tempo misurato | M2.4 | M |
| M2.7 | Piano di validazione hardware: test NKRO/rollover (es. tester online), repeat, tutti i tasti, hot-plug, suspend/resume dell'host | checklist compilata in `docs/keyboards/yunzii-b68.md` | M2.6 | M |
| M2.8 | Prima installazione via `sinowisp write` **se il bootloader stock è compatibile**; altrimenti saltare a M3 (full flash via ICP) | firmware SMK in esecuzione; rollback stock testato | M0.6 | M |

**Exit M2:** C1 ✔ (USB) e chord di recupero attivo (condizione per M4). Nessun codice Vial ancora.

> Nota di prudenza: quando il bootloader stock non è compatibile con `sinowisp write`, la prima installazione
> passa da ICP (M3.3). Prova sempre prima su `release` e tieni il dump a portata di mano.

---

## M3 — Bootloader SMK sulla B68

**Obiettivo:** recuperabilità senza ICP (chord all'accensione) e flash via `sinowisp`. **Opzionale** da quando
esiste il chord di recupero nell'applicazione (M2.9, D7); resta consigliata perché copre anche i bug *prima* di
`main()`. Richiede il programmatore ICP per il primo install.

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M3.1 | `boot_src` della B68 = matrice + `user_init` minimale; build `yunzii-b68_boot.hex` | `boot_check` OK; **codice ≤ 3840 B**; riporta il margine (R8) | M2.4 | M |
| M3.2 | Verifica chord: `boot_chord_gen.py` risolve S,M,K sul `layout.c`; se la B68 non ha quei tasti, scegli un chord alternativo (`-Dboot_chord=`) | build ok; il chord è raggiungibile a mano | M2.5 | S |
| M3.3 | **Primo install via ICP** (`yunzii-b68_default_full_flash`), con dump stock verificato (M0.5/M0.6) | tastiera si avvia nell'app; chord entra in ISP; `sinowisp read` coerente | M0.6, M3.1 | M |
| M3.4 | Round-trip `sinowisp`: `write` poi `read` e confronto; test di interruzione a metà scrittura (la scheda deve rientrare in ISP) | immagine identica; recovery dopo scrittura interrotta | M3.3 | M |
| M3.5 | Preset `sinowisp -d yunzii-b68` (upstream) oppure uso flag espliciti (`--isp_iface_num`, `--isp_report_id`, VID:PID) e aggiornamento del target `_flash` | `meson compile … yunzii-b68_default_flash` funziona | M3.4 | M |
| M3.6 | Test simulatore: aggiungi la scheda ai test boot (chord) se parametrizzabili | `meson test` verde | M3.2 | M |

**Exit M3:** C2 ✔.

---

## M4 — VIAL

**Obiettivo:** C3 e C4. Sottodivisa in spike (decidono il design), trasporto, protocollo, persistenza, sicurezza e validazione.

### 4.0 Spike (prima di scrivere il resto)

| ID | Spike | Domanda a cui risponde | Verifica | Eff. |
| --- | --- | --- | --- | --- |
| S1 | Esempio standalone `src/sino51lib/examples/usb_rawhid` (stesso stile degli esempi esistenti): interfaccia HID senza ID, EP2 IN+OUT, eco di report da 32 B | L'SIE gestisce OUT su EP2 a 32 B? Serve abilitare `OEP2IE`? Come si gestiscono NAK/ready? | script host (hidapi/Python) invia/riceve 1000 pacchetti senza perdite, su Linux e Windows | M |
| S2 | Misura durata `flash_erase` (512 B) e `flash_program_byte` con GPIO + analizzatore logico; osserva l'effetto su USB e sulla scansione | Quanto dura il blocco? È accettabile durante la digitazione? | numeri in `docs/keyboards/yunzii-b68.md`; regola: "scrivi solo in idle" se > ~1 ms | S |
| S3 | Leggi i sorgenti **vial-gui** e **vial-qmk** per fissare: sequenza all'apertura, comandi VIA obbligatori, formato di compressione della definizione, versione keycode (v6), regole di unlock | Elenco minimo di comandi e formati | documento `docs/vial-protocol-notes.md` (lo scriviamo insieme) | M |
| S4 | Datasheet: endurance della flash e tempi SSP (vedi [REFERENCE.md](REFERENCE.md)) | Quanti cicli di erase sopporta la keymap? | valore citato con fonte | S |

### 4.1 Trasporto USB (dopo S1, secondo D1)

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M4.1 | Ristrutturare i descrittori USB come da D1 (EP1 condiviso; iface Raw HID su EP2; ISP report su iface 0) | `lsusb -v` / `hid-tools` mostrano le interfacce attese; tastiera, media, NKRO, LED OUT ancora OK su Linux/Win/macOS | S1, M2.6 | L |
| M4.2 | Disaccoppia **NKRO USB** (≤ 13 B di bitmap) dal payload NKRO **radio** (20 B) — oggi condividono `NKRO_REPORT_BITS` | USB e RF possono avere dimensioni diverse; `rf_send_nkro` invariato | M4.1 | M |
| M4.3 | Console di debug: spezzare in chunk (≤ 15 B) o spostarla fuori da EP1 | `smk-console` continua a funzionare in `debug` | M4.1 | M |
| M4.4 | `raw_hid` in `src/smk/`: abilita `OEP2IE`, ISR **solo** copia 32 B + flag; `raw_hid_task()` nel main loop elabora e risponde su EP2 IN; politica di overrun (richiesta nuova prima della risposta) | `check_interrupts` OK; test sim (eco) verde; nessuna perdita a 1 kHz di richieste | M4.1 | L |
| M4.5 | Aggiornare `tests/test_usb*.py` per i nuovi descrittori/endpoint | `meson test` verde | M4.4 | M |

### 4.2 Protocollo VIA/Vial

Il parser va tenuto **puro** (nessun accesso a SFR): puoi compilarlo con `gcc` sul PC e provarlo con test C.

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M4.6 | `via.c`: dispatch comandi VIA (`id_get_protocol_version`, `get_keyboard_value`, `dynamic_keymap_*`, `id_unhandled` per i resto) e prefisso Vial `0xFE` | test host con vettori attesi (da vial-qmk) | S3, M4.4 | L |
| M4.7 | Accessor keymap: sostituire l'uso diretto di `keymaps[…]` in `matrix.c` con `keymap_get(layer,row,col)`; cache XRAM inizializzata dai default | nessuna regressione nei test; digitazione invariata | M2.6 | M |
| M4.8 | Dynamic keymap: `get/set_keycode`, `get/set_buffer`, `get_layer_count`, `reset`; endianness del buffer come in vial-qmk | Vial legge/scrive la keymap; byte-order verificato con S3 | M4.6, M4.7 | M |
| M4.9 | Definizione del layout: `vial.json` della B68 (+ `customKeycodes`) → script `utils/` che lo comprime e genera `const __code uint8_t[]` + UID a 8 byte; target Meson | Vial mostra layout e nome corretti | S3, M2.5 | L |
| M4.10 | Comandi Vial: `get_keyboard_id`, `get_size`, `get_def` (pagine da 32 B) | Vial carica la definizione senza errori | M4.9 | M |
| M4.11 | **Unlock**: `get_unlock_status/unlock_start/unlock_poll/lock` con combo di tasti (righe/colonne definite in config), legge `matrix[]` | senza unlock i comandi pericolosi sono rifiutati; unlock con combo funziona | M4.10 | M |
| M4.12 | Comandi distruttivi: `bootloader_jump` → `isp_jump()` (via `usb_task`, **non** dalla ISR), `eeprom_reset` → default; entrambi dietro unlock | prova manuale + log | M4.11 | S |

### 4.3 Persistenza

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M4.13 | Generalizzare `meson.build` a **N settori** per la keymap (`FLASH_KEYMAP_ADDR/SIZE`), `--code-size` sotto la regione; `_Static_assert` di non sovrapposizione con settings e marker | mappa stampata in `meson setup`; build fallisce se overlap | S2, S4 | M |
| M4.14 | `keymap_store`: formato (magic, versione, layer count, CRC), load al boot, fallback ai default su dato invalido | corruzione simulata ⇒ default; modifica ⇒ persiste dopo reset | M4.13 | L |
| M4.15 | Write-coalescing: flush dopo ≥ 2 s di inattività o a USB suspend; mai durante `scan`/pressione tasti; power-fail safety (A/B) | stacca il cavo a metà: la tastiera riparte con keymap valida (vecchia o nuova) | M4.14 | L |
| M4.16 | Interazione con `user_settings` esistenti (stesso `nvm`): regione separata, nessun cambio di formato | LED/rf_link continuano a persistere | M4.14 | M |

### 4.4 Keycode e layer

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M4.17 | Motore layer v1: `MO/TO/TG/DF` + **`OSL/OSM`** + trasparenza, fino a **8 layer**, **cache del keycode premuto per posizione** (rilascio coerente anche se il layer cambia) | test dedicato: premi, cambia layer, rilascia ⇒ nessun tasto bloccato | M4.7 | L |
| M4.18 | Rimappare i custom keycode di scheda su `QK_KB_x` e descriverli in `vial.json` | i tasti BT/LED compaiono nella GUI e funzionano | M4.9 | M |
| M4.19b | **Timebase al millisecondo** (`timer_read`) derivata dal Timer2 (o da SOF quando disponibile), prerequisito di `LT/MT` (D3 v2) | contatore monotono verificato con GPIO/analizzatore; nessuna deriva misurabile | M4.17 | M |
| M4.19 | Politica per keycode non supportati (⇒ `KC_NO`, nessun crash) e elenco in docs | mostrare keycode non supportati non rompe nulla | M4.17 | S |

### 4.5 Validazione

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M4.20 | Validazione con Vial GUI: rilevamento, layout, modifica tasti su 4 layer, matrix tester, export/import `.vil`, persistenza a replug, unlock | checklist in `docs/keyboards/yunzii-b68.md` | M4.1–M4.19 | M |
| M4.21 | Linux: regola udev per `hidraw`; Windows/macOS: verifica rilevamento | istruzioni in docs | M4.20 | S |
| M4.22 | Regressione: NKRO, media key, LED lock, `sinowisp write` e chord ISP ancora OK dopo la ristrutturazione USB | checklist verde | M4.20 | M |

**Exit M4:** C3 ✔, C4 ✔.

---

## M5 — LED / indicatori

**Obiettivo:** retroilluminazione e indicatori della B68 (la forma dipende da M1.3).

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M5.1 | Scegli la strategia: riuso di [`indicators.c`](../src/keyboards/nuphy-air60/layouts/default/indicators.c) (se RGB per-key a matrice) o implementazione minima (solo status LED/monocromatico) | decisione scritta | M1.3 | S |
| M5.2 | PWM/GPIO init in `user_init.c` (periodo, duty, drive) | LED accendibili da test | M5.1 | M |
| M5.3 | `indicators_*` + geometria (`led_rows/cols/cx2/cy2`) | effetti visibili; nessuna interferenza sulla scansione (scan e LED sono alternati, [§7](architecture.md#7-scheduling-timer2-e-tick)) | M5.2 | L |
| M5.4 | Salvataggio impostazioni LED in `user_settings` | persistono al reset | M5.3 | S |
| M5.5 | (opz.) Controllo LED da Vial: custom keycode (già) o VialRGB (M7) | — | M4.18 | M |

---

## M6 — Wireless (BK3632) e sleep

**Gate:** esito di M1.5 (R1). Se il protocollo diverge, si decide: reimplementare dal capture oppure rinviare.

| ID | Task | Verifica | Dip. | Eff. |
| --- | --- | --- | --- | --- |
| M6.1 | Pin SPI radio in `kbdef.h`/`user_init.c` (open-drain come nella Air60) | bit-bang pilota le linee come atteso (analizzatore logico) | M1.5 | M |
| M6.2 | `rf_init` + `rf_get_status`: il radio risponde `0xBB` con checksum valido | status letto, batteria/connessione plausibili | M6.1 | L |
| M6.3 | Invio report 6KRO/NKRO/extra al radio e prova su host reale (BT e 2.4G) | digitazione via radio; blanking corretto | M6.2 | L |
| M6.4 | Pairing/link switch (`LNK_*`), persistenza `rf_link`, factory reset bond | pairing riuscito, scambio fra 3 slot BT | M6.3 | M |
| M6.5 | `kb.c`: routing USB↔RF con slider, `kb_update` supervisor | cambio modalità a caldo senza stuck-key | M6.4 | M |
| M6.6 | **Sleep**: ricava dal firmware stock la sequenza di parcheggio dei pin (M1.6) per `user_sleep.c`; INT4 wake | corrente di sleep misurata; wake con tasto; nessuna perdita di link | M6.5 | L |
| M6.7 | Documentare: Vial non disponibile in RF (D4) | nota in docs | M6.5 | S |

---

## M7 — Estensioni VIAL (opzionali)

Una per volta, solo quando M4 è stabile e c'è margine di flash/XRAM (misurato):

* Macro dinamiche (comandi `macro_*` 0x0C–0x10; buffer in XRAM + persistenza).
* `LT/MT/OSM/OSL` (logica tap-hold con timer, vedi D3).
* `qmk_settings` (tap-hold timing, ecc.).
* Tap dance / combo / key override (`vial_dynamic_entry_op`).
* VialRGB (controllo LED standard da GUI).

---

## M8 — Hardening, documentazione, upstream

| ID | Task | Verifica |
| --- | --- | --- |
| M8.1 | Aggiungere `yunzii-b68` alla matrice CI e ai test simulati dove sensato | CI verde |
| M8.2 | `docs/keyboards/yunzii-b68.md` finale: specifiche, code option, pinout, descrittori stock, risultati di validazione | revisione di tutto il doc |
| M8.3 | README (tabella dispositivi), `docs/smk.md` (feature list con Vial) | link corretti |
| M8.4 | Review globale (`/code-review`), pulizia `dprintf`, rimozione codice morto, registro delle dimensioni finali | report dimensioni vs baseline |
| M8.5 | (opz.) PR upstream: decisione VID:PID (D6), licenza, descrizione | PR aperta |

---

## 15. Modalità di lavoro e review

**Ciclo per ogni task** (le specifiche dettagliate le preparo io quando si parte):

1. Io pubblico la **scheda del task**: contesto, file da toccare, API/vincoli, criteri di accettazione, test da portare come evidenza.
2. Tu implementi su un branch dedicato (un task ≈ un PR/commit coerente, stile dei messaggi del repo).
3. Mi passi il branch/diff; io faccio **review** classificando i rilievi: *blocker*, *major*, *minor*, *nit*, più "cosa è fatto bene".
4. Si chiude quando i criteri di accettazione e la checklist sono verdi.

**Checklist di review standard** (da allegare come evidenza ad ogni task):

* [ ] build `debug` **e** `release` (`meson compile -C build`), `-Werror` pulito
* [ ] `check_interrupts` OK (nessuna collisione di overlay ISR/main)
* [ ] `meson test` verde (se tocca USB/boot/stack)
* [ ] dimensioni (flash/XRAM/stack) prima/dopo riportate; bootloader ≤ 3840 B se toccato
* [ ] nessun `delay` lungo nel super-loop senza watchdog; nessun lavoro pesante in ISR
* [ ] test su hardware descritto (cosa, come, esito)
* [ ] niente binari/dump di terzi nel commit

**Principi di ingegneria** (che userò come criterio nelle review): ISR corte (copia + flag), stato condiviso
`volatile` con accessi atomici, nessuna assunzione sul timing flash, fallimenti sicuri (default su dati invalidi),
costanti con `_Static_assert` per i vincoli di layout, una funzione = una responsabilità, niente "magic number"
non documentati (soprattutto SFR/protocollo radio).

---

## 16. Domande aperte per te

Risolte (rev. 2): ICP opzionale (D7) · wireless richiesto ma dopo VIAL · 6KRO · 8 layer · Vial come adattatore (D8).

Ancora aperte:

1. L'**Arduino Nano** che hai scelto va bene se è un ATmega328P (vedi [recovery-icp.md §2](recovery-icp.md#2-larduino-che-hai-trovato-va-bene)): quando arriva, leggi la signature (M0.3).
2. Quale variante esatta della B68 (versione PCB, RGB per-key o no, 2.4G con dongle, batteria)? Foto del PCB e dell'etichetta.
3. Vuoi restare **solo personale** o puntare a una PR upstream (impatta D6 e la pulizia del codice)?
