PROGS = ext2_cp ext2_ln ext2_ls ext2_mkdir ext2_rm

# Creates all ext2 commands
all : $(PROGS)

$(PROGS) : % : %.c ext2.h ext2_welp.h
	gcc -Wall -g -o $@ $<

# Create backup of images
backup :
	cp -r images .backup

# Restore images from backup
restore :
	cp -r .backup images

# Clean up compiled stuff
clean : 
	rm -f $(PROGS)

# Really cleanup repo
purge :
	rm -rf $(PROGS) .backup