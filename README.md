# IOS-project2
Semaphores

Process synchronization (Post Office)
Project is inspired by book from Allen B. Downey: The Little Book of Semaphores (The barbershop
problem

## Usage

$ ./proj2 N_CUS N_OFF T_CUS T_OFF F

N_CUS - Number of customers
N_OFF - Number of officers
T_CUS - The maximum time in milliseconds that a customer waits after its creation before entering the
mail (eventually leaves with not received). 0<=T_CUS<=10000
T_OFF - Maximum length of a officer break in milliseconds. 0<=T_OFF<=100
F - Maximum time in milliseconds after which mail is closed for new arrivals.
0<=F<=10000

## License

This project is licensed under the [MIT License](LICENSE).

## Credits

- Daniel Sehnoutek (@danoboss1): Creator
