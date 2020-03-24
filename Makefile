PROGS = cp ln ls mkdir rm

# Creates all ext2 commands
all : $(PROGS)

$(PROGS) : % : ext2_%.c ext2.h
	gcc -Wall -g -c $<

%.o : %.c ext2.h
	gcc -Wall -g -c $<

# Create backup of images
backup :
	cp -r images .backup

# Restore images from backup
restore :
	cp -r .backup images

# Clean up compiled stuff
clean : 
	rm -f *.o

# Really cleanup repo
purge :
	rm -rf *.o .backup