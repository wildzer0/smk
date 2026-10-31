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

*Da compilare: la B68 entra in ISP senza applicazione? sì/no.*
