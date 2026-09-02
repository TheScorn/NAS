# NAS
Network Attached Storage

Repo na serwer do przechowywania danych

Początkowy schemat podobny do http.
Cały init właściwie można przekopiować


Nie zakładamy z góry że żądania są typu http więc piszemy je sami

Każde żądanie musi być podpisane loginem i hasłem

Żądania będą szyfrowane (trzeba wybrać jakim kluczem - raczej asymetrycznym)

Z NAS można będzie korzystać przez serwer HTTP, za pomocą clienta CLI i aplikacji, więc requesty muszą być dość ogólne i proste


Działania które chcemy móc wykonać: lista plików z danego folderu, pobranie pliku, upload pliku, test połączenia

Ogólny wygląd requestów

LIST /path flags(int) login:{login} password:{password}

GET /path flags(int) login:{login} password:{password}

PUT /path flags(int) login:{login} password:{password}

DELETE /path login:{login} password:{password}

TEST
Odpowiedź ACK


Każdy uzytkownik ma wpisane w bazie swoje dane oraz posiada swój folder
admin powinien móc przeglądać wszystkie foldery

Przesyłanie pliku będzie dwustopniowe
Początkowo wysyłane będą metadane w json, w nich: nazwa, rozszerzenie, długość rozszerzenia, wielkość pliku, czas ostatniej modyfikacji



Każdy początkek wiadomości to 16 znaków tłumaczonych na liczbę hex. 
Znając tą liczbę wiemy ile bajtów wiadomości musimy odebrać

Odbieramy w pętli póki nie będzie 16 bajtów
Odczytujemy wartość hex.
Tworzymy bufor odpowiedniej wielkości i odbieramy póki nie przyjdzie cała wiadomość.
Jak przyjdzie to odpowiadamy




PUT

przychodzi
{PREFIX}PUT {ścieżka do folderu} login:{login} password:{password}

od razu po tym idzie
{PREFIX}{type}{16x hex mtime}{16x hex size}{name}

Serwer wysyła ACCEPT albo REFUSE w zależności od ilości miejsca klienta

przychodzi
{PREFIX} -> sprawdzamy czy zgadza się z wysłaną wielkością pliku

tworzymy plik i zapisujemy




Podczas auth dodajemy wyciąganie wielkości przeznaczonej pamięci z db.

Robimy potem normalnie auth.

Jeśli nie jest elevated to przy sprawdzaniu czy ścieżka prowadzi do folderu otiweramy też root użytkownika i znajdujemy ile waży.

Jeśli waga + plik przychodzący > Miejsce na serwerze
REFUSE

Inaczej ACCEPT


Czy na pewno chcemy zapisywać miejsce użytkownika w MB????
Do testów mało wygodne, ale jeśli serwer będzie działał na poważnie to pewnie pamięć będzie liczona w GB.



TODO: zmiana mtime po zapisaniu pliku.

BUG: Po użyciu list, użycie put powoduje segfault po 0 DEBUGU.
Można użyć dwa razy put

Po użyciu get użycie put powoduje segfault

używanie ls i get naprzemiennie nie powoduje błędów. problem jest prawdopodbnie gdzieś w PUT