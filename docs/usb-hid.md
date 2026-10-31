# Stack USB e report HID — guida e specifica

> Guida per chi conosce poco USB/HID e deve lavorare sullo stack di SMK. Ha tre parti:
> **(A)** concetti (cosa sono EP0, EP1, EP2, report, descrittori), **(B)** il catalogo di **ciò che SMK
> implementa oggi**, **(C)** il **design di destinazione** per VIAL e lo **spike S1**.
> Documenti collegati: [architecture.md §9](architecture.md#9-stack-usb), [porting-plan.md](porting-plan.md)
> (D1, M4), [REFERENCE.md §5](REFERENCE.md#5-usb-e-hid).
>
> Convenzioni: **[F]** = fatto letto nel codice · **[P]** = proposta/design non ancora implementato ·
> **[V]** = da verificare (sul datasheet, su hardware o nei sorgenti Vial).

---

## Indice

**Parte A — Concetti**
1. [USB in 10 minuti](#1-usb-in-10-minuti)
2. [EP0, EP1, EP2 sul SH68F90A](#2-ep0-ep1-ep2-sul-sh68f90a)
3. [HID in 10 minuti](#3-hid-in-10-minuti)

**Parte B — Quello che SMK implementa oggi**
4. [Topologia e descrittori](#4-topologia-e-descrittori)
5. [Catalogo dei report](#5-catalogo-dei-report)
6. [Richieste di controllo gestite](#6-richieste-di-controllo-gestite)
7. [Flussi di dati e architettura del codice](#7-flussi-di-dati-e-architettura-del-codice)
8. [Osservazioni e limiti noti](#8-osservazioni-e-limiti-noti)

**Parte C — Dove stiamo andando**
9. [Raw HID e protocollo VIA/Vial](#9-raw-hid-e-protocollo-viavial)
10. [Design di destinazione (D1)](#10-design-di-destinazione-d1)
11. [Cos'è uno "spike" e cos'è lo spike S1](#11-cosè-uno-spike-e-cosè-lo-spike-s1)

**Appendici**
12. [Strumenti per ispezionare l'USB](#12-strumenti-per-ispezionare-lusb)
13. [Glossario](#13-glossario)

---

# Parte A — Concetti

## 1. USB in 10 minuti

**Il bus è guidato dall'host.** Il PC (host) è l'unico che inizia le comunicazioni: la tastiera (device) non può
"mandare" niente da sola, può solo **rispondere quando l'host la interroga**. Un tasto premuto non parte
immediatamente: resta pronto nel buffer finché l'host, entro 1 ms (il `bInterval`), non lo legge.

### 1.1 L'albero dei descrittori

All'inserimento (**enumerazione**) l'host chiede al device di descriversi con dei **descrittori**:

```
Device descriptor          VID:PID, classe, dimensione pacchetto EP0, n° configurazioni
└─ Configuration           potenza richiesta, n° interfacce
   ├─ Interface 0          "una funzione" del device (es. la tastiera)
   │   ├─ HID descriptor   puntatore al Report Descriptor + lunghezza
   │   └─ Endpoint(s)      i canali dati di questa funzione
   └─ Interface 1          un'altra funzione (es. tasti media)
       ├─ HID descriptor
       └─ Endpoint(s)
```

Un **device composito** (come una tastiera) espone più interfacce; l'OS può caricare un driver diverso per
ognuna. Per le tastiere HID il driver è quello generico "HID".

### 1.2 Endpoint

Un **endpoint** è un canale logico dentro il device, con **numero** (0–15) e **direzione**.

* La direzione è **sempre dal punto di vista dell'host**: **IN** = device → host (la tastiera *manda* dati), **OUT** =
  host → device (la tastiera *riceve*).
* L'indirizzo si scrive `0x81` = "EP1, IN" (bit 7 = IN), `0x02` = "EP2, OUT".
* Ogni endpoint ha un **tipo di trasferimento** e una **dimensione massima del pacchetto** (`wMaxPacketSize`).

| Tipo | Per cosa | Garanzie |
| --- | --- | --- |
| **Control** | configurazione, enumerazione, richieste speciali (solo EP0) | consegna garantita, più lenta |
| **Interrupt** | piccoli dati periodici a bassa latenza (tastiere, mouse) | l'host interroga ogni `bInterval` ms |
| Bulk | grandi dati senza urgenza (dischi) | consegna garantita, senza tempi |
| Isochronous | audio/video | tempo garantito, nessun reinvio |

**"Interrupt" in USB non è un interrupt hardware**: è un trasferimento in cui l'host *fa polling* (chiede) con
periodo fisso; il device risponde con dati oppure **NAK** ("niente da dire").

### 1.3 Pacchetti e transazioni

Una **transazione** = token (l'host dice "SETUP/IN/OUT verso EP n") + dati + handshake (ACK/NAK/STALL). I dati
sono spezzati in pacchetti al massimo `wMaxPacketSize`. Un trasferimento è terminato da un pacchetto **più corto**
del massimo (o da uno vuoto, "ZLP").

---

## 2. EP0, EP1, EP2 sul SH68F90A

[F] [`sh68f90.h` righe 824–837](../src/sino51lib/sh68f90/sh68f90.h) definisce i buffer degli endpoint nella
"finestra USB" in XRAM:

| Endpoint | Buffer OUT (host→device) | Buffer IN (device→host) | Dimensione | Uso oggi in SMK |
| --- | --- | --- | --- | --- |
| **EP0** | `0x1100` | `0x1108` | **8 B** | Control pipe (vedi sotto) |
| **EP1** | `0x1110` | `0x1120` (+ `COM` `0x1130`) | **16 B** | **IN**: tastiera 6KRO. OUT: non usato |
| **EP2** | `0x1140` | `0x1180` (+ `COM` `0x11C0`) | **64 B** | **IN**: media/NKRO/console. OUT: non usato |

Conseguenze:

* Il chip ha **tre endpoint**: EP0 + **due** con direzione IN e OUT. In pratica puoi avere **al massimo due canali
  dati in uscita (IN) oltre al control**. È il vincolo che pilota tutto il design.
* Cosa faccia il buffer **`COM`** (probabilmente un buffer comune/di ping-pong, e c'è un bit `…BUFSEL` nei
  registri `EPxCON`) **[V]** si legge sul datasheet.

### 2.1 EP0 — l'endpoint di controllo

* **Sempre presente**, indirizzo 0, **bidirezionale** (IN e OUT), non va dichiarato nei descrittori.
* Serve per tutto ciò che *non* ha un canale dedicato: enumerazione, comandi standard, comandi di classe HID.
* Un trasferimento di controllo ha tre fasi:

```
SETUP stage   host → device   8 byte: bmRequestType, bRequest, wValue, wIndex, wLength
DATA stage    (opzionale)     IN o OUT, a pacchetti da 8 B (qui bMaxPacketSize0 = 8)
STATUS stage  direzione opposta, pacchetto vuoto: "ho finito, tutto ok"
```

* In SMK la macchina a stati di EP0 è in [usb.c](../src/smk/usb.c) (`EP0_STAGE_IDLE → DATA_IN → STATUS_OUT`,
  `DATA_OUT → STATUS_IN`).
* **Con 8 B di pacchetto i trasferimenti sono lenti** e un report da 32 B richiede 4 pacchetti. Va bene per
  configurare, non per il flusso continuo dei tasti.

### 2.2 EP1 ed EP2

Sono endpoint **interrupt** dichiarati nei descrittori:

* **EP1 IN (0x81)**: 16 B, `bInterval` 1 ms → la tastiera standard.
* **EP2 IN (0x82)**: 64 B, `bInterval` 1 ms → tasti media/system, NKRO, console di debug.
* Entrambi hanno anche un lato **OUT** nel silicio, **mai usato oggi**: è il candidato per ricevere i comandi di
  Vial (spike S1).

Il firmware **non** usa gli interrupt di completamento dell'IN (`usb_hw_epN_in_complete` è vuoto): scrive nel
buffer, imposta `IEPxRDY` e, prima della scrittura successiva, aspetta (max ~10 ms) che il bit si azzeri. L'ISR
USB abilitata copre solo EP0 (`USBIE2 = OEP0IE | IEP0IE`).

---

## 3. HID in 10 minuti

**HID** (Human Interface Device) è la classe USB di tastiere, mouse, gamepad. L'idea: il device non invia "il tasto
A è premuto" in un formato fisso, ma **descrive il proprio formato** e poi manda blocchi di byte conformi.

### 3.1 Report e Report Descriptor

* Un **report** è un blocco di byte con un significato definito. Tre tipi:
  * **Input**: device → host (stato dei tasti), di solito sull'endpoint interrupt IN.
  * **Output**: host → device (es. i LED Caps/Num Lock), via EP0 (`SET_REPORT`) o, se esiste, un endpoint OUT.
  * **Feature**: configurazione bidirezionale **sempre via EP0** (`GET/SET_REPORT`), non a flusso.
* Il **Report Descriptor** è un piccolo "linguaggio" di item (ad es. *Usage Page*, *Report Size*, *Report Count*,
  *Input*, *Output*, *Feature*) che dice all'host: "i primi 8 bit sono i modificatori, poi 1 byte riservato, poi
  6 byte di tasti …". L'host lo legge una volta e poi sa decodificare ogni report.
* **Usage Page / Usage** dicono *che cosa è* ogni campo: pagina 0x01/usage 0x06 = Keyboard; pagina 0x0C = Consumer;
  pagina `0xFF00–0xFFFF` = **vendor-defined** (significato nostro). Le tabelle sono le *HID Usage Tables*
  ([REFERENCE §5](REFERENCE.md#5-usb-e-hid)).
* **Collection (Application)**: raggruppa i campi di una funzione (es. "tastiera"). Windows/Linux trattano ogni
  collection di primo livello come un dispositivo HID separato.

### 3.2 Report ID

Se un'interfaccia porta **più report diversi**, ognuno ha un **Report ID** (1 byte) messo **come primo byte** di ogni
trasferimento. Regola HID: se *un solo* report usa gli ID, **tutti** i report di quel descrittore devono usarli.
Un'interfaccia senza ID manda il report "nudo". Questo è il motivo per cui il **Raw HID di Vial** (senza ID) non può
convivere con report con ID nella stessa interfaccia.

### 3.3 Boot protocol

Le tastiere HID possono dichiarare un sottotipo **Boot** e supportare il **boot protocol**: un formato fisso da 8 byte
(mods, riservato, 6 tasti) che BIOS/UEFI/bootloader sanno leggere **senza** interpretare il descrittore. L'host passa da
un protocollo all'altro con `SET_PROTOCOL` (0 = boot, 1 = report).

### 3.4 Esempio: leggere il report tastiera

Il report da 8 byte di SMK ([report.h](../src/smk/report.h)):

```
byte:   0        1         2     3     4     5     6     7
        mods     reserved  key0  key1  key2  key3  key4  key5
bit 0 = Left Ctrl, 1 = Left Shift, 2 = Left Alt, 3 = Left GUI,
    4 = Right Ctrl, 5 = Right Shift, 6 = Right Alt, 7 = Right GUI
```

Premendo `A` (usage 0x04): `00 00 04 00 00 00 00 00`. Premendo `Shift+A`: `02 00 04 00 00 00 00 00`.
Questo è "6KRO": al massimo **6 tasti non modificatori** contemporaneamente (i modificatori sono nel byte 0).

---

# Parte B — Quello che SMK implementa oggi

Tutto qui sotto è **[F]**: letto da [usb.c](../src/smk/usb.c), [report.h](../src/smk/report.h),
[host.c](../src/smk/host.c), [usbhw.c](../src/sino51lib/sh68f90/usbhw.c).

## 4. Topologia e descrittori

```
Device         USB 2.0, bMaxPacketSize0 = 8, VID:PID per scheda (es. NuPhy 05ac:024f), 1 configurazione
Configuration  2 interfacce, self/bus-powered con remote wakeup, bMaxPower 250 (500 mA)
│
├─ Interface 0  "Keyboard"    HID, subclass BOOT (1), protocol 1 (keyboard)
│   └─ EP1 IN  (0x81)  interrupt  maxpkt 16  interval 1 ms
│       Report descriptor: tastiera boot-compatibile, SENZA Report ID
│
└─ Interface 1  "Extra"       HID, subclass none, protocol none
    └─ EP2 IN  (0x82)  interrupt  maxpkt 64  interval 1 ms
        Report descriptor: più collection, TUTTE con Report ID
```

Stringhe: manufacturer, product, serial (`USB_STR_*`, sovrascrivibili da Meson).

## 5. Catalogo dei report

### 5.1 Interfaccia 0 — Tastiera (EP1, nessun Report ID)

| Report | Dir. | Trasporto | Dimensione | Layout |
| --- | --- | --- | --- | --- |
| **Keyboard Input** | IN | EP1 IN | 8 B | `[mods][0][k0][k1][k2][k3][k4][k5]` — vedi §3.4 |
| **LED Output** | OUT | EP0 `SET_REPORT` (Output), iface 0, `wLength = 1` | 1 B | bit0 Num, bit1 Caps, bit2 Scroll, bit3 Compose, bit4 Kana, bit5–7 padding → `keyboard_set_led_state()` |

### 5.2 Interfaccia 1 — Extra (EP2, con Report ID)

| ID | Report | Dir. | Trasporto | Dimensione | Layout / note |
| --- | --- | --- | --- | --- | --- |
| **1** | System Control | IN | EP2 IN | 1+1 nel descrittore | bit0 Power Down, bit1 Sleep, bit2 Wake Up (+5 bit di padding). Vedi §8 (osservazione sui 3 byte inviati) |
| **2** | Consumer Control | IN | EP2 IN | 1+2 | un usage a 16 bit (`0x0000–0x023C`): volume, play/pausa, luminosità, … (tabella in `report.h`) |
| **5** | **ISP command** | OUT/Feature | EP0 `SET_REPORT` (Feature), iface 1 | 5 B (+ID) | `[5][0x75]…` → richiede il salto nel bootloader (`usb_isp_requested`, eseguito da `usb_task`). Solo se `ISP_ENABLE` |
| **6** | **NKRO Keyboard** | IN | EP2 IN | 22 B | `[6][mods][20 B bitmap]`: bit *n* = usage *n* (0–159). Solo se `NKRO_ENABLE` |
| **7** | **Console** (debug) | IN (+Feature OUT) | EP2 IN / EP0 | 1+32 B | testo di debug (`dprintf`) letto da `smk-console`. Solo `DEBUG` |

Formati usati in firmware:

```c
// report.h
report_keyboard_t : raw[8]  { mods, reserved, keys[6] }
report_nkro_t     : raw[22] { report_id, mods, bits[20] }
report_extra_t    : raw[3]  { report_id, usage (u16 LE) }
```

## 6. Richieste di controllo gestite

[F] `usb_setup_irq()` smista per tipo:

| Tipo | Richieste implementate |
| --- | --- |
| **Standard** | `GET_STATUS`, `CLEAR_FEATURE`, `SET_FEATURE`, `SET_ADDRESS`, `GET_DESCRIPTOR` (device, configuration, string, HID, HID report), `GET_CONFIGURATION`, `SET_CONFIGURATION`, `GET_INTERFACE`, `SET_INTERFACE` |
| **Class (HID)** | `SET_REPORT`, `SET_IDLE`, `GET_IDLE`, `SET_PROTOCOL`, `GET_PROTOCOL` |
| Altro | `STALL` |

Note:

* **`GET_REPORT` non è implementata** (viene risposto con STALL). Gli host moderni non ne hanno bisogno per una
  tastiera, ma sarà richiesta per eventuali feature report in lettura.
* L'interfaccia deve essere 0 o 1: `iface > 1` ⇒ STALL (andrà esteso per una terza interfaccia).
* `ep0_receive()` accetta al massimo **8 B** (`EP0_BUF_SIZE`) di dati OUT: sufficiente per LED e ISP, **insufficiente
  per un feature report da 32–64 B** senza estendere la macchina a stati (rilevante per il piano B di Vial).
* `SET_PROTOCOL` salva il protocollo per interfaccia; NKRO è attivo solo se `protocol == REPORT` **e**
  `keymap_config.nkro`.

## 7. Flussi di dati e architettura del codice

### 7.1 Tasto → host

```
matrix_task() → process_key_state() → send_keycode() → send_keyboard_report()
   → report.c  (send_6kro_report / send_nkro_report, solo se il report cambia)
   → host.c    (host_keyboard_send / host_nkro_send / host_extra_send)
   → kb_send_*()  [della scheda: USB oppure RF]
   → usb.c     (usb_send_report / usb_send_nkro / usb_send_extra)
   → usbhw.c   (usb_hw_ep1_in_send / usb_hw_ep2_in_send):
        attende buffer libero → copia nel buffer IN → imposta il contatore → IEPxRDY
   → l'host, al prossimo poll (≤ 1 ms), legge il pacchetto
```

### 7.2 Host → device (LED, ISP)

```
SETUP (SET_REPORT) ─► ISR USB ─► usb_setup_irq() ─► ep0_receive() ─► pacchetto OUT
   ─► usb_ep0_out_irq() ─► switch(ep0_out_target): LED | ISP | CONSOLE
ISP:  ep0_isp_pending=1 ─► al STATUS stage ─► usb_isp_requested=1
      ─► main loop: usb_task() ─► isp_jump()      (mai dalla ISR)
```

### 7.3 Strati

```
usb.c (smk)      descrittori, macchina a stati EP0, richieste standard e HID, API usb_send_*
   │ chiama
usbhw.c (sh68f90) accesso ai registri USB: init, send su EP1/EP2, buffer
   │ usa
sh68f90.h / usbhw.h   SFR, bit, buffer __xdata __at(...)
```

L'ISR (`usb_interrupt_handler`) salva `INSCON`/`FLASHCON`, chiama `usb_irq_dispatch()` e li ripristina.

## 8. Osservazioni e limiti noti

Sono rilievi dalla lettura del codice, utili quando lo tocchi:

1. **System Control inviato con 3 byte.** `usb_send_extra()` invia sempre `EXTRA_REPORT_SIZE = 3` (ID + usage a 16 bit)
   anche per il Report ID 1, che nel descrittore ha 1 byte di dati (3 bit + 5 padding). Il byte in più è presumibilmente
   ignorato dall'host, ma è una discrepanza da verificare con un analizzatore **[V]**.
2. **`GET_REPORT` assente** (§6).
3. **Un solo IN in volo per endpoint, con attesa attiva** (fino a ~10 ms) se il buffer non è ancora stato letto:
   durante un blocco prolungato del bus il main loop rallenta.
4. **Il protocollo (boot/report) è memorizzato ma non influenza il formato dei report 6KRO**: il 6KRO è sempre a 8 B
   senza ID, il che è corretto per l'interfaccia 0 di oggi, ma cambierà con l'EP condiviso (§10).
5. **Il report descriptor dell'interfaccia 0 ha un commento fuorviante** (`// System Control` sull'usage 0x06, che è
   Keyboard): è solo un commento.
6. **Nessun endpoint OUT in uso**, nessun interrupt EP1/EP2 abilitato: l'ISR va estesa per Raw HID.

---

# Parte C — Dove stiamo andando

## 9. Raw HID e protocollo VIA/Vial

### 9.1 Cos'è

Vial parla con la tastiera tramite un'interfaccia HID **"raw"** (vendor-defined):

* **Usage Page `0xFF60`**, **Usage `0x61`** (valori di default di QMK, [QMK Raw HID](https://docs.qmk.fm/features/rawhid) ✔),
* **nessun Report ID**,
* **report da 32 byte, sempre**, in entrambe le direzioni (anche se il contenuto utile è più corto).

La GUI individua il dispositivo cercando tra le interfacce HID quella con quella usage page/usage; come lo
distingua da altre tastiere QMK (probabilmente dal numero di serie) **[V — S3]**.

### 9.2 Come passano i dati

```
Host → device   write 32 B   ─► (EP OUT dell'interfaccia raw, oppure SET_REPORT su EP0 se non esiste)
Device → host   read  32 B   ─► EP IN dell'interfaccia raw (interrupt)
```

Il modello è **richiesta/risposta**: l'host scrive un comando di 32 B e legge una risposta di 32 B (il firmware
sovrascrive lo stesso buffer, come in `vial.c`). Poiché la GUI usa la libreria `hidapi`, la lettura funziona solo se
l'interfaccia ha un **endpoint IN** dedicato: è il motivo per cui il Raw HID ha bisogno di un endpoint tutto suo.

### 9.3 Formato dei pacchetti (VIA + Vial)

Dai sorgenti `vial-qmk` letti in questa sessione ([REFERENCE §4.2](REFERENCE.md#42-sorgenti-da-leggere-sono-la-specifica-de-facto)) ✔:

```
byte 0        comando VIA (0x01 get_protocol_version, 0x02 get_keyboard_value, 0x04 get_keycode, 0x05 set_keycode, …)
              0xFE = prefisso Vial  →  byte 1 = sotto-comando Vial (0x00 get_keyboard_id, 0x01 get_size, 0x02 get_def, …)
byte 1…31     argomenti / dati della risposta (i comandi sovrascrivono lo stesso buffer)
```

Esempio — risposta a `get_keyboard_id` (`FE 00`): byte 0–3 = `VIAL_PROTOCOL_VERSION` (u32 little-endian, oggi 6),
byte 4–11 = UID della tastiera (8 B), byte 12 = flag VialRGB. La definizione del layout (JSON compresso) si
scarica a pagine da 32 B (`get_def`, numero di pagina in byte 2–3). Il catalogo completo dei comandi sarà in un
documento dedicato (`vial-protocol-notes.md`, task S3).

## 10. Design di destinazione (D1)

**[P]** Decisioni già prese ([porting-plan.md §4](porting-plan.md#4-decisioni-architetturali-aperte)): wired **6KRO** (niente
NKRO via cavo), Vial-compatibile, 8 layer. Il design conserva **due endpoint dati**:

```
Interface 0  "Keyboard (shared)"      EP1 IN  (0x81, 16 B)     report CON Report ID
   ├─ ID k   Keyboard 6KRO   (8 B + ID = 9 B)
   ├─ ID 1   System Control
   ├─ ID 2   Consumer Control
   └─ ID 5   ISP command (feature, via EP0)   ← spostato qui dalla vecchia interfaccia 1 [V: preset sinowisp]
     Boot protocol: il report tastiera viaggia SENZA ID (come fa QMK con "shared endpoint").

Interface 1  "Raw HID (Vial)"          EP2 IN (0x82) + EP2 OUT (0x02), pacchetto 64 B, report da 32 B
   └─ usage page 0xFF60 / usage 0x61, nessun Report ID, 32 B in, 32 B out

EP0:   enumerazione, LED output report, SET/GET_IDLE, SET/GET_PROTOCOL, feature report ISP
```

Cosa cambia nel codice:

| Area | Modifica |
| --- | --- |
| Descrittori (`usb.c`) | nuova interfaccia raw; report descriptor condiviso con ID; `bNumInterfaces` 2→2 ma con ruoli diversi |
| `usb_send_*` | tutti su EP1; il report tastiera va con o senza ID a seconda di `protocol` |
| ISR USB | abilitare `OEP2IE`; gestire `_OEP2IF`; copiare 32 B in un buffer + flag (nessuna elaborazione in ISR) |
| `usb_hid_request` | accettare `iface 0/1` secondo la nuova numerazione; eventualmente `GET_REPORT` |
| Debug console | non entra più nei 16 B di EP1 (33 B): va spezzata in chunk o spostata |
| Test (`tests/test_usb*.py`) | nuovi descrittori/endpoint |

**Se S1 fallisce** (cioè l'OUT su EP2 non funziona come serve): piano B = canale di configurazione via **feature
report su EP0** (nessun endpoint in più) con configuratore WebHID nostro; richiede di estendere `ep0_receive` oltre
gli 8 B. Il nucleo (layer, keymap, persistenza) resta identico.

## 11. Cos'è uno "spike" e cos'è lo spike S1

**Spike** (termine di Extreme Programming): un esperimento **piccolo, a tempo limitato e usa-e-getta** per rispondere
a una **domanda tecnica rischiosa** *prima* di impegnarsi nel design. Non produce funzionalità: produce **conoscenza**
e una decisione. Si scrive il minimo codice che risponde alla domanda, poi si decide e (di solito) si butta via.

### 11.1 La domanda di S1

> *Il blocco USB del SH68F90A riesce a ricevere e a rispondere con report da **32 byte** su **EP2 in modalità
> IN+OUT**, in modo affidabile, con un'interfaccia HID **senza Report ID** riconosciuta da `hidapi` su Linux e Windows?*

Se la risposta è **sì**, D1 (EP2 = Raw HID) è confermato. Se è **no**, scattiamo sul piano B.

### 11.2 Cosa si costruisce

Un **firmware d'esempio** minimo nello stile di [`src/sino51lib/examples/`](../src/sino51lib/examples) (come
`usb_minimal`), chiamato ad esempio `usb_rawhid`:

* un'interfaccia HID con usage page `0xFF60`/usage `0x61`, 32 B in + 32 B out, senza ID;
* EP2 IN+OUT configurati e interrupt OUT abilitato;
* **eco**: ciò che l'host scrive (32 B) viene rispedito con il primo byte incrementato.

Non include tastiera, LED, radio. Si installa sulla B68 (con il chord di recupero attivo!), poi si ripristina SMK/stock.

### 11.3 Come si prova (lato PC)

Script Python con `hidapi` (modulo `hid`) **[bozza]**:

```python
import hid, time
dev_info = next(d for d in hid.enumerate() if d["usage_page"] == 0xFF60 and d["usage"] == 0x61)
d = hid.device(); d.open_path(dev_info["path"])
for i in range(1000):
    payload = bytes([i & 0xFF]) + bytes(range(31))               # 32 byte di dati
    d.write(b"\x00" + payload)                                   # 1 byte "Report ID 0" (= nessun ID) + 32 byte
    resp = bytes(d.read(32, timeout_ms=100))
    assert resp[0] == (i + 1) & 0xFF and resp[1:] == payload[1:], (i, resp)
```

### 11.4 Criteri di successo

* Il dispositivo si enumera su **Linux** e **Windows** senza errori, e `hid.enumerate()` mostra l'interfaccia raw
  con usage page/usage corretti.
* **1000 scambi consecutivi** senza perdite né risposte sbagliate, anche mentre si digita su un'altra interfaccia
  (se il firmware la include) o con un host sotto carico.
* Latenza per scambio misurata (attesa ≲ qualche ms).
* Comportamento documentato per: richiesta nuova prima della risposta precedente, NAK, riconnessione.

### 11.5 Cosa si decide

| Esito | Decisione |
| --- | --- |
| Tutti i criteri OK | confermato D1: procediamo con M4.1–M4.4 |
| OUT su EP2 non affidabile / non funzionante | piano B (feature report su EP0 + configuratore WebHID); aggiornare il piano |
| Problemi solo con Windows | indagare sul descrittore (non cambia il design) |

Documento di output: i numeri e le osservazioni confluiscono in `docs/keyboards/yunzii-b68.md` e nell'aggiornamento
di [porting-plan.md](porting-plan.md).

---

## 12. Strumenti per ispezionare l'USB

| Cosa vuoi vedere | Strumento |
| --- | --- |
| Descrittori completi (Linux) | `lsusb -v -d VID:PID` (in WSL2 serve `usbipd attach`, vedi [recovery-icp.md §7](recovery-icp.md#7-wsl2-far-vedere-le-periferiche-usb-a-linux)) |
| Il report descriptor HID decodificato | `usbhid-dump -d VID:PID` · `hid-recorder /dev/hidrawN` (pacchetto `hid-tools`) |
| I report mentre li invii | `hid-recorder`, `sudo cat /dev/hidraw0 \| xxd` |
| Traffico USB reale | Wireshark + usbmon (Linux) o USBPcap (Windows) |
| Enumerazione da Python | `hid.enumerate()` (modulo `hidapi`) |
| Tester online del rollover | siti di "keyboard tester" (utile per 6KRO) |
| Test nel simulatore | `tests/test_usb.py`, `test_usb_ep0.py` |

## 13. Glossario

| Termine | Significato |
| --- | --- |
| **Host / Device** | PC / tastiera |
| **Enumerazione** | fase in cui l'host legge i descrittori e configura il device |
| **Descrittore** | struttura che descrive device, configurazione, interfaccia, endpoint o report |
| **Interface** | una "funzione" logica del device; ha il suo driver sull'host |
| **Endpoint (EP)** | canale dati con numero e direzione; EP0 è quello di controllo |
| **IN / OUT** | direzione vista dall'host: IN = device→host, OUT = host→device |
| **Control / Interrupt transfer** | trasferimento di configurazione / polling periodico a bassa latenza |
| **SETUP** | primo pacchetto di un trasferimento di controllo (8 byte) |
| **NAK / ACK / STALL** | "non ho dati" / "ricevuto" / "richiesta non supportata o errore" |
| **ZLP** | zero-length packet: pacchetto vuoto che chiude un trasferimento |
| **HID** | classe USB per dispositivi di input |
| **Report** | blocco di byte con formato descritto dal Report Descriptor |
| **Report ID** | primo byte che distingue più report sulla stessa interfaccia |
| **Input / Output / Feature** | report device→host / host→device / configurazione via EP0 |
| **Usage Page / Usage** | categoria e significato di un campo (es. 0x01/0x06 = tastiera) |
| **Collection** | gruppo di campi che formano una funzione (es. "Application") |
| **Boot protocol** | formato fisso della tastiera leggibile da BIOS senza parsing |
| **6KRO / NKRO** | 6 tasti contemporanei / numero illimitato (bitmap) |
| **Raw HID** | interfaccia HID vendor-defined a pacchetti fissi, usata da QMK/VIA/Vial |
| **VIA / Vial** | protocolli di configurazione della tastiera sopra Raw HID |
| **SIE** | Serial Interface Engine: la parte di hardware che gestisce il protocollo USB nel chip |
| **Spike** | esperimento breve per rispondere a una domanda tecnica prima del design |
| **ICP / ISP** | programmazione tramite pin dedicati / tramite bootloader su USB (vedi [recovery-icp.md](recovery-icp.md)) |
