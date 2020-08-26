## FunkGBM (beta)

Ein Gleisbesetztmelder, der ins C-Gleis eingebaut werden kann und den Besetztzustand
ohne Kabel an die Zentrale meldet - über einen Gateway und per TCP.

# die Hardware im Gleis besteht aus:
- ATmega328p Prozessor (Arduino)
- Strommessung als Spannungsabfall an zwei Schottky-Dioden
- RFM69 Modul zur Datenübertragung (868MHz)
- Flash-Memory zur OTA (over-the-air) Softwareaktualisierung
(basiert auf dem "Moteino" von LowPowerLab)

# der "Gateway":
- Empfang der 868MHz Pakete vom Gleis
- Umkodierung in LocoNet-Datenpakete oder Selectrix-Sensor Messages (im Moment als Processing-Sketch realisiert, könnte aber auch per Hardware implementiert werden mit direktem Anschluss an das LocoNet oder den SX-Bus)
- die Sensor Informationen werden per TCP an einen "SX-Server" oder "LbServer" (LN/Fremo) gesendet

#Vorteile:
* keine Kabel
* Sw-Updates über Funk möglich

#Nachteile:
* vergleichsweise teuer


<img src="funk-gbm-proto-kl.jpg">Funk_GBM Prototyp im C-Gleis</img>

<img src="funk-gbm-sw-kl.jpg">Screenshot Raspi mit LbServer, Gleisbildstellpult und Gateway</img>

(C) Michael Blank 2020

siehe auch <a href="http://opensx.net/funkgbm">Funk-GBM auf opensx.net</a>
