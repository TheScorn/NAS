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