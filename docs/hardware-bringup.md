# Prima accensione di SMK sulla B68: sequenza sul banco

Tutto ciò che resta da fare **con l'hardware in mano**, nell'ordine in cui va fatto. Ogni passo dice cosa fai, cosa deve
succedere e quando fermarti. Il codice e i test sul simulatore sono già a posto; questi passi servono a sapere se la
realtà coincide con quello che abbiamo ricavato dal firmware stock.

Regola generale: **batteria e cavo USB della tastiera scollegati** quando colleghi o scolleghi il programmatore.

## Fase 0 — Senza il Nano (solo ISP, rischio accettato)

Se il Nano non c'è ancora, si può già provare tutto via ISP (`sinowisp`, lo stesso strumento con cui hai fatto il dump).
**Il rischio**: se il nostro firmware non parte, la tastiera resta inutilizzabile fino al ripristino via ICP con il Nano (il
dump è al sicuro e riscrivibile). Per ridurlo, prima si prova la scrittura con un'immagine che è identica allo stock.

| # | Cosa fai | Risultato atteso |
| --- | --- | --- |
| 0.1 | Copia in una cartella di Windows `backups/dump/app_r1.bin` (l'applicazione stock, 61440 B, **senza** bootloader) | |
| 0.2 | `.\sinowisp.exe write --platform sh68f90 --vendor_id 0x258a --product_id 0x010c --format bin app_r1.bin` | scrittura riuscita; la tastiera funziona come prima |
| 0.3 | Rileggi: `.\sinowisp.exe read --platform sh68f90 --vendor_id 0x258a --product_id 0x010c -s firmware --format bin check.bin` e confrontala con `app_r1.bin` | uguali, tranne i 2 byte di stato a `0xC60A` e `0xC65A` |

Se 0.2 o 0.3 non vanno, **fermati**: significa che la scrittura via ISP non funziona con i parametri di default e va capito
prima di scrivere qualsiasi cosa nostra. Se vanno, la strada di scrittura è provata senza aver cambiato nulla.

Poi il firmware sonda (Fase B), scritto allo stesso modo:
`.\sinowisp.exe write --platform sh68f90 --vendor_id 0x258a --product_id 0x010c --format ihex -f yunzii-b68_probe_smk.hex`
(`-f` perché l'immagine è più corta dell'area firmware).

Se dopo la sonda la tastiera non compare: tieni premuti i tre tasti del chord **mentre colleghi l'USB** (la prima riga, i primi tre
tasti da sinistra: è l'ipotesi più probabile per le celle colonna 0, 1, 2 riga 0). Se entra in ISP (`sinowisp` la trova)
rimetti `app_r1.bin` come in 0.2. Altrimenti si aspetta il Nano.

## Fase A — Rete di sicurezza (solo lettura)

| # | Cosa fai | Risultato atteso | Se no |
| --- | --- | --- | --- |
| A1 | Nano ricevuto: leggi la signature con `avrdude` ([recovery-icp.md](recovery-icp.md)) | `0x1e950f` (ATmega328P) | `0x1e9406` = ATmega168: non basta, riordinane uno |
| A2 | Mandami il pinout del connettore (sigle dei chip, etichette dei contatti, GND/VCC, come alimenti la scheda) | io confermo i collegamenti | non collegare nulla |
| A3 | Prepara il Nano con `sinodude-serial` e collega la B68 come da [recovery-icp.md](recovery-icp.md) | il Nano risponde | rivedi i collegamenti, **prima di dare corrente** |
| A4 | `sinodude read -c sinodude-serial -p sh68f90a --port /dev/ttyUSB0 --flash icp1.hex`, due volte | due file identici | cavi lunghi / contatto: riprova |
| A5 | Confronta `icp1.hex` col dump via ISP (`backups/dump/r1.bin`) | uguali, tranne i 2 byte di stato a `0xC60A` e `0xC65A` | fermati e mandami la differenza |
| A6 | Leggi le code option ([recovery-icp.md](recovery-icp.md)) | tabella di byte | |
| A7 | Riscrivi il dump stock con `sinodude write` e controlla che la tastiera funzioni come prima | tastiera normale | **non andare avanti**: il recupero non funziona |

Finché A7 non è verde non si scrive nessun firmware nostro: è la prova che, se sbagliamo, torniamo indietro.

## Fase B — Il firmware sonda (trova la mappa dei tasti)

Il layout `probe` fa digitare a ogni tasto le coordinate della sua cella ("c03r2 "). Serve a ricavare quale tasto fisico è
su quale (colonna, riga), e conferma che la matrice 5×16 dello stock è giusta.

| # | Cosa fai | Risultato atteso |
| --- | --- | --- |
| B1 | `meson compile -C build-b68 yunzii-b68_probe_smk.hex` | compila (nel `nix develop`) |
| B2 | Scrivi **solo l'applicazione** via ICP, lasciando intatto il bootloader stock: `sinodude write -c sinodude-serial -p sh68f90a --port /dev/ttyUSB0 --flash build-b68/yunzii-b68_probe_smk.hex --start_addr 0x0000 --end_addr 0xF000` | scrittura riuscita |
| B3 | Scollega il Nano, collega l'USB, apri un editor di testo | la tastiera compare come dispositivo HID |
| B4 | Premi i tasti **in un ordine noto** (riga per riga, da sinistra) | per ogni tasto compare `cNNrR ` |
| B5 | Incollami il testo | io ricavo la mappa |

Se non compare nulla: vedi la Fase 0 (chord di recupero, poi ripristino con `app_r1.bin` o con il Nano).

Cosa mi dice la sonda in più: se `P7.0` o altri ingressi non producono nulla, la matrice è giusta; se alcune celle non rispondono, i
pin delle colonne o delle righe differiscono dall'ipotesi.

## Fase C — Da qui in avanti (dopo la mappa)

1. Io scrivo il layout vero, il `vial.json` con i tasti fisici e il chord di recupero definitivo (tre tasti reali).
2. Riflash **senza programmatore**: `sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead -f <hex>`
   (la nostra build ha `0x258a:0xdead` finché non decidiamo il VID:PID, D6). L'ingresso in ISP è lo stesso dello stock.
3. Spike S1 (EP2 OUT a 64 B) e trasporto Raw HID, poi Vial GUI.
4. LED, sleep e wireless.

## Cose da sapere

* **Il riflash non cancella la keymap salvata** se la zona non rientra nell'immagine scritta: dopo aver cambiato layout, un
  "reset" da Vial (o il reset della keymap) riporta ai default compilati.
* I passi di questa sequenza non sono ancora stati eseguiti; le righe marcate come "da verificare" in
  [recovery-icp.md](recovery-icp.md) restano tali finché non le proviamo.

## Stato (2026-10-31)

* Fase 0 eseguita: scrittura dell'immagine stock via ISP e rilettura identica; firmware sonda scritto via ISP, matrice mappata
  (vedi [keyboards/yunzii-b68.md](keyboards/yunzii-b68.md)). Il Nano non è ancora arrivato.
* **Prossimo** (senza Nano): riflash via ISP **dal firmware SMK** (VID:PID `258a:dead`) con il layout `default`:
  `.\sinowisp.exe write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex -f yunzii-b68_default_smk.hex`.
  Prova anche che l'ingresso in ISP dall'applicazione SMK funziona sul chip reale.

* **Fatto (2026-10-31):** riflash da SMK a SMK via ISP riuscito; il chord Esc+1+2 porta la tastiera in ISP (compare come
  `0603:1020`); `sinowisp` rileva da solo il dispositivo già in ISP ("Device not found. Trying ISP device...") e si usano gli
  stessi comandi con gli stessi VID:PID dell'applicazione.
* **Trovato:** la tastiera ha 66 tasti + encoder rotativo (pressione = `c15r0`). Il layout è stato corretto di conseguenza.
* **Prossimo (hardware):** flashare il layout `probe` aggiornato e girare l'encoder lentamente: ora scrive anche `p70=0` ecc. quando un ingresso
  non di matrice cambia, e `iNN` per i contatti a massa fissa.

## Spike S1: raw HID su EP2 (build `yunzii-b68_rawhid_smk.hex`)

Domanda: l'endpoint EP2 OUT del SH68F90A riceve davvero pacchetti da 32 byte e il firmware risponde su EP2 IN? Se sì, Vial
si fa col piano principale (D8); se no, piano B (feature report su EP0).

**Cosa contiene la build.** Il layout `default` con l'interfaccia 1 sostituita da un'interfaccia raw HID (usage page `0xFF60`,
usage `0x61`, report da 32 byte senza ID), che passa ogni pacchetto a `via_receive()` (VIA e Vial). **Limiti noti, voluti per il
test:** niente tasti multimediali né encoder (nessun posto dove mandarli, finché non li spostiamo sull'interfaccia 0), niente
NKRO, niente report ISP: per tornare al firmware normale si entra in ISP col **chord Esc + 1 + 2 mentre colleghi l'USB**.

| # | Cosa fai | Risultato atteso |
| --- | --- | --- |
| S1.1 | Flasha `build-b68/yunzii-b68_rawhid_smk.hex` come al solito (`sinowisp write ... -f`) | la tastiera riparte e si digita |
| S1.2 | Apri `tools/rawhid-test.html` con Chrome/Edge, premi **Connect** e scegli la tastiera | compare "connected" (se la tastiera non è in elenco: la build non ha l'interfaccia raw HID) |
| S1.3 | **VIA protocol version** | risposta `01 00 09 ...` |
| S1.4 | **Vial id** | `06 00 00 00` e l'id `46 ec d0 56 5d f8 4f a8` |
| S1.5 | **Read the whole Vial definition** | "xz header ok" |
| S1.6 | **Stress: 1000 round trips** | 0 bad |
| S1.7 | Se tutto è verde, prova **Vial** (https://get.vial.today): deve riconoscere la tastiera e mostrare il layout | |

Per tornare indietro: chord Esc + 1 + 2 mentre colleghi l'USB, poi `sinowisp write ... -f yunzii-b68_default_smk.hex`.

Se S1.2 non trova la tastiera o S1.3 non risponde, dimmelo con cosa vedi in `Gestione dispositivi` (o `lsusb -v`): è il dato che
decide il piano B.
