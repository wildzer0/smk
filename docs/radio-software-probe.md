# Sonda software per la radio (senza aprire la tastiera)

> **Nota (rimossa dalla build):** il layout di diagnosi descritto qui è stato tolto dal repository; resta nella storia git (commit `bd19894` e precedenti). Esiste una sola build di rilascio.


Cosa si può sapere **senza** analizzatore: quali ingressi si muovono da soli, cioè quali pin dell'MCU sono collegati a qualcosa che parla (la radio). Non permette di decodificare i frame.

Il layout `probe` (build `yunzii-b68_probe_smk.hex`), dopo 4 s dall'accensione senza toccare nulla, scrive una riga come
`n47 n05 n--` : i pin di ingresso che hanno cambiato livello da soli durante quei 4 s (`n47` = `P4.7`). Poi scrive come al solito le variazioni delle altre linee.

Procedura:
1. Flasha la sonda; non toccare niente per 4 s; copia il testo `n.. n..` che compare.
2. Ripeti con l'interruttore di modalità nelle posizioni **2.4G**, **BT** e **USB/spenta** (o come sono etichettate): le linee della radio devono comportarsi diversamente.
3. Incolla i tre risultati. Se `P4.7` (attenzione della radio) compare in modalità wireless e non in USB, l'ipotesi è confermata; `P5.5`/`P5.6`
   appariranno solo se la radio trasmette senza essere interrogata.

Per decodificare i frame serve la guida con l'analizzatore (`docs/radio-capture-guide.md`).
