
default: exe.ihex exe.elf

# Upload:
u: exe.ihex
	sudo avrdude -p atmega328p -c arduino -P /dev/ttyUSB0 -b 115200 -U flash:w:exe.ihex:i


exe.ihex: exe.elf
	avr-objcopy -O ihex -R .eeprom exe.elf exe.ihex


exe.elf: main.c lcd.h
	avr-gcc -mmcu=atmega328p -O3 main.c -o exe.elf


asm: main.c lcd.h
	avr-gcc -mmcu=atmega328p -S -O3 main.c -o main.asm


clean:
	rm *.hex *.ihex *.o *.asm *.elf
