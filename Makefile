build:
	gcc ./myinit.c -lm -o myinit

run:
	nohup ./myinit -p /home/mkrvmark/hw/unix/3/programs.txt &