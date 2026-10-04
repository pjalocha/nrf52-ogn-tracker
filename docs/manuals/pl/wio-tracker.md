# Instrukcja obsługi Wio-Tracker

**Język:** polski  
**Urządzenie:** Seeed Studio Wio Tracker L1  
**Stan dokumentu:** wersja robocza; instrukcja będzie uzupełniana

Ten dokument opisuje obsługę firmware OGN Tracker na urządzeniu Wio Tracker L1. Szczegółowe opisy poszczególnych ekranów, parametrów i typowych problemów będą dopisywane stopniowo.

## 1. Przyciski, przełącznik oraz joystick

Wyłącznik oraz przycisk reset umieszczone są z boku w taki sposób, że przypadkowe ich uruchomienienie jest w zasadzie niemożliwe. Służą one do załączania, wyłączania oraz operacji aktualizacji oprogramowania.

Przycisk oraz joystick na panelu z przodu służą natomiast do przełączania stron oraz ustawiania parametrów.

| Element | Krótkie naciśnięcie | Przytrzymanie |
| --- | --- | --- |
| Środek joysticka | Wejście do zaznaczonej pozycji albo powrót do listy menu bez zapisywania zmian | Otwiera menu z ekranu głównego; zapisuje wybraną wartość; na ekranie potwierdzenia wykonuje opisaną operację |
| Joystick góra/dół | W menu: wybór pozycji lub zmiana wartości | Przy edycji tekstu przytrzymanie powtarza zmianę znaku |
| Joystick lewo/prawo | Poza menu: poprzednia/następna strona ekranu; przy edycji adresu lub tekstu: zmiana pozycji | Poza menu: może przełączać strony; przy edycji tekstu przytrzymanie powtarza zmianę znaku |
| Przycisk zmiany stron | Przełącza strony ekranu | Blokuje albo odblokowuje joystick, aby ograniczyć przypadkowe naciśnięcia |

Przytrzymanie środka joysticka przez około 2 sekundy otwiera menu. Lista menu przewija się cyklicznie. Na ekranie potwierdzenia krótkie naciśnięcie środka anuluje operację, a przytrzymanie ją zatwierdza.

## 2. Otwieranie i opuszczanie menu

1. Przytrzymaj środek joysticka przez około 2 sekundy.
2. Wybierz pozycję joystickiem góra/dół.
3. Krótko naciśnij środek joysticka, aby wejść do wybranej pozycji.
4. Zmień wartość odpowiednimi kierunkami joysticka.
5. Przytrzymaj środek joysticka, aby zapisać zmianę. Krótkie naciśnięcie wraca do listy bez zapisywania.

Po pomyślnym zapisie urządzenie pokazuje komunikat i sygnał dźwiękowy. Jeżeli parametr nie wymaga zmiany, zatwierdzenie może nadal pokazać komunikat „Saved”.

## 3. Pozycje menu

Dokładna lista zależy od opcji wkompilowanych w daną wersję firmware. Podstawowe pozycje to:

| Pozycja na ekranie | Znaczenie / działanie |
| --- | --- |
| `AcftType` | Typ statku powietrznego nadawany przez tracker, np. szybowiec, samolot, paralotnia lub UAV. |
| `AddrType` | Rodzaj identyfikatora: RND, ICAO, FLARM albo OGN. Zmień go tylko zgodnie z przydzielonym identyfikatorem. |
| `Address` | 24-bitowy adres wyświetlany jako sześć cyfr szesnastkowych. |
| `Tx power` | Moc nadajnika w dBm. Dostępny zakres w menu: 0–22 dBm. |
| `Warn time` | Czas wyprzedzenia ostrzeżeń: 20, 30, 40 albo 50 sekund. |
| `Alerts` | Minimalny poziom ostrzeżenia: wszystkie, poziom 1 i wyższe, poziom 2 i wyższe, poziom 3 i wyższe albo wyłączone. |
| `Ghost` | Ustawienie trybu Ghost. Dostępne wartości: Off, Traffic, Altitude. Szczegółowy opis skutków poszczególnych trybów zostanie uzupełniony. |
| `Reg` | Edycja znaku rejestracyjnego. |
| `Pilot` | Edycja nazwy pilota. |
| `Format flash` | Formatuje zewnętrzną pamięć flash po ekranie potwierdzenia. Zobacz ostrzeżenie poniżej. |
| `Reset defaults` | Przywraca domyślne wartości parametrów po ekranie potwierdzenia. |
| `Register TTN` | Pozycja dostępna w firmware z LoRaWAN; rozpoczyna konfigurację TTN i pokazuje kod QR. |
| `Shutdown` | Pozycja dostępna w firmware z obsługą wyłączania; zatrzymuje tracker. Do ponownego uruchomienia użyj przycisku RESET. |

