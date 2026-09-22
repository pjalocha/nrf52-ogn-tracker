all:        ttn-reg

ttn-reg:    ttn-reg.cc
	g++ -std=c++11 -Wall -O2 -o ttn-reg -I../src/ ttn-reg.cc -ljpeg -ldl

clean:
	rm -f ttn-reg
