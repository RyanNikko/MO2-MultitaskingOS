# MO2-MultitaskingOS

Group 2:

- Apa, Giusippi Maria II
- Geralde, Klyde Audre G.
- Kho, John Zechariah S.
- Taino, Ryan Nicholas A.

Entry Class File: main.cpp (Located in MCO2_Corrected Directory)

How to Run Program

1.) Click Run/Debug on IDE
2.) Enter 'initialize' command
3.) Enter a command from the Command List:
exit: Ends the program
- screen -s <name of process>: Creates a process with an associated name. Peforms screen -r command with the name of the process as the parameter, effectively transferring you to a new screen. *Warning: Do not put an empty string.*
- screen -r <name of process>: Go to a specific process with new screen. The new screen's text history and the main
console text history are saved.
- initialize: Can only be used at the start of the program to read the config txt once.
- marquee: Opens the marquee console.
- report-util: screen-ls but appends (or create a txt file and put there) the output into the text file.
- screen-ls: Outputs the current processes status as well as the CPU that currently works for them.
- scheduler-test: Initiates the creation of processes.
- scheduler-stop: Stops the creation of processes.
- process-smi: provides a summarized view of the available/used memory, as well as the list of processes and memory occupied.
- vmstat: provides a detailed view of the active/inactive processes, available/used memory, and pages.
