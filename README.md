# Master Control Program

A multi-threaded software solution for a banking system to handle hundreds of thousands of user transactions with a guarantee of safe data synchronization while running in parallel

## Repo Organization
---
## part1

A directory containing a single threaded solution, which serves as the scaffolding for the system. Reads in an input file, processes the different types of transactions, and updates account balances

## part2

A directory containing a multi-threaded solution, which evenly distributes the transactions amongst worker threads running in parallel. Mutex locks are introduced to handle protecting the critical sections. Implements a basic banker thread to be improved upon in part3

## part3

A directory containing a multi-threaded solution with inter-process communication, which uses signaling to communicate amongst different threads. The banker thread now updates account balances regularly. When the worker threads hit a specified threshold of processed transactions, the banker thread increases user account balances based on a reward rate (ex: promotional deal, specifics in codebase comments)

## User Instructions
---
* Save the master-control-program repo locally
* From the command line, navigate to one of the directories ```part1```, ```part2```, or ```part3```
* Compile the corresponding C files with command ```make```
* Clean a directory of executables and object files with command ```make clean```
* Run a program with command ```./bank input-1.txt```
* The program creates a file ```output.txt``` which contains the resulting end balances of all accounts
* The program creates a directory ```output``` which contains files for each account, showing their updates after each processed transaction

## Contact Info
---
[LinkedIn](https://www.linkedin.com/in/jtkashuba)

kashuba.jt@gmail.com