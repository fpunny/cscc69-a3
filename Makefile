PROGS = ext2_cp ext2_ln ext2_ls ext2_mkdir ext2_rm ext2_rm_bonus

# Creates all ext2 commands
all : $(PROGS)

$(PROGS) : % : %.c ext2.h ext2_welp.h
	gcc -Wall -g -o $@ $<

# Restore images from backup
restore :
	cp -r .backup/* images/

# Clean up compiled stuff
clean : 
	rm -f $(PROGS)

# Really cleanup repo
purge :
	rm -rf $(PROGS) .backup
