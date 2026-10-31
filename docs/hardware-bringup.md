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
