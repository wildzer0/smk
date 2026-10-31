# Yunzii B68

> Stato: **porting in corso** (branch `port/yunzii-b68`). Piano: [porting-plan.md](../porting-plan.md).
> Task: [tasks/](../tasks/README.md). Questo file raccoglie i fatti verificati sulla tastiera;
> le sezioni vuote si riempiono nelle milestone indicate.

## Specs

- MCU: BYK916 (SH68F90A) — *da confermare fisicamente (M0.4)*
- Wireless: BK3632 (BT + 2.4G) — *il protocollo potrebbe differire da NuPhy (rischio R1)*
- Matrice, LED, indicatori, switch: *da rilevare (M0.4 / M2)*

## Ambiente (M0.1, chiuso)

| Strumento | Versione |
| --- | --- |
| SDCC | 4.5.0 (#15242) |
| Meson / Ninja | 1.10.2 / 1.13.2 |
| gcc (host) | 15.2.0 |
| ucsim-sh68f90 | 4.5.0 (patchato) |
| Commit di partenza | `dbd5c54` |

Ambiente fornito da `nix develop` (`use flake`). Build dir: `build/` (release), `build-debug/` (debug).

## Baseline di build (nuphy-air60, target di riferimento)

| Immagine | Build | XRAM usata | Flash usata | Stack libero |
| --- | --- | --- | --- | --- |
| firmware `default_smk` | release | 1053 B / 4096 | 21415 B / 60416 | 222 B |
| firmware `default_smk` | debug | 1234 B / 4096 | 24764 B / 60416 | 222 B |
| bootloader `boot` | release/debug | 96 B / 4096 | 3773 B / 3840 | 174 B |

Il bootloader ha ~67 B di margine. Altri target (release): example 11874 B flash, eyooso-z11 14129 B,
genesis-thor-300 13999 B, ciy-x77 15080 B. Le cifre combaciano con [architecture.md §17](../architecture.md).

## Esito dei controlli

| Controllo | Release | Debug |
| --- | --- | --- |
| `boot_check` (4 bootloader) | OK | OK |
| `check_interrupts` (6 firmware) | OK, nessuna collisione di overlay | OK |
| `meson test` (75 test) | 74 OK, 1 skipped, **1 errore noto** | non eseguita la suite completa |

**Errore noto:** `test_boot.TestFullBoot.test_boot_runs_full_init_to_main_loop` fallisce sulla build release
con `KeyError: 'console$console_buf not found in .map'`, perché la console esiste solo nella build debug.
Non è un difetto del firmware. Con la build debug il test passa:

```sh
cd tests && SMK_UCSIM=<ucsim-sh68f90> \
  SMK_FIRMWARE=$PWD/../build-debug/nuphy-air60_default_smk.hex \
  python3 -m unittest test_boot
```

Il report grezzo di M0.1 è in [tasks/M0.1-report.txt](../tasks/M0.1-report.txt).

## USB stock (M1.7, 2026-10-31)

Dati grezzi in [tasks/M1.7-report.txt](../tasks/M1.7-report.txt).

**Tastiera cablata:** `258a:010c`, produttore "BY Tech", prodotto "Gaming Keyboard", nessun seriale, bcdDevice 0x2002,
full-speed, 500 mA, 3 endpoint (EP0 + 2).

| Interfaccia | Classe | Endpoint | Contenuto |
| --- | --- | --- | --- |
| 0 | HID boot, protocollo 1 (tastiera) | EP1 IN 8 B, 1 ms | Tastiera 6KRO standard + 5 LED in uscita (via EP0) |
| 1 | HID, nessun boot | EP2 IN 16 B, 1 ms | Report con ID: 1 system control (3 bit), 2 consumer (16 bit), 3 vendor (3 B), 4 NKRO (120 bit, usage 4–0x70), 5 **feature 5 B** (ISP), 6 vendor: input 7 B + **feature 519 B**, 7 mouse (5 pulsanti, X/Y 16 bit, wheel, pan) |

Osservazioni utili al porting:

* La struttura è **la stessa di SMK/NuPhy** (EP1 8 B boot + EP2 16 B composito, ISP = feature report 5 sull'interfaccia 1),
  per cui il preset di sinowisp `--isp_iface_num=1 --isp_report_id=5` è confermato.
* **Non esistono endpoint OUT**: i LED e tutta la configurazione viaggiano su EP0 (SET_REPORT/GET_REPORT). Il
  software del produttore usa il **feature report ID 6 da 519 byte** come canale di configurazione: prova pratica che
  il canale a feature report via EP0 (piano B di D8 e spike S1) funziona su questo chip.
* La mia ipotesi D1 (EP2 IN+OUT come Raw HID da 32 B per VIA/Vial) resta da validare con lo spike S1: lo stock non usa
  EP OUT e non dimostra che funzionino.
* Il mouse report (ID 7) è presente: se lo vogliamo (mousekeys) possiamo copiare questo layout.

**Dongle 2.4G (non richiesto, utile):** `3554:FA09`, produttore "Compx", prodotto "2.4G Wireless Receiver"; low-speed;
interfaccia 0 boot tastiera con EP1 IN 8 B (inclusa una riga vendor), interfaccia 1 boot mouse con EP2 IN 25 B e un
feature report vendor ID 6 da 7 B. È un dispositivo separato: la tastiera in modalità 2.4G parla con il dongle via BK3632,
non con il PC.

**Preset sinowisp:** non verificati. Il test è stato lanciato in modo errato (`yunizii-al66` con refuso e senza il
sottocomando `read`). Non serve: con `--platform sh68f90 --vendor_id 0x258a --product_id 0x010c` sinowisp funziona già.

## Matrice (probe, 2026-10-31)

Misurata sulla scheda con il layout `probe` (ogni tasto digita `cNNrR`), tasti premuti riga per riga da sinistra. **Conferma
l'ipotesi ricavata dal firmware stock**: 16 colonne × 5 righe, tutti i tasti rispondono sulle celle previste. Risultato
(colonna, riga) per ogni tasto fisico, da sinistra a destra:

| Riga | Colonne dei tasti presenti | Tasti |
| --- | --- | --- |
| 0 | 0–13, 15 | Esc 1 2 3 4 5 6 7 8 9 0 - = Backspace · c15 |
| 1 | 0–13, 15 | Tab Q W E R T Y U I O P [ ] \ · c15 |
| 2 | 0–11, 13, 15 | Caps A S D F G H J K L ; ' · Enter · c15 |
| 3 | 0–10, 13, 14, 15 | Shift Z X C V B N M , . / · Shift · Su · c15 |
| 4 | 0, 1, 2, 5, 8, 9, 13, 14, 15 | Ctrl Win Alt · Spazio · Alt Fn · Sx Giù Dx |

Sono **67 celle**: 66 tasti più la **pressione dell'encoder rotativo** (`c15r0`). Non manca nessun tasto (la riga 4 ha 9
celle). Il layout `default` e `vial.json` usano le larghezze di una 65% ANSI; sotto l'encoder, `c15r1..r3` sono assunti
Del, PgUp, PgDn (da confermare). La **rotazione** dell'encoder non fa parte della matrice: i suoi contatti vanno trovati
(layout `probe`, righe `pNN=L` e `iNN`). Il chord di recupero è Esc + 1 + 2 (celle `c0r0`, `c1r0`, `c2r0`) e **è stato
verificato sul chip reale**: la tastiera compare come `0603:1020` ("Gaming KB", report 5 e 6 del bootloader stock). Lo
sblocco Vial è Esc + Invio.

## Hardware (M0.4)

*Da compilare: foto PCB, pad ICP, pinout.*

## Dump del firmware stock (M0.5–M0.7)

Dump eseguito via ISP (M0.5), file in `backups/` (ignorato da git, fuori dal repo: tenere anche una copia su altro disco).

| File | Contenuto | sha256 |
| --- | --- | --- |
| `full.hex` | flash completa 0x0000–0xFFFF (64 KiB) | `ce6f7b83…d1ddcf9184` |
| `firmware.hex` | = `full` 0x0000–0xEFFF | `2aad0baf…c96060` |
| `bootloader.hex` | = `full` 0xF000–0xFFFF, ma indirizzato da 0x0000 nell'hex | `e1f44081…37ddab` |

Verifica incrociata fatta da Claude: firmware e bootloader sono coerenti con `full`. Osservazioni sullo stock:
reset vector `0x0000: LJMP 0x9A26` (punta all'applicazione, non al bootloader come in SMK); bootloader stock a 0xF000
inizia con `clr EA; mov SP,#0x70`; nessuno stub tipo `ISP_ENTRY` evidente a 0xFF00. Da usare per il gate G0.
**Attenzione:** per ripristinare il bootloader da `bootloader.hex` va rispostato a 0xF000.

Code option e prova di ripristino: M0.6/M0.7 (ancora da fare).

## Gate G0 (M0.9)

**Esito (Claude, 2026-10-31): NON superato — il Nano diventa obbligatorio prima del primo flash (R11).**

* In ISP si entra **tramite l'applicazione stock**: `sinowisp` apre il dispositivo in esecuzione `258a:010c`
  (interfaccia 1, feature report 5), la tastiera si resetta nel bootloader, si legge, si riavvia. Non è stato provato
  nessun ingresso indipendente dall'app (tasto/ponticello a freddo), né sappiamo cosa fa il bootloader stock se
  l'app è assente o non valida.
* Quindi con un'app che non parte non c'è un percorso di recupero provato. Per sbloccare il primo flash serve l'ICP
  (M0.3), oppure un test controllato futuro.
* Preset da usare: `--platform sh68f90 --vendor_id 0x258a --product_id 0x010c` (isp_iface_num=1, isp_report_id=5 di
  default, identici a SMK).

**Anomalia dump — risolta (M0.10):** `dump/r1.bin` e `dump/r2.bin` sono identici (MD5 `4aea8bec8f13356a66a90788be58f179`,
64 KiB) e coincidono con la rilettura di M0.9. Rispetto al primo `full.hex` differiscono solo 2 byte: `0xC60A`
(0x11→0x08) e `0xC65A` (0x04→0x01), cioè dati di stato scritti dal firmware stock in una zona a 0xC600.
Il backup è affidabile. **Dump canonico: `backups/dump/r1.bin`** (binario, 65536 B); gli `.hex` restano come copie.