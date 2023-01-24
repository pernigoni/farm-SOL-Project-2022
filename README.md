# Progetto SOL a.a. 2022/2023 - farm
## Possibili problemi e miglioramenti
- Cosa succede con un file che contiene meno di 8 byte?
- I file regolari potevano essere inseriti direttamente nella coda concorrente invece che farli passare da liste.
- Si poteva fare una sola `connect` per ogni worker che manteneva aperta la connessione fino alla fine (fare troppe `connect` può essere rischioso).
- Si poteva usare un messaggio dedicato (3 `write`) per comunicare al Collector di stampare, avrei risparmiato molte `read` e `write`.
- Non serviva notificare al Collector che deve terminare con una `write`, si capiva dalla connessione chiusa.
- Invece di fare `socket`, `bind` e `listen` prima della `fork` (ho fatto così per evitare corse critiche tra processi) le avrei potute fare nel 
Collector. Se i worker non avessero trovato il socket pronto avrebbero potuto semplicemente riprovare a connettersi. In questo modo il processo Collector 
funzionerebbe anche se non fosse parente.
