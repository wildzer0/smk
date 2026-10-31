# Prima accensione di SMK sulla B68: sequenza sul banco

Tutto ciò che resta da fare **con l'hardware in mano**, nell'ordine in cui va fatto. Ogni passo dice cosa fai, cosa deve
succedere e quando fermarti. Il codice e i test sul simulatore sono già a posto; questi passi servono a sapere se la
realtà coincide con quello che abbiamo ricavato dal firmware stock.

Regola generale: **batteria e cavo USB della tastiera scollegati** quando colleghi o scolleghi il programmatore.

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

Se non compare nulla: tieni premute le celle del chord di recupero **mentre colleghi l'USB** (celle provvisorie: colonna 0, 1 e 2
della riga 0, cioè i primi tre tasti che il probe scrive come `c00r0`, `c01r0`, `c02r0`; non sai quali siano, quindi prova i tre
tasti del primo angolo in alto a sinistra) per rientrare in ISP; altrimenti ripristina il dump con il Nano.

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
