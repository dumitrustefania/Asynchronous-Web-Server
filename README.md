# PLANIFICATOR DE THREADURI

322CA - Bianca Ștefania Dumitru
Sisteme de operare

Ianuarie 2023
----------------------------------------------------------------------------------------------------
## Introducere

* Server web asincron
  *  programul implementeaza un server web asincron in Linux
  *  serverul foloseste operatii avansate de intrare/ieșire:
  	- operatii asincrone pe fișiere
	- operatii non-blocante pe socketi
	- mecanismul zero-copying (folosind sendfile)
	- multiplexarea operațiilor I/O (cu ajutorul API-ului epoll)
  * clientii si serverul comunica folosind protocolul HTTP
  * serverul e implementat ca o masina de stari pentru fiecare conexiune,
  	 ce este interogata periodic pentru actualizarea transferului
  * portul pe care serverul web asculta pentru conexiuni este 8888

## Continut

Proiectul este constituit din cateva fisiere, care asigura functionarea
serverului:

* http-parser/* - API pentru parsarea requesturilor HTTP.
* utils/w_epoll.h - API pentru multiplexarea operațiilor I/O folosind epoll.
* utils/sock_util.h - Functii folosite pentru manipularea socketilor.
* utils/debug.h & utils/util.h- Macro-uri pentru debug.
* aws.h - Macro-uri folosite in cadrul serverului.
* aws.c - Implementarea efectiva a serverului.

Am implementat doar transmiterea fisierelor statice.

## Cum functioneaza?

In primul rand, este creat socketul de listen al serverului si adaugat la
epoll. Apoi, serverul asteapta primirea unui eveniment nou. Acesta poate fi
un eveniment de creare a unei noi conexiuni, un eveniment de input sau unul
de output. 

In cazul evenimentului de input, dupa ce primesc inputul, il parsez cu
ajutorul parserului HTTP si detremin daca este un fisier static. Incerc sa
deschid fisierul din request path-ul determinat de parser. In caz ca acesta
nu exista, setez raspunsul ca fiind o eroare de tip 404. Altfel, daca fisierul
a fost deschis corect, raspunsul va avea statusul 200.

In cazul semnalarii unui eveniment de output, incep prin a trimite bufferul
rezultat pe socket. Fiind vorba doar de fisiere statice, acestea sunt transmise
prin mecanismul zero-copy care copiaza datele direct in kernel-space, fara a le
mai copia in user space. In final daca fisierul a fost trimis in intregime,
incheiem conexiunea.

## Cum se ruleaza programul?
Pentru a construi programul, trebuie rulata comanda 'make' din folderul 'util'.

Pentru a testa biblioteca folosind testele prezente in fisierul '_test', trebuie
rulat 'make -f Makefile.checker' din folderul 'checker-lin'.

## Feedback

Tema este 95% cod din sample-urile epoll_echo_server.c, http_reply_once.c si
test_get_request_path.c. M-am inspirat si din fisierul epoll_server.c din 
arhiva vechiului curs 11. Am combinat toate aceste fisiere in aws.c, adaugand
si folosirea sendfile. Nu am implementat partea cu fisiere dinamice. Desi am
incercat, m-am incurcat prea tare asa ca am renuntat si am ramas doar cu cele
statice. 

Mi se pare ca tema are o legatura mult prea minimala cu ce am facut la laborator.
Uitandu-ma la laboratorul 11 din anii trecuti, foarte multe exemple si exercitii
au legatura si ajuta mult la tema aceasta.

## Resurse
* https://ocw.cs.pub.ro/courses/so/teme/tema-5
* https://ocw.cs.pub.ro/courses/so/cursuri/curs-10
* https://ocw.cs.pub.ro/courses/so/cursuri/curs-11
* https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-11

* https://man7.org/linux/man-pages/man2/sendfile.2.html
* https://man7.org/linux/man-pages/man7/epoll.7.html
* https://linux.die.net/man/1/wget
* https://linux.die.net/man/1/nc
* https://pubs.opengroup.org/onlinepubs/009696699/functions/fstat.html
* https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods
* https://developer.mozilla.org/en-US/docs/Web/HTTP/Status