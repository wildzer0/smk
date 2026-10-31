# Task di sviluppo

Qui vivono le **schede dei task** del [piano di sviluppo](../porting-plan.md). Una scheda = un task = (di norma)
un branch/PR. Le schede le scrivo io; tu implementi; io faccio review e tengo aggiornati documentazione, indice e stato delle milestone.

## Come si usa

1. Apri la scheda del task in stato **Pronto**.
2. Leggi *Contesto* e *Prima di iniziare*; se qualcosa non è chiaro, chiedimelo **prima** di scrivere codice. Nessuna domanda di review: la scheda dice cosa fare e cosa mi aspetto.
3. Lavora su un branch dedicato, con commit nello stile del repo (`area: messaggio breve in minuscolo`).
4. Compila la sezione *Evidenza* (output dei comandi, numeri, foto) e passami branch/diff.
5. Review: ti rispondo con rilievi classificati e *cosa è fatto bene*. Chiudiamo quando i criteri di accettazione
   sono tutti verdi.

## Stati

`Bozza` → `Pronto` → `In corso` → `In review` → `Chiuso` (o `Bloccato`).

## Indice

| ID | Titolo | Milestone | Stato | Eff. |
| --- | --- | --- | --- | --- |
| [M0.1](M0.1-ambiente-e-baseline.md) | Ambiente di sviluppo e baseline di build | M0 | **Chiuso** | S |
| M0.2 | Scheletro di `docs/keyboards/yunzii-b68.md` | M0 | **Chiuso** (assorbito in M0.1) | S |
| M0.3 | Acquisti: Arduino Nano + MOSFET + componenti (vedi [recovery-icp.md](../recovery-icp.md)) | M0 | Bozza | S |
| M0.4 | Individuare i pad ICP e fotografare il PCB | M0 | Bozza | M |
| M0.5 | Dump del firmware stock (prima via ISP `-s full`, poi ICP) | M0 | Bozza | M |
| M0.6 | Prova di ripristino del dump | M0 | Bozza | M |
| M0.7 | Lettura delle code option | M0 | Bozza | M |
| M0.9 | **Gate G0**: dimostrare l'ingresso in ISP senza l'applicazione | M0 | Bozza | M |

I task successivi li dettaglio uno alla volta quando chiudiamo quello precedente.

## Modello di scheda

Ogni scheda ha queste sezioni (copia da [M0.1](M0.1-ambiente-e-baseline.md)):

* **Metadati**: milestone, stato, effort, dipendenze, branch suggerito.
* **Obiettivo** e **Perché** (che cosa sblocca).
* **Contesto / Da leggere prima**: file e documenti.
* **Requisiti**: cosa deve fare, numerati.
* **Vincoli e trappole**.
* **Fuori ambito**.
* **Criteri di accettazione**: verificabili, uno per riga.
* **Evidenza da allegare**.
* **Checklist di review** (quella del [piano §15](../porting-plan.md#15-modalità-di-lavoro-e-review) + specifiche).
