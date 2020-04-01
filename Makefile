PROGS = ext2_cp ext2_ln ext2_ls ext2_mkdir ext2_rm

# Creates all ext2 commands
all : $(PROGS)

$(PROGS) : % : %.c ext2.h ext2_welp.h
	gcc -Wall -g -o $@ $<

# Restore images from backup
restore : images
	cp -r .backup/* images

images :
	mkdir images

# Clean up compiled stuff
clean : 
	rm -f $(PROGS)

# Really cleanup repo
purge :
	rm -rf $(PROGS) .backup