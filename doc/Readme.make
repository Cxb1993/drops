=============
  Makefile:
=============

-----------------------------
  Hinweise fuer DROPS-User:
-----------------------------

Zunaechst muss in der Datei drops.conf im DROPS-Rootverzeichnis die verwendete
Rechnerarchitektur eingetragen werden (z.B. LINUX). Die Compilereinstellungen
erfolgen dann in der Datei arch/<Architekur>/mk.conf .

Im DROPS-Rootverzeichnis befindet sich das top-level-Makefile. Zu dessen
Benutzung muss GNU Make installiert sein. Mit "make <rule>" bzw. "gmake <rule>
wird die entsprechende Regel ausgefuehrt, wobei <rule> fuer eine der folgenden
Regeln steht:

    dep        erzeugt automatisch die Abhaengigkeiten und legt ein
               entsprechendes dependency-file an.

    all        erzeugt alle ausfuehrbaren Programme in DROPS.

    doc        legt eine html-Dokumentation an (mit doxygen).

    stat       listet eine Statistik aller Dateien auf.

    clean      loescht alle Objektdateien sowie alle ausfuehrbaren Dateien.

    distclean  wie "clean", loescht zusaetzlich alle Dateien mit Endungen
               .off, .dat sowie geom/topo.cpp, das dependency-file und
               die Dokumentation.

Nach einem CVS checkout muessen zunaechst die Abhaengigkeiten mit "make dep"
erzeugt werden. Erst dann kann das eigentliche Compilieren beginnnen.

In den jeweiligen Unterverzeichnissen befinden sich die lokalen Makefiles.
Diese verstehen als Regeln
  - "all", "clean", "distclean", die im jeweiligen Verzeichnis wirken
  - "dep", "doc" rufen die entsprechende Regel des top-level-Makefiles auf
  - sowie die jeweiligen Namen der executables und Objektdateien in diesem
    Verzeichnis


----------------------------------------------
  Hinweise fuer die parallele Nutzung von DROPS
----------------------------------------------

Die parallele Version von DROPS kann mit dem cvs Flag "-rmpi_branch" aus-
gecheckt werden. Zudem werden die Bibliotheken DDD und ParMetis benoetigt,
um das Programm parallel laufen zu lassen. Diese sind auch in dem CVS-Repository
vorhanden (Modulnamen: DDD und ParMetis).

Die aktuellste Version der parallelen DROPS-Implementierung kann man im SVN-
Repository
    https://pubvm10.rz.rwth-aachen.de//repos/Project_parDROPS/trunk
finden.

Die Kompiler Konfiguration erfolgt wie im seriellen Fall ueber die Datei
arch/<Architekur>/mk.conf, wobei als Architektur LINUX_MPI, SOLARIS_MPI oder
INTEL_MPI angegeben werden kann. Fuer eine lauffaehige parallele Version von
DROPS werden die beiden Bibliotheken DDD und ParMETIS ben�tigt, welche auch
im CVS- bzw. SVN-Repository liegen. Standardmaessig werden die Ordner DDD und
ParMetis im Verzeichnis $(DROPS_ROOT)/.. vermutet.

Um die parallele Version von DROPS zu kompilieren, muss man wie folgt vorgehen:

- Erstellen der Library DDD:      make DDD
- Erstellen der Library ParMetis: make ParMetis
- Erstellen der Abhaengigkeiten:  make dep

Danach lassen sich die parallelen Programme, die im Verzeichnis "./partests"
liegen, kompilieren und ausfuehren.

---------------------------------------
  Hinweise fuer die DROPS-Entwickler:
---------------------------------------

Damit die automatische Generierung der Abhaengigkeiten funktioniert, muessen
im Quelltext alle eingebundenen DROPS-Header-Files *immer* mit Pfadangabe
versehen sein (auch wenn das Header-File im selben Verzeichnis steht!).
Also muss z.B. in geom/boundary.cpp stehen:
#include "geom/boundary.h"    statt    #include "boundary.h"

Wenn sich die Abhaengigkeiten geaendert haben, koennen diese automatisch
mit Hilfe des top-level-Makefiles neu erzeugt werden: Dazu muss im
DROPS-Rootverzeichnis der Befehl
    make dep
ausgefuehrt werden.

Wenn neue executables hinzugekommen sind, muessen diese im jeweiligen lokalen
Makefile eingetragen werden, indem sie der Variablen EXEC hinzugefuegt werden
und eine neue Regel zum Linken des executable angelegt wird.

Kleiner Makefile-Chrash-Kurs:
------------------------------
Eine Regel sieht so aus:

    target: depend1 depend2 ...
       <Tab>   command

target und depends koennen auch Patterns enthalten; das Zeichen % steht dabei
als Platzhalter.
Weitere nuetzliche Anwendung von Patterns: Ersetzung, z.B.
    FILES = xxx.c yyy.c zzz.c
    OBJ   = $(FILES:%.c=%.o)     # -> xxx.o yyy.o zzz.o

Automatische Variablen, die in command verwendet werden koennen:

$@ = target
$< = erste Abhaengigkeit depend1
$^ = alle Abhaengigkeiten
$* = target ohne Suffix, bzw. = % bei Patterns

ACHTUNG: Automatische Variablen koennen nicht in conditionals
    ifeq "..." "..."
verwendet werden!

Besteht der Wert einer automatischen Variable aus Verzeichnis und Dateiname,
so kann mit $(@D) bzw. $(@F) der directory- bzw. file-Anteil zurueckgegeben
werden (in diesem Beispiel fuer die automatische Variable $@).

Referenzen:
- How to write a Makefile:  http://vertigo.hsrl.rutgers.edu/ug/make_help.html
- GNU Make:                 http://www.gnu.org/manual/make/index.html

