# Instrukcja obsługi Wio-Tracker

**Język:** polski  
**Urządzenie:** Seeed Studio Wio Tracker L1  
**Stan dokumentu:** wersja robocza; instrukcja będzie uzupełniana

Ten dokument opisuje obsługę firmware OGN-Tracker na urządzeniu Wio Tracker L1. Szczegółowe opisy poszczególnych ekranów, parametrów i typowych problemów będą dopisywane stopniowo.

## 1. Przyciski, przełącznik oraz joystick

Wyłącznik oraz przycisk reset służą do załączania, wyłączania oraz operacji aktualizacji oprogramowania. Umieszczone są one z boku obudowy w taki sposób, że przypadkowe ich uruchomienienie jest w zasadzie niemożliwe.

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
| `Address` | 24-bitowy adres wyświetlany jako sześć cyfr szesnastkowych. |
| `AddrType` | Rodzaj adresu: RND, ICAO, FLARM albo OGN. Zmieniaj go tak, aby Twoje transmisje radiowe były możliwie spójne |
| `Tx power` | Moc nadajnika w dBm. Dostępny zakres w menu: 0–22 dBm. |
| `Warn time` | Czas wyprzedzenia ostrzeżeń dzwiękowych: 20, 30, 40 albo 50 sekund. |
| `Alerts` | Minimalny poziom ostrzeżenia: wszystkie, poziom 1 i wyższe, poziom 2 i wyższe, poziom 3 i wyższe albo wyłączone. |
| `Ghost` | Ustawienie trybu Ghost. Dostępne wartości: Off, Traffic, Altitude. Szczegółowy opis skutków poszczególnych trybów zostanie uzupełniony. |
| `Reg` | Edycja znaku rejestracyjnego. |
| `Pilot` | Edycja imienia pilota. |
| `Format flash` | Formatuje zewnętrzną pamięć flash po ekranie potwierdzenia. Zobacz ostrzeżenie poniżej. |
| `Reset defaults` | Przywraca domyślne wartości parametrów po ekranie potwierdzenia. Nie kasuje konfiguracji TTN/LoRaWAN |
| `Register TTN` | Wygeneruj nowy klucz dla sieci TheThingsNetwork (kasuje bezpowrotnie poprzedni) - kod QR musi zostać wysłany do rejestracji |
| `Shutdown` | Zamyka logger plików i usypia tracker. Do ponownego uruchomienia użyj przycisku RESET. |

### Edycja adresu

Wartość adresu składa się z sześciu cyfr szesnastkowych. Góra/dół zmienia zaznaczoną cyfrę, a lewo/prawo wybiera cyfrę do edycji. Przytrzymaj środek joysticka, aby zapisać nowy adres.
Pomyśl, czy na pewno chcesz wpisać nowy adres i co to jest za adres: nie wpisuj cudzego adresu np. ICAO albo FLARM chyba, że właśnie lecisz tym samolotem, wtedy jak najbardziej, a po zakończeniu lotu wpisz swój, albo wróć do ustawień fabrycznych.

### Edycja adresu

Ustaw właściwy rodzaj adresu, który wpisałeś: np. jesli wpisałeś adres ICAO samolotu to ustaw także rodzaj adresu ICAO.

### Edycja elementów identyfikacji

Pozycje `Reg` i `Pilot` edytuje się znak po znaku. Lewo/prawo wybiera znak, góra/dół zmienia go. Dostępny zestaw obejmuje litery, cyfry, spację i wybrane znaki interpunkcyjne. Przytrzymaj środek joysticka, aby zapisać tekst.

### Formatowanie pamięci flash (logi oraz inne pliki)

Formatowanie pamięci usuwa jej zawartość, w tym zapisane pliki i logi lotów. Wykonuj tę operację tylko wtedy, gdy jest potrzebna, na przykład przy pierwszym uruchomieniu funkcji logowania albo po problemie wymagającym ponownego przygotowania systemu plików. Firmware nie wykona formatowania z menu, jeżeli log jest nadal otwarty.

### Reset ustawień

`Reset defaults` zmienia zapisane parametry na wartości domyślne. Użyj tej opcji tylko wtedy, gdy chcesz ponownie skonfigurować tracker 'od zera'.

### Wyłączanie

Zalecane jest wykonanie `Shutdown`: tracker zatrzyma logowanie, GPS i radio, a następnie wejdzie w stan uśpienia. Poczekaj na zakończenie procesu. Do ponownego uruchomienia użyj RESET lub odłącz i ponownie podłącz zasilanie.
Wyłaczenie zasilania, szczególnie w trakcie lotu, może skutkować zniszczeniem niektórych plików.

