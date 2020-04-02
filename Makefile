PROGS = ext2_cp ext2_ln ext2_ls ext2_mkdir ext2_rm ext2_rm_bonus
FILES = ext2_cp.c ext2_ln.c ext2_ls.c ext2_mkdir.c ext2_rm.c ext2_rm_bonus.c

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
	rm -rf $(PROGS) images a3.tar.gz

# For submissions
compile :
	tar -czvf a3.tar.gz $(FILES) ext2.h ext2_welp.h INFO.txt Makefile
