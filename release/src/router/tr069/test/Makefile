CFLAGS = -Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections
LDFLAGS = -s $(EXTRALDFLAGS) -ffunction-sections -fdata-sections -Wl,--gc-sections

all: udpclient sendtocli notify

udpclient: udpclient.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o udpclient udpclient.c
sendtocli: sendtocli.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o sendtocli sendtocli.c
notify: notify.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o notify notify.c
	
clean:
	@rm -f udpclient sendtocli notify