## 5. Aktualizacja oprogramowania poprzez plik UF2

1. Pobierz plik UF2 przeznaczony dla **Wio-Tracker**. Nie używaj obrazu dla T-Echo ani innego urządzenia.
2. Dwukrotnie naciśnij RESET, aby wejść w tryb bootloadera. Urządzenie powinno pojawić się na komputerze jako dysk USB.
3. Jeśli chcesz zachować bieżące oprogramowanie to wykonaj kopię zapasową pliku `CURRENT.UF2`
4. Skopiuj (nowy) pobrany plik UF2 na dysk USB.
5. Poczekaj na restart urządzenia i sprawdź, czy uruchamiła się nowa wersja.

Aktualizację można wykonywać dowolną liczbę razy, można wracać do poprzedniego oprogramowania, wpisywać zupełne inne oprogramowanie np. meshCore - nie ma tutaj żadnych ograniczeń typu, że jak wpisałem OGN-Tracker to już nic innego nie mogę nigdy wpisać.

## 6. System ostrzeżeń przeciwkolizyjnych

System ten monitoruje statki powietrzne będące w zasięgu odbioru i wykonuje predykcje ich torów: jeśli któryś z torów zbliża się niebepiecznie do pozycji własnej odczytywanej z GPSa to wtedy generowane jest ostrzeżenie dzwiękowe.

Najwcześniejszy poziom ostrzegania (nazwijmy go 'zerowym') to bliskość innego statku powietrznego, która jest sygnalizowana pojedynczym tonem w odstępie 10 sekund: nie jest to ostrzeżenie krytyczne, ponieważ nie wynika ze zbieżności toru tego statku.

Jeżeli zostanie wykryty zbieżny tor to na 20 sekund przed szacowanym punktem maksymalnego zbliżenia pojawi się pierwszy poziom ostrzegania wyrażany jednym tonem w odstępnie sekundy: to oznacza, że za 20 sekund będziecie naprawdę blisko siebie !
Ten czas 20 sekund można wydłuzyć w ustawieniach do 30, 40 lub 50 sekund.

W miarę zbliżania się punktu krytycznego zbliżenia odzywać się będą kolejne poziomy alarmów: drugi (podwójny i wyższy ton) oraz trzeci (potrójny i jeszcze wyższy ton).
W ustawieniach można ustawić, od którego poziomu chcemy słyszeć alarmy: np. 2+ oznacza, że usłyszymy tylko te od drugiego poziomu w górę, czyli drugi i trzeci (ostatni).

**UWAGA:** algorytm przeciwkolizyjny jest eksperymentalny i choć został przesymulowany to nie jest żadną gwarancją czegokolwiek - w szczególności nie zastępuje on normalnym i obowiązkowych metod unikania kolizji w powietrzu czyli obserwacji przestrzeni powietrznej oraz zdrowego rozsądku.

## 7. Logowanie lotów

Wio-Tracker zapisuje (loguje) loty w zewnętrznej pamięci flash o pojemności około 2MB. Termin 'zewnętrzna' oznacza, że pamięć ta nie jest częścią jednostki centralnej, ale jest poza nią, na osobnym układzie scalonym (ale wciąż w tym samym urządzeniu).

Pamięć ta jest sformatowana jako dysk typu FAT i pliki na niej zapisywane są równoprawne z plikami na innych nosnikach pamięci. Możliwy jest bezpośredni dostęp to tych plików jeśli ustawić Tracker w trybie 'USB memory': można wtedy pliki kopiować w jedną i w drugą stronę, można nawet uzywac klasycznych narzędzi do odzyskiwania plików, bo jest to zupełnie normalna partycja FAT, jak w innych komputerach.
**Uwaga:** w trybie 'USB memory' Tracker **przestaje** działać jako urządzenie elektronicznej widoczności: aby przywrócić normalne funkcje należy przycisnąć przycisk RESET lub wyłączyć i włączyć ponownie urządzenie.

Pliki zapisywane są w formacie binarnym, w celu maksymalnego wykorzystania ograniczonej przestrzeni do zapisu. Konwerter logów do postaci czytelnej dla użytkownika zostanie przedstawiony nieco później.

Przy pierwszym uruchomieniu należy sformatować pamięć flash: patrz punkt w menu 'Format flash'. Flash należy sformatować w wypadku jego zepsucia np. poprzez wyłaczenie zasilania w nieodpowiednim momencie.
Tutaj kolejna uwaga: przed wyłączeniem urządzenia dobrze jest wykonać 'Shutdown' w celu płynnego zamknięcia systemu plików, co minimalizuje ryzyko jego popsucia.

