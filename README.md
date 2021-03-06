# RPLidar A3

This work has begun from https://github.com/Club-INTech/rplidar_a2.

Adaptation of the library to RPLidar A3. Baudrate can be 154200 and 256000.

## Measures

TCP server is on port 17685

Connecting starts the RPLidar and sends data continuously in ASCII.

Each line is one full turn, meaning each turn is separated with a '\n'

The data has this form : d1:a1;d2:a2; ... ;dn:an\nd1:a1;d2:a2; ... ;dn:an\n ...

dn is the distance in mm in float for the Nth point
an is the angle in degrees in float for the Nth point

N might vary

## Compilation

On a Debian-like system:

```bash
$ git clone https://github.com/clemsciences/rplidar_a3.git
$ cd rplidar_a3
$ sudo apt-get install libnet1-dev
$ cmake CMakeLists.txt
$ make 
```
