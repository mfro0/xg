WARN    = -Wall -Wpointer-arith -Wwrite-strings -Wmissing-prototypes -Wshadow \
	  -Werror #-Wcast-qual
COPTS	= -D_TRACE -g -O2 -fstrength-reduce #-DNODEBUG
CFLAGS  = -funsigned-char -fomit-frame-pointer \
	  -I. -I../include -I/usr/GEM/include \
	  $(COPTS) $(WARN)

#SHELL  = /bin/ash
#TMPDIR = /ram/tmp

CC  = m68k-atari-mint-gcc
CXX = m68k-atari-minit-g++
LD  = m68k-atari-mint-gcc -g
AR  = m68k-atari-mint-ar rs

PRG    = X.app
OBJDIR = ../obj
LIBS   = gem iio
DEPEND = $(OBJDIR)/depends

LDFLAGS = -v $(COPTS) -L/usr/GEM/lib


$(OBJDIR)/%.o: %.c ; rm -f $(PRG) ; $(CC) $(CFLAGS) -D_$(<:.c=)_ -c $< -o $@


ALL: $(PRG)

SRC = Atom.c Cursor.c Pointer.c Property.c Prop_ICCC.c Request.c \
      clnt.c clnt_swap.c colormap.c drawable.c draw_img.c \
      event.c font.c font_init.c fontable.c gcontext.c grph.c keyboard.c \
      main.c pixmap.c pixm_mono.c selection.c server.c \
      window.c wind_draw.c wind_grab.c wind_pntr.c wind_save.c wind_util.c \
      wmgr.c wmgr_draw.c wmgr_prog.c x_gem.c x_printf.c xrsc.c

OBJ = $(SRC:%.c=$(OBJDIR)/%.o)


$(PRG): $(OBJDIR) $(OBJ) Makefile config.c ../include/version
	@expr "0`cat -s $(OBJDIR)/build`" + 1 > $(OBJDIR)/build
	@$(CC) $(CFLAGS) -c config.c \
	       -DVERSION=\"`cat -s ../include/version`\" \
	       -DBUILD=\"`cat -s $(OBJDIR)/build`\" \
	       -o $(OBJDIR)/config.o
	$(LD) $(LDFLAGS) -o $@ $(OBJ) $(OBJDIR)/config.o $(LIBS:%=-l%)
	stack -S 128k $@


$(OBJDIR):
	@echo "*** Prepare obj-Directory ***"
	@mkdir $(OBJDIR)
	@echo "0" > $(OBJDIR)/build
	@echo "Create dependencies ..."
	@$(MAKE) $(DPND)
	@echo "*** Make $(PRG) ***"


clean: ; rm -f *.% *.[bB][aA][kK] ../include/*.%


#_______________________________________________________________________________
DPND = $(SRC:.c=.d)
depend: __udpnd Makefile $(DPND)
	@echo

__udpnd: ; rm -f $(DEPEND)

$(DPND):
	@echo -n " <$(@:%.d=%)> "
	@$(CC) -pipe $(CFLAGS: $(WARN)=) -M $(@:%.d=%.c) -D_$(@:.d=)_ \
	 | sed "s,^\(.*\)\.o:,$(OBJDIR)/\1.o:," >> $(DEPEND)

-include $(OBJDIR)/depends