Pozycje `Register TTN` i `Shutdown` pojawiają się tylko wtedy, gdy firmware zostało zbudowane z odpowiednimi funkcjami. Inne opcje mogą pojawić się w zależności od konfiguracji.

### Edycja adresu

Wartość adresu składa się z sześciu cyfr szesnastkowych. Góra/dół zmienia zaznaczoną cyfrę, a lewo/prawo wybiera cyfrę do edycji. Przytrzymaj środek joysticka, aby zapisać nowy adres.

### Edycja tekstu

Pozycje `Reg` i `Pilot` edytuje się znak po znaku. Lewo/prawo wybiera znak, góra/dół zmienia go. Dostępny zestaw obejmuje litery, cyfry, spację i wybrane znaki interpunkcyjne. Przytrzymaj środek joysticka, aby zapisać tekst.

## 4. Ważne ostrzeżenia

### Aktualizacja firmware

Przed aktualizacją firmware Wio Tracker sprawdź na dysku bootloadera, czy znajduje się plik `CURRENT.UF2`. Jeśli jest dostępny, skopiuj go w bezpieczne miejsce przed wgraniem nowej wersji. Może posłużyć do powrotu do poprzedniego firmware.

`CURRENT.UF2` jest kopią firmware, a nie lotów zapisanych w zewnętrznej pamięci flash. Nie formatuj zewnętrznej pamięci przy zwykłej aktualizacji.

### Formatowanie pamięci flash

Formatowanie zewnętrznej pamięci usuwa jej zawartość, w tym zapisane pliki i logi lotów. Wykonuj tę operację tylko wtedy, gdy jest potrzebna, na przykład przy pierwszym uruchomieniu funkcji logowania albo po problemie wymagającym ponownego przygotowania systemu plików. Firmware nie wykona formatowania z menu, jeżeli log jest nadal otwarty.

### Reset ustawień

`Reset defaults` zmienia zapisane parametry na wartości domyślne. Użyj tej opcji tylko wtedy, gdy chcesz ponownie skonfigurować tracker.

### Wyłączanie

Jeśli menu zawiera pozycję `Shutdown`, po jej zatwierdzeniu tracker zatrzyma logowanie, GPS i radio, a następnie się wyłączy. Poczekaj na zakończenie procesu. Do ponownego uruchomienia użyj RESET lub odłącz i ponownie podłącz zasilanie.

## 5. Aktualizacja przez plik UF2

1. Pobierz plik UF2 przeznaczony dla **Wio-Tracker**. Nie używaj obrazu dla T-Echo ani innego urządzenia.
2. Jeśli jest dostępny, wykonaj kopię `CURRENT.UF2` zgodnie z sekcją ostrzeżeń.
3. Dwukrotnie naciśnij RESET, aby wejść w tryb bootloadera. Urządzenie powinno pojawić się na komputerze jako dysk USB.
4. Skopiuj plik UF2 na dysk bootloadera.
5. Poczekaj na restart urządzenia i sprawdź, czy uruchamia się nowa wersja.

## 6. Miejsce na kolejne rozdziały

- Opis stron OLED i informacji wyświetlanych podczas lotu.
- Szczegółowy opis parametrów i zalecane ustawienia.
- Pierwsze uruchomienie i konfiguracja identyfikatora.
- Logowanie lotów oraz odczyt plików z pamięci.
- Połączenie BLE z aplikacją nawigacyjną.
- Rozwiązywanie problemów i interpretacja komunikatów.

