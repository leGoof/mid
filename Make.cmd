$(TARG): $(OFILES)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIB)

$(OFILES): $(HFILES)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

.PHONY: clean install

clean:
	rm -f *.o $(TARG)

install: $(TARG)
	cp $(TARG) /usr/local/bin
