-std=gnu99
objects = echo-driver.o device-controller-simulator.o
echo-driver: $(objects)
	cc -o echo-driver $(objects)
echo-driver.o: echo-driver.c device-controller.h
	cc -c -g echo-driver.c
device-controller-simulator.o : device-controller-simulator.c device-controller.h
	cc -c device-controller-simulator.c
clean:
	rm echo-driver $(objects)

